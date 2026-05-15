#include "AnalysisManager.h"
#include "TrackDatabase.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>

AnalysisManager::AnalysisManager(TrackDatabase &db)
    : Thread("AnalysisThread"), database(db) {
  formatManager.registerBasicFormats();
  startThread(juce::Thread::Priority::background);
}

AnalysisManager::~AnalysisManager() { stopAnalysis(); }

void AnalysisManager::queueTrack(const juce::File &file) {
  juce::ScopedLock sl(queueLock);
  // Avoid duplicates
  if (queue.contains(file)) return;
  queue.add(file);
  waitEvent.signal();
}

void AnalysisManager::stopAnalysis() {
  shouldStop = true;
  waitEvent.signal();
  stopThread(2000);
}

void AnalysisManager::run() {
  while (!threadShouldExit() && !shouldStop) {
    try {
      analyzeNext();
    } catch (...) {
    }
    waitEvent.wait(2000);
  }
}

void AnalysisManager::analyzeNext() {
  juce::File file;
  { juce::ScopedLock sl(queueLock); if (queue.isEmpty()) return; file = queue[0]; queue.remove(0); }

  { juce::ScopedLock sl(pathLock); currentAnalyzingPath = file.getFullPathName(); currentProgress = 0.0f; }

  TrackDatabase::Track t;
  if (database.getTrackByPath(file.getFullPathName(), t)) {
    TrackDatabase::Analysis a;
    if (database.loadAnalysis(t.id, a)) { juce::ScopedLock sl(pathLock); currentAnalyzingPath = ""; return; }
  }

  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
  if (!reader) { juce::ScopedLock sl(pathLock); currentAnalyzingPath = ""; return; }

  t.path = file.getFullPathName();
  t.name = file.getFileNameWithoutExtension();
  t.artist = "Unknown Artist";
  t.createdAt = juce::Time::getCurrentTime();
  t.lastPlayed = t.createdAt;

  t.bpm = detectBpm(reader.get());
  currentProgress = 0.1f;

  t.key = detectKey(reader.get());
  currentProgress = 0.2f;

  int trackId = database.addOrUpdateTrack(t);

  TrackDatabase::Analysis a;
  a.trackId = trackId;
  generateWaveformData(reader.get(), a.waveform);
  currentProgress = 0.4f;

  // Detect downbeat position (first strong beat after quantization)
  a.beatgrid = detectDownbeat(reader.get(), t.bpm);

  generateStems(reader.get(), trackId, file.getFullPathName(), a);
  currentProgress = 1.0f;

  database.saveAnalysis(a);

  if (onAnalysisFinished)
    juce::MessageManager::callAsync([this] { if (onAnalysisFinished) onAnalysisFinished(); });

  { juce::ScopedLock sl(pathLock); currentAnalyzingPath = ""; }
}

bool AnalysisManager::isAnalyzing(const juce::String &path) {
  juce::ScopedLock sl(pathLock);
  return currentAnalyzingPath == path;
}

// ── BPM detection via autocorrelation ─────────────────────────────────
double AnalysisManager::detectBpm(juce::AudioFormatReader *reader) {
  if (reader == nullptr) return 120.0;
  double sr = reader->sampleRate;
  if (sr < 1000.0 || reader->lengthInSamples < 1) return 120.0;
  int totalSamples = (int)reader->lengthInSamples;

  int analysisLen = juce::jmin(totalSamples, (int)(sr * 10.0));
  if (analysisLen < (int)(sr * 1.0)) return 120.0;
  int decim = (int)(sr / 2000.0);
  if (decim < 1) decim = 1;
  int dsLen = analysisLen / decim;
  if (dsLen < 100) return 120.0;
  double dsSr = sr / decim;

  int rawCh = reader->numChannels;
  if (rawCh < 1 || rawCh > 2) rawCh = 2;

  juce::AudioBuffer<float> raw(rawCh, analysisLen);
  raw.clear();
  reader->read(&raw, 0, analysisLen, 0, true, true);

  juce::AudioBuffer<float> buf(1, dsLen);
  buf.clear();
  for (int i = 0; i < dsLen; ++i) {
    float sum = 0.0f;
    for (int c = 0; c < rawCh; ++c)
      sum += raw.getSample(c, i * decim);
    buf.setSample(0, i, sum / rawCh);
  }

  const float* audio = buf.getReadPointer(0);

  std::vector<float> env(static_cast<size_t>(dsLen), 0.0f);
  for (int i = 1; i < dsLen; ++i) {
    float diff = audio[i] - audio[i - 1];
    env[static_cast<size_t>(i)] = diff > 0.0f ? diff : 0.0f;
  }

  int minLag = (int)(60.0 * dsSr / 200.0);
  int maxLag = (int)(60.0 * dsSr / 60.0);
  if (maxLag >= dsLen / 2) maxLag = dsLen / 2 - 1;
  if (minLag < 1) minLag = 1;
  if (minLag >= maxLag) return 120.0;

  int step = (maxLag - minLag > 1000) ? 4 : 1;
  int range = (maxLag - minLag + step - 1) / step;
  if (range < 3) return 120.0;

  std::vector<float> acf(static_cast<size_t>(range), 0.0f);
  for (int lag = minLag; lag < maxLag; lag += step) {
    float sum = 0.0f;
    int n = dsLen - lag;
    const float* e = env.data();
    for (int i = 0; i < n; ++i)
      sum += e[i] * e[i + lag];
    size_t idx = static_cast<size_t>((lag - minLag) / step);
    if (idx < static_cast<size_t>(range))
      acf[idx] = sum / (float)n;
  }

  float maxAcf = 0.0f;
  for (auto& v : acf) if (v > maxAcf) maxAcf = v;
  if (maxAcf < 1e-6f) return 120.0;
  for (auto& v : acf) v /= maxAcf;

  int bestLagIdx = minLag;
  float bestVal = 0.0f;
  for (int i = 2; i < range - 1; ++i) {
    size_t idx = static_cast<size_t>(i);
    if (acf[idx] > acf[idx - 1] && acf[idx] >= acf[idx + 1] && acf[idx] > 0.3f) {
      if (acf[idx] > bestVal) {
        bestVal = acf[idx];
        bestLagIdx = minLag + i * step;
      }
    }
  }

  double bpm = 60.0 * dsSr / bestLagIdx;
  return juce::jlimit(60.0, 200.0, std::round(bpm * 10.0) / 10.0);
}

// ── Key detection via chroma features ─────────────────────────────────
juce::String AnalysisManager::detectKey(juce::AudioFormatReader *reader) {
  if (reader == nullptr || reader->sampleRate < 1000.0 || reader->lengthInSamples < 1) return "?";
  double sr = reader->sampleRate;
  int totalSamples = (int)reader->lengthInSamples;
  int analysisLen = juce::jmin(totalSamples, (int)(sr * 8.0));
  if (analysisLen < (int)(sr * 0.5)) return "?";

  int fftSize = 4096;
  int hop = 2048;
  int numFrames = (analysisLen - fftSize) / hop + 1;
  if (numFrames < 1) numFrames = 1;

  // Mono mix
  juce::AudioBuffer<float> buf(1, analysisLen);
  buf.clear();
  int rawCh = reader->numChannels;
  if (rawCh > 2) rawCh = 2;
  if (rawCh < 1) return "?";
  juce::AudioBuffer<float> raw(rawCh, analysisLen);
  raw.clear();
  reader->read(&raw, 0, analysisLen, 0, true, true);
  for (int i = 0; i < analysisLen; ++i) {
    float sum = 0.0f;
    for (int c = 0; c < rawCh; ++c) sum += raw.getSample(c, i);
    buf.setSample(0, i, sum / rawCh);
  }
  const float* audio = buf.getReadPointer(0);

  juce::dsp::FFT fft(12);
  std::vector<float> chroma(12, 0.0f);
  int frameCount = 0;

  std::vector<float> window(static_cast<size_t>(fftSize));
  for (int i = 0; i < fftSize; ++i)
    window[static_cast<size_t>(i)] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));

  // Build chroma bin mapping
  std::array<std::vector<int>, 12> chromaBins;
  double a4 = 440.0;
  for (int semi = 0; semi < 128; ++semi) {
    double freq = a4 * std::pow(2.0, (semi - 69) / 12.0);
    int bin = (int)std::round(freq * fftSize / sr);
    if (bin > 0 && bin < fftSize / 2)
      chromaBins[static_cast<size_t>(semi % 12)].push_back(bin);
  }

  std::vector<std::complex<float>> fftBuf(static_cast<size_t>(fftSize));
  for (int f = 0; f < numFrames; ++f) {
    int start = f * hop;
    for (int i = 0; i < fftSize; ++i) {
      int idx = start + i;
      fftBuf[static_cast<size_t>(i)] = (idx < analysisLen)
        ? std::complex<float>(audio[idx] * window[static_cast<size_t>(i)], 0.0f)
        : std::complex<float>(0.0f, 0.0f);
    }
    fft.perform(fftBuf.data(), fftBuf.data(), false);

    std::vector<float> mag(static_cast<size_t>(fftSize / 2), 0.0f);
    for (int b = 0; b < fftSize / 2; ++b)
      mag[static_cast<size_t>(b)] = std::abs(fftBuf[static_cast<size_t>(b)]);

    std::vector<float> frameChroma(12, 0.0f);
    for (int pc = 0; pc < 12; ++pc) {
      float sum = 0.0f;
      for (int bin : chromaBins[static_cast<size_t>(pc)])
        if (bin < (int)mag.size()) sum += mag[static_cast<size_t>(bin)];
      frameChroma[static_cast<size_t>(pc)] = sum;
    }

    float norm = 0.0f;
    for (auto& v : frameChroma) norm += v;
    if (norm > 1e-6f)
      for (auto& v : frameChroma) v /= norm;

    for (int pc = 0; pc < 12; ++pc)
      chroma[static_cast<size_t>(pc)] += frameChroma[static_cast<size_t>(pc)];
    frameCount++;
  }

  if (frameCount > 0)
    for (auto& v : chroma) v /= (float)frameCount;

  // Krumhansl-Schmuckler key profiles
  const float kspMajor[12][12] = {
    {6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88},
    {2.88,6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29},
    {2.29,2.88,6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66},
    {3.66,2.29,2.88,6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39},
    {2.39,3.66,2.29,2.88,6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19},
    {5.19,2.39,3.66,2.29,2.88,6.35,2.23,3.48,2.33,4.38,4.09,2.52},
    {2.52,5.19,2.39,3.66,2.29,2.88,6.35,2.23,3.48,2.33,4.38,4.09},
    {4.09,2.52,5.19,2.39,3.66,2.29,2.88,6.35,2.23,3.48,2.33,4.38},
    {4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88,6.35,2.23,3.48,2.33},
    {2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88,6.35,2.23,3.48},
    {3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88,6.35,2.23},
    {2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88,6.35}
  };

  float bestCorr = -1e10f;
  int bestKey = 0;
  bool bestIsMajor = true;

  for (int k = 0; k < 12; ++k) {
    float corrMajor = 0.0f;
    for (int pc = 0; pc < 12; ++pc) {
      int idx = (pc + 12 - k) % 12;
      corrMajor += chroma[static_cast<size_t>(pc)] * kspMajor[k][idx];
    }

    float corrMinor = 0.0f;
    int minorIdx = (k + 3) % 12;
    for (int pc = 0; pc < 12; ++pc) {
      int idx = (pc + 12 - minorIdx) % 12;
      corrMinor += chroma[static_cast<size_t>(pc)] * kspMajor[minorIdx][idx];
    }

    if (corrMajor > bestCorr) { bestCorr = corrMajor; bestKey = k; bestIsMajor = true; }
    if (corrMinor > bestCorr) { bestCorr = corrMinor; bestKey = k; bestIsMajor = false; }
  }

  const char* majorKeys[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  const char* minorKeys[] = {"Am", "A#m", "Bm", "Cm", "C#m", "Dm", "D#m", "Em", "Fm", "F#m", "Gm", "G#m"};

  if (bestIsMajor) return juce::String(majorKeys[bestKey]);
  return juce::String(minorKeys[bestKey]);
}

void AnalysisManager::generateWaveformData(juce::AudioFormatReader *reader,
                                           juce::MemoryBlock &dest) {
  if (reader == nullptr) return;
  int numSamples = (int)reader->lengthInSamples;
  if (numSamples <= 0) return;

  int samplesPerPixel = numSamples / 1000;
  if (samplesPerPixel < 1) samplesPerPixel = 1;

  int numPixels = numSamples / samplesPerPixel;
  if (numPixels < 1) numPixels = 1;
  dest.setSize(numPixels * sizeof(float));
  float *out = (float *)dest.getData();

  juce::AudioBuffer<float> buffer(reader->numChannels, samplesPerPixel);
  for (int i = 0; i < numPixels; ++i) {
    reader->read(&buffer, 0, samplesPerPixel, (juce::int64)i * samplesPerPixel,
                 true, true);
    out[i] = buffer.getMagnitude(0, samplesPerPixel);
    if (threadShouldExit()) break;
  }
}

float AnalysisManager::getAnalysisProgress(const juce::String &path) {
  juce::ScopedLock sl(pathLock);
  if (juce::File(currentAnalyzingPath) == juce::File(path))
    return currentProgress;
  return -1.0f;
}

// ── Downbeat detection ─────────────────────────────────
juce::String AnalysisManager::detectDownbeat(juce::AudioFormatReader *reader, double bpm) {
  if (reader == nullptr || bpm < 20.0) return "{\"version\":1,\"markers\":[]}";
  double sr = reader->sampleRate;
  int totalSamples = (int)reader->lengthInSamples;
  if (totalSamples < (int)(sr * 1.0)) return "{\"version\":1,\"markers\":[]}";

  int analysisLen = juce::jmin(totalSamples, (int)(sr * 8.0));
  int beatSamples = (int)(sr * 60.0 / bpm);
  int barSamples = beatSamples * 4;
  int numBars = analysisLen / barSamples;
  if (numBars < 2) return "{\"version\":1,\"markers\":[]}";

  // Mono mix + downsample
  int decim = (int)(sr / 2000.0);
  if (decim < 1) decim = 1;
  int dsLen = analysisLen / decim;
  int dsBarSamples = barSamples / decim;

  int rawCh = reader->numChannels;
  if (rawCh > 2) rawCh = 2;
  juce::AudioBuffer<float> raw(rawCh, analysisLen);
  raw.clear();
  reader->read(&raw, 0, analysisLen, 0, true, true);

  std::vector<float> energy(static_cast<size_t>(numBars * 4), 0.0f);
  for (int bar = 0; bar < numBars; ++bar) {
    for (int beat = 0; beat < 4; ++beat) {
      float maxE = 0.0f;
      for (int s = 0; s < beatSamples; s += decim) {
        int idx = bar * barSamples + beat * beatSamples + s;
        if (idx < analysisLen) {
          float sum = 0.0f;
          for (int c = 0; c < rawCh; ++c) sum += std::abs(raw.getSample(c, idx));
          if (sum > maxE) maxE = sum;
        }
      }
      energy[static_cast<size_t>(bar * 4 + beat)] = maxE;
    }
  }

  // Find which beat position has the most energy across all bars
  std::vector<float> beatProfile(4, 0.0f);
  for (int bar = 0; bar < numBars; ++bar)
    for (int beat = 0; beat < 4; ++beat)
      beatProfile[static_cast<size_t>(beat)] += energy[static_cast<size_t>(bar * 4 + beat)];

  // The downbeat (beat 0) should have the strongest energy in most music
  // If another beat is consistently stronger, adjust the offset
  int maxBeat = 0;
  for (int b = 1; b < 4; ++b)
    if (beatProfile[static_cast<size_t>(b)] > beatProfile[static_cast<size_t>(maxBeat)])
      maxBeat = b;

  // firstDownbeatOffset = how many samples from start to first downbeat
  int firstDownbeatSample = maxBeat * beatSamples;
  double firstDownbeatSec = (double)firstDownbeatSample / sr;

  return "{\"version\":1,\"markers\":[{\"pos\":" + juce::String(firstDownbeatSec, 3) + ",\"type\":\"downbeat\"}]}";
}

void AnalysisManager::generateStems(juce::AudioFormatReader *reader,
                                    int trackId,
                                    const juce::String &originalPath,
                                    TrackDatabase::Analysis &analysis) {
  currentProgress = 1.0f;
}
