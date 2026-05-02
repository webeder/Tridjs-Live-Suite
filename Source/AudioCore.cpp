#include "AudioCore.h"
#include <cmath>

AudioCore::AudioCore()
{
    // Initialize ONNX Runtime
    try {
        ortEnv = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "TridjsDemucs");
        // We will load the session on first demand or if model exists
        onnxInitialized = true;
    } catch (...) {
        onnxInitialized = false;
    }

    // Registra formatos WAV, MP3, etc.
    formatManager.registerBasicFormats();

    // Setup FX Processors
    for (int i = 0; i < NUM_FX; ++i)
    {
        EffectSlot slot;
        fxProcessors.push_back(std::move(slot));
    }

    // Main Track Deck
    mainTrackChannel = std::make_unique<PlaybackChannel>();
    mainTrackChannel->transport = std::make_unique<juce::AudioTransportSource>();
    mainTrackChannel->resampler = std::make_unique<juce::ResamplingAudioSource>(mainTrackChannel->transport.get(), false, 2);
    mixer.addInputSource(mainTrackChannel->resampler.get(), false);

    // Cria 9 canais simulando os 9 Pads
    for (int i = 0; i < 9; ++i)
    {
        auto channel = std::make_unique<PlaybackChannel>();
        channel->transport = std::make_unique<juce::AudioTransportSource>();
        channel->resampler = std::make_unique<juce::ResamplingAudioSource>(channel->transport.get(), false, 2);
        mixer.addInputSource (channel->resampler.get(), false);
        padChannels.push_back (std::move (channel));
    }
    // Inicializa os ganhos dos stems como 1.0 (não mutado)
    for (int i = 0; i < NUM_STEMS; ++i)
        stemGains[i].setCurrentAndTargetValue(1.0f);
}

AudioCore::~AudioCore()
{
    mixer.removeAllInputs();
}

void AudioCore::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    mixer.prepareToPlay (samplesPerBlockExpected, sampleRate);
    
    // Initialize Delay Buffer (1 second capacity)
    delayBuffer.setSize(2, (int)sampleRate);
    delayBuffer.clear();
    writePos = 0;
    currentSampleRate = sampleRate;

    // Configura o smoothing para os ganhos dos stems (100ms de rampa)
    for (int i = 0; i < NUM_STEMS; ++i)
        stemGains[i].reset(sampleRate, 0.1);

    // XY Filter Setup
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlockExpected;
    spec.numChannels = 2;
    xyLpFilter.prepare(spec);
    xyLpFilter.setMode(juce::dsp::LadderFilterMode::LPF24);
    xyHpFilter.prepare(spec);
    xyHpFilter.setMode(juce::dsp::LadderFilterMode::HPF24);

    xyFlanger.prepare(spec);
    xyFlanger.setCentreDelay(7.0f);
    xyFlanger.setFeedback(0.4f);
    xyFlanger.setDepth(0.5f);
    
    xyEcho.prepare(spec);
    xyEcho.setMaximumDelayInSamples((int)(sampleRate * 2.0));
    xyEcho.setDelay((float)(sampleRate * 0.4)); // 400ms fixo
}

void AudioCore::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* mainBuffer = bufferToFill.buffer;
    int start = bufferToFill.startSample;
    int num = bufferToFill.numSamples;

    // Capture Recording from Microphone (INPUT data must be captured BEFORE mixer starts writing to buffer)
    float inLevel = mainBuffer->getMagnitude(start, num);
    lastInputLevel.store(inLevel);

    if (padRecorder.isRecording.load()) {
        const juce::ScopedLock sl (padRecorder.lock);
        if (padRecorder.writer != nullptr) {
            // Se o usuário tem uma placa multicanal (ex: 8 canais), o mic pode estar no canal 3, 4, etc.
            // Vamos somar todos os inputs disponíveis nos canais 0 e 1 do arquivo para garantir que gravamos o som.
            juce::AudioBuffer<float> tempRecordBuffer(2, num);
            tempRecordBuffer.clear();
            
            int numIn = mainBuffer->getNumChannels();
            for (int ch = 0; ch < numIn; ++ch) {
                tempRecordBuffer.addFrom(ch % 2, 0, *mainBuffer, ch, start, num);
            }
            
            padRecorder.writer->writeFromAudioSampleBuffer(tempRecordBuffer, 0, num);
        }
    }

    // 1. Mixer Original (Sempre som limpo por padrão)
    mixer.getNextAudioBlock (bufferToFill);

    // 2. Volume Master (Aplicado ao mix final: Track + Pads)
    mainBuffer->applyGain(start, num, masterVolume);

    // 3. Logica de Stems - Verifica se precisa chavear para o modo stems
    bool anyStemActive = false;
    for (int s = 0; s < NUM_STEMS; ++s) {
        if (stemGains[s].getNextValue() < 0.99f || stemGains[s].isSmoothing()) 
            anyStemActive = true;
    }

    if (stemsAreReady.load() && anyStemActive && isMainTrackPlaying())
    {
        auto posSec = getMainTrackPosition();
        auto startSample = (juce::int64)(posSec * 44100.0);

        for (int ch = 0; ch < mainBuffer->getNumChannels(); ++ch) {
            auto* out = mainBuffer->getWritePointer(ch, start);
            for (int i = 0; i < num; ++i) {
                juce::int64 idx = startSample + i;
                if (idx >= 0 && idx < vocalBuffer.getNumSamples()) {
                    float v = vocalBuffer.getSample(ch % vocalBuffer.getNumChannels(), (int)idx);
                    float d = drumsBuffer.getSample(ch % drumsBuffer.getNumChannels(), (int)idx);
                    float b = bassBuffer.getSample(ch % bassBuffer.getNumChannels(), (int)idx);
                    
                    // Soma dos Stems com Ganho Suave (Fase preservada pelo Demucs)
                    out[i] = (v * stemGains[0].getCurrentValue()) + 
                             (d * stemGains[1].getCurrentValue()) + 
                             (b * stemGains[2].getCurrentValue());
                }
            }
        }
        
        // Avança o smoothing para o próximo bloco
        for (int s = 0; s < NUM_STEMS; ++s) stemGains[s].skip(num);
    }

    // 4. Engine de Efeitos - Só processa se algum FX estiver ON e a música tocando
    bool anyFx = false;
    for (int f = 0; f < NUM_FX; ++f) if (fxProcessors[f].enabled) anyFx = true;

    if (anyFx)
    {
        float lfoInc = juce::MathConstants<float>::twoPi * (0.35f / (float)currentSampleRate);

        for (int ch = 0; ch < mainBuffer->getNumChannels(); ++ch) {
            auto* data = mainBuffer->getWritePointer(ch, start);
            auto* delayData = delayBuffer.getWritePointer(ch % delayBuffer.getNumChannels());
            int localWritePos = writePos;
            int chIdx = ch % 2;

            for (int i = 0; i < num; ++i) {
                float input = data[i];
                float output = input;
                lfoPhase += lfoInc;
                if (lfoPhase > juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;

                // FX 0: Delay
                if (fxProcessors[0].enabled) {
                    int delayLen = (int)(currentSampleRate * 0.4);
                    int rPos = (localWritePos - delayLen + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples();
                    float delaySample = delayData[rPos];
                    delayData[localWritePos] = juce::jlimit(-1.0f, 1.0f, input + delaySample * 0.45f * fxProcessors[0].amount);
                    output = juce::jlimit(-1.0f, 1.0f, output + delaySample * fxProcessors[0].amount);
                } else if (!fxProcessors[1].enabled && !fxProcessors[5].enabled) {
                    delayData[localWritePos] = input; // Basic buffer update
                }

                // FX 1: Echo (Longer Tap)
                if (fxProcessors[1].enabled) {
                    int echoLen = (int)(currentSampleRate * 0.65);
                    int rPos = (localWritePos - echoLen + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples();
                    output = juce::jlimit(-1.0f, 1.0f, output + delayData[rPos] * fxProcessors[1].amount * 0.7f);
                }

                // FX 2: Reverb (3-Tap Comb Feedback)
                if (fxProcessors[2].enabled) {
                    int d1 = (int)(currentSampleRate * 0.027);
                    int d2 = (int)(currentSampleRate * 0.035);
                    int d3 = (int)(currentSampleRate * 0.041);
                    float r1 = delayData[(localWritePos - d1 + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples()];
                    float r2 = delayData[(localWritePos - d2 + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples()];
                    float r3 = delayData[(localWritePos - d3 + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples()];
                    output = juce::jlimit(-1.0f, 1.0f, output + (r1 + r2 + r3) * 0.4f * fxProcessors[2].amount);
                }

                // FX 3: Flange (Modulated Delay)
                if (fxProcessors[3].enabled) {
                    float mod = (1.0f + std::sin(lfoPhase)) * 140.0f; 
                    int rPos = (localWritePos - (int)(120 + mod) + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples();
                    output = (output + delayData[rPos] * fxProcessors[3].amount) * 0.7f;
                }

                // FX 4: Space (Pseudo-Stereo Depth)
                if (fxProcessors[4].enabled) {
                    int sLen = (int)(currentSampleRate * (chIdx == 0 ? 0.005 : 0.015)); // Offset L/R
                    int rPos = (localWritePos - sLen + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples();
                    output = output * (1.0f - fxProcessors[4].amount * 0.5f) + delayData[rPos] * fxProcessors[4].amount * 0.5f;
                }

                // FX 5: Dub Echo (High Saturation)
                if (fxProcessors[5].enabled) {
                    int dLen = (int)(currentSampleRate * 0.38);
                    int rPos = (localWritePos - dLen + delayBuffer.getNumSamples()) % delayBuffer.getNumSamples();
                    float fb = delayData[rPos];
                    delayData[localWritePos] = std::tanh(input + fb * 0.78f * fxProcessors[5].amount);
                    output = juce::jlimit(-1.0f, 1.0f, output + fb * fxProcessors[5].amount);
                }

                data[i] = output;
                localWritePos = (localWritePos + 1) % delayBuffer.getNumSamples();
            }
            if (ch == mainBuffer->getNumChannels() - 1) writePos = localWritePos;
        }
    }

    // 5. Pico para VU meter
    float peak = 0.0f;
    for (int ch = 0; ch < mainBuffer->getNumChannels(); ++ch) {
        float mag = mainBuffer->getMagnitude(ch, start, num);
        if (mag > peak) peak = mag;
    }
    currentPeakLevel.store(peak);
    // 6. XY Filter (Mola Suite)
    if (xyEnabled.load())
    {
        juce::dsp::AudioBlock<float> block(*mainBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);

        if (xyMode == XyMode::Ladder)
        {
            xyLpFilter.process(context);
        }
        else
        {
            // X-Axis: Flanger (Left) ou Echo (Right)
            if (xyFlangerMix > 0.01f) {
                xyFlanger.setMix(xyFlangerMix * 0.7f);
                xyFlanger.process(context);
            } else if (xyEchoMix > 0.01f) {
                for (int ch = 0; ch < mainBuffer->getNumChannels(); ++ch) {
                    auto* samples = mainBuffer->getWritePointer(ch, start);
                    for (int i = 0; i < num; ++i) {
                        float delayed = xyEcho.popSample(ch);
                        xyEcho.pushSample(ch, samples[i] + delayed * 0.5f);
                        samples[i] += delayed * xyEchoMix * 0.6f;
                    }
                }
            }

            // Y-Axis: HPF (Up) ou LPF (Down)
            if (xyLpMix > 0.01f) {
                xyLpFilter.process(context);
            } else if (xyHpMix > 0.01f) {
                xyHpFilter.process(context);
            }
        }
    }

    // Capture Master Recording (Final output AFTER all processing)
    if (masterRecorder.isRecording.load()) {
        const juce::ScopedLock sl(masterRecorder.lock);
        if (masterRecorder.writer != nullptr) {
            masterRecorder.writer->writeFromAudioSampleBuffer(*mainBuffer, start, num);
        }
    }
}

void AudioCore::asyncExtractStems(const juce::File& file)
{
    stemsAreReady = false;
    stemProgress = 0.0f;

    std::thread([this, file]() {
        juce::AudioFormatManager manager;
        manager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(file));
        
        if (reader == nullptr) return;

        int numCh = reader->numChannels;
        int numSamples = (int)reader->lengthInSamples;

        vocalBuffer.setSize(numCh, numSamples);
        drumsBuffer.setSize(numCh, numSamples);
        bassBuffer.setSize(numCh, numSamples);

        // Temp buffer for processing
        juce::AudioBuffer<float> tempBuffer(numCh, numSamples);
        reader->read(&tempBuffer, 0, numSamples, 0, true, true);

        // Proof of concept extraction (High-Precision 4th Order Crossover)
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* src = tempBuffer.getReadPointer(ch);
            auto* vOut = vocalBuffer.getWritePointer(ch);
            auto* dOut = drumsBuffer.getWritePointer(ch);
            auto* bOut = bassBuffer.getWritePointer(ch);

            // Per-channel internal filter states
            float lp[4] = {0,0,0,0};
            float hp[4] = {0,0,0,0};

            for (int i = 0; i < numSamples; ++i)
            {
                float s = src[i];
                
                // 4th Order LP (Bass < 150Hz) - coefficient calibrated for 44.1k
                float lpIn = s;
                for(int p=0; p<4; ++p) {
                    lp[p] += 0.022f * (lpIn - lp[p]);
                    lpIn = lp[p];
                }
                float bass = lp[3];

                // 4th Order HP (Drums/High > 4000Hz)
                float hpIn = s;
                for(int p=0; p<4; ++p) {
                    hp[p] += 0.22f * (hpIn - hp[p]);
                    hpIn = hp[p];
                }
                float highs = s - hp[3];

                bOut[i] = bass;
                dOut[i] = highs;
                vOut[i] = s - bass - highs; // Pure Mid/Vocal residue
            }
        }

        stemsAreReady = true;
        stemProgress = 1.0f;
    }).detach();
}

void AudioCore::releaseResources()
{
    mixer.releaseResources();
}

// -------------------------------------------------------------
// CrossfadingLoopSource Implementation (5ms Anti-Click)
// -------------------------------------------------------------

AudioCore::CrossfadingLoopSource::CrossfadingLoopSource(juce::AudioFormatReader* reader, bool deleteReaderWhenDone) 
    : readerSource(std::make_unique<juce::AudioFormatReaderSource>(reader, deleteReaderWhenDone))
{
    readerSource->setLooping(false); // Vamos controlar o loop manualmente para aplicar o crossfade
}

AudioCore::CrossfadingLoopSource::~CrossfadingLoopSource() {}

void AudioCore::CrossfadingLoopSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    crossfadeSamples = static_cast<int>(sampleRate * 0.005); // 5 milissegundos
    readerSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void AudioCore::CrossfadingLoopSource::releaseResources() { readerSource->releaseResources(); }
void AudioCore::CrossfadingLoopSource::setNextReadPosition(juce::int64 newPosition) { readerSource->setNextReadPosition(newPosition); }
juce::int64 AudioCore::CrossfadingLoopSource::getNextReadPosition() const { return readerSource->getNextReadPosition(); }
juce::int64 AudioCore::CrossfadingLoopSource::getTotalLength() const { return readerSource->getTotalLength(); }
bool AudioCore::CrossfadingLoopSource::isLooping() const { return looping; }
void AudioCore::CrossfadingLoopSource::setLooping(bool shouldLoop) { looping = shouldLoop; }
void AudioCore::CrossfadingLoopSource::setLoopRange(juce::int64 start, juce::int64 length) { 
    loopStart = start; 
    loopLength = length; 
}

void AudioCore::CrossfadingLoopSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
    auto totalLen = readerSource->getTotalLength();
    if (totalLen == 0) return;

    // Se não estiver em modo loop de range, usa o comportamento padrão
    juce::int64 start = (looping && loopLength > 0) ? loopStart : 0;
    juce::int64 end = (looping && loopLength > 0) ? (loopStart + loopLength) : totalLen;
    end = juce::jmin(end, totalLen);

    if (!looping || (end - start) <= crossfadeSamples * 2) {
        readerSource->getNextAudioBlock(bufferToFill);
        if (looping && readerSource->getNextReadPosition() >= end)
            readerSource->setNextReadPosition(start);
        return;
    }

    int samplesLeft = bufferToFill.numSamples;
    int dstStart = bufferToFill.startSample;
    
    while (samplesLeft > 0)
    {
        auto pos = readerSource->getNextReadPosition();
        
        if (pos >= end - crossfadeSamples)
        {
            int samplesToProcess = juce::jmin(samplesLeft, (int)(end - pos));
            if (samplesToProcess <= 0) { readerSource->setNextReadPosition(start); continue; }

            juce::AudioBuffer<float> tailBuffer(bufferToFill.buffer->getNumChannels(), samplesToProcess);
            tailBuffer.clear();
            juce::AudioSourceChannelInfo tailInfo(&tailBuffer, 0, samplesToProcess);
            readerSource->getNextAudioBlock(tailInfo);
            
            juce::AudioBuffer<float> headBuffer(bufferToFill.buffer->getNumChannels(), samplesToProcess);
            headBuffer.clear();
            juce::AudioSourceChannelInfo headInfo(&headBuffer, 0, samplesToProcess);
            
            juce::int64 offset = pos - (end - crossfadeSamples);
            readerSource->setNextReadPosition(start + offset);
            readerSource->getNextAudioBlock(headInfo);
            
            readerSource->setNextReadPosition(start + offset + samplesToProcess);

            for (int chan = 0; chan < bufferToFill.buffer->getNumChannels(); ++chan)
            {
                auto* dst = bufferToFill.buffer->getWritePointer(chan, dstStart);
                auto* tailSrc = tailBuffer.getReadPointer(chan);
                auto* headSrc = headBuffer.getReadPointer(chan);
                for (int i = 0; i < samplesToProcess; ++i) {
                    float factor = (float)i / (float)samplesToProcess; 
                    dst[i] = tailSrc[i] * (1.0f - factor) + headSrc[i] * factor;
                }
            }
            dstStart += samplesToProcess;
            samplesLeft -= samplesToProcess;
        }
        else
        {
            int samplesToProcess = juce::jmin(samplesLeft, (int)((end - crossfadeSamples) - pos));
            if (samplesToProcess <= 0) { readerSource->setNextReadPosition(end - crossfadeSamples); continue; }

            juce::AudioSourceChannelInfo normalInfo(bufferToFill.buffer, dstStart, samplesToProcess);
            readerSource->getNextAudioBlock(normalInfo);
            dstStart += samplesToProcess;
            samplesLeft -= samplesToProcess;
        }
    }
}
// -------------------------------------------------------------

bool AudioCore::loadMainTrack (const juce::File& file, double bpm)
{
    if (!file.existsAsFile()) return false;
    auto* reader = formatManager.createReaderFor (file);
    if (reader != nullptr)
    {
        auto newSource = std::make_unique<CrossfadingLoopSource> (reader, true);
        mainTrackChannel->transport->setSource (newSource.get(), 0, nullptr, reader->sampleRate);
        mainTrackChannel->readerSource = std::move (newSource);
        
        if (bpm > 0.0)
            mainTrackBpm = bpm;
        else
            mainTrackBpm = detectBpm(file);

        thumbnail.setSource(new juce::FileInputSource(file));
        
        // Start background stem extraction
        asyncExtractStems(file);
        
        return true;
    }
    return false;
}

void AudioCore::playMainTrack()
{
    mainTrackChannel->transport->start();
}

void AudioCore::stopMainTrack()
{
    mainTrackChannel->transport->stop();
}

double AudioCore::getMainTrackPosition() const
{
    return mainTrackChannel->transport->getCurrentPosition();
}

double AudioCore::getMainTrackLength() const
{
    return mainTrackChannel->transport->getLengthInSeconds();
}

bool AudioCore::isMainTrackPlaying() const
{
    return mainTrackChannel->transport->isPlaying();
}

void AudioCore::seekMainTrack(double positionSeconds)
{
    mainTrackChannel->transport->setPosition(positionSeconds);
}

void AudioCore::ejectMainTrack()
{
    mainTrackChannel->transport->stop();
    mainTrackChannel->transport->setSource(nullptr);
    mainTrackChannel->readerSource.reset();
    thumbnail.clear();
    
    // Reset states to avoid hiss/glitches
    delayBuffer.clear();
    stemsAreReady = false;
    reverbState[0] = 0.0f;
    reverbState[1] = 0.0f;
    for(int s=0; s<NUM_STEMS; ++s) stemMuted[s] = false;
}

void AudioCore::setMainTrackLoopRange(double startTime, double duration) {
    if (mainTrackChannel && mainTrackChannel->readerSource) {
        if (startTime <= 0) startTime = mainTrackChannel->transport->getCurrentPosition();
        mainTrackChannel->readerSource->setLoopRange((juce::int64)(startTime * currentSampleRate), (juce::int64)(duration * currentSampleRate));
    }
}

void AudioCore::setMainTrackLoopEnabled(bool enabled) {
    if (mainTrackChannel && mainTrackChannel->readerSource)
        mainTrackChannel->readerSource->setLooping(enabled);
}

bool AudioCore::isMainTrackLoopEnabled() const {
    if (mainTrackChannel && mainTrackChannel->readerSource)
        return mainTrackChannel->readerSource->isLooping();
    return false;
}

void AudioCore::setFxEnabled(int fxIndex, bool enabled)
{
    if (fxIndex >= 0 && fxIndex < NUM_FX)
        fxProcessors[fxIndex].enabled = enabled;
}

void AudioCore::setFxAmount(int fxIndex, float amount)
{
    if (fxIndex >= 0 && fxIndex < NUM_FX)
        fxProcessors[fxIndex].amount = amount;
}

bool AudioCore::isFxEnabled(int fxIndex) const
{
    if (fxIndex >= 0 && fxIndex < NUM_FX)
        return fxProcessors[fxIndex].enabled;
    return false;
}

void AudioCore::setXyMode(XyMode mode)
{
    xyMode = mode;
    // Reset Total
    xyFlangerMix = 0.0f;
    xyEchoMix = 0.0f;
    xyLpMix = 0.0f;
    xyHpMix = 0.0f;
    xyLpFilter.setResonance(0.0f);
    xyLpFilter.setCutoffFrequencyHz(20000.0f);
}

void AudioCore::setXyFilter(float x, float y)
{
    if (xyMode == XyMode::Ladder)
    {
        // X -> Cutoff (20Hz a 18kHz)
        float cutoff = 20.0f * std::pow(900.0f, x);
        xyLpFilter.setCutoffFrequencyHz(cutoff);
        // Y -> Resonance (0.0 a 0.85)
        xyLpFilter.setResonance(y * 0.85f);
        xyLpMix = 1.0f;
    }
    else
    {
        // X -> Flanger (Left) ou Echo (Right)
        if (x < 0.48f) {
            xyFlangerMix = (0.5f - x) * 2.0f;
            xyEchoMix = 0.0f;
        } else if (x > 0.52f) {
            xyEchoMix = (x - 0.5f) * 2.0f;
            xyFlangerMix = 0.0f;
        } else {
            xyFlangerMix = 0.0f;
            xyEchoMix = 0.0f;
        }

        // Y -> LPF (Down) ou HPF (Up)
        if (y < 0.48f) {
            xyLpMix = (0.5f - y) * 2.0f;
            xyHpMix = 0.0f;
            float cutoff = 20000.0f * std::pow(0.015f, xyLpMix); 
            xyLpFilter.setCutoffFrequencyHz(cutoff);
            xyLpFilter.setResonance(0.0f);
        } else if (y > 0.52f) {
            xyHpMix = (y - 0.5f) * 2.0f;
            xyLpMix = 0.0f;
            float cutoff = 20.0f * std::pow(200.0f, xyHpMix); 
            xyHpFilter.setCutoffFrequencyHz(cutoff);
        } else {
            xyLpMix = 0.0f;
            xyHpMix = 0.0f;
        }
    }
}

void AudioCore::setXyFilterEnabled(bool enabled)
{
    xyEnabled.store(enabled);
}

void AudioCore::setGlobalPitchRatio(double val)
{
    val = juce::jlimit(-1.0, 1.0, val);
    if (std::abs(val - pitchValue) < 0.0001) return; // Ignore tiny changes
    
    pitchValue = val;
    double speedFactor = 1.0 + (pitchValue * 0.06);
    double ratio = speedFactor; // Higher ratio = more input samples consumed = faster
    
    mainTrackChannel->resampler->setResamplingRatio(ratio);
    for (auto& pc : padChannels) {
        if (pc && pc->resampler)
            pc->resampler->setResamplingRatio(ratio);
    }
}

bool AudioCore::loadAudioFile (const juce::File& file, int padIndex, bool shouldLoop)
{
    if (padIndex < 0 || padIndex >= (int)padChannels.size()) return false;
    if (!file.existsAsFile()) return false;

    auto* reader = formatManager.createReaderFor (file);
    if (reader != nullptr)
    {
        auto newSource = std::make_unique<CrossfadingLoopSource> (reader, true);
        newSource->setLooping(shouldLoop);
        padChannels[padIndex]->transport->setSource (newSource.get(), 0, nullptr, reader->sampleRate);
        padChannels[padIndex]->readerSource = std::move (newSource);
        return true;
    }
    return false;
}

double AudioCore::detectBpm(const juce::File& file)
{
    // Simplified peak detection for BPM estimation
    // In a real app, this would use a proper transient analyzer.
    // We'll simulate it for now or return a standard default.
    return 128.0; 
}

void AudioCore::playPad (int padIndex)
{
    if (padIndex >= 0 && padIndex < (int)padChannels.size()) {
        padChannels[padIndex]->transport->setPosition(0.0);
        padChannels[padIndex]->transport->start();
    }
}

bool AudioCore::isPadPlaying (int padIndex) const
{
    if (padIndex >= 0 && padIndex < (int)padChannels.size()) {
        return padChannels[padIndex]->transport->isPlaying();
    }
    return false;
}

void AudioCore::setPadLoop (int padIndex, bool shouldLoop)
{
    if (padIndex >= 0 && padIndex < (int)padChannels.size()) {
        if (padChannels[padIndex]->readerSource)
            padChannels[padIndex]->readerSource->setLooping(shouldLoop);
    }
}

void AudioCore::stopPad (int padIndex)
{
    if (padIndex >= 0 && padIndex < (int)padChannels.size()) {
        padChannels[padIndex]->transport->stop();
    }
}

void AudioCore::ejectPad (int padIndex)
{
    if (padIndex >= 0 && padIndex < (int)padChannels.size()) {
        padChannels[padIndex]->transport->stop();
        padChannels[padIndex]->transport->setSource(nullptr);
        padChannels[padIndex]->readerSource.reset();
    }
}

void AudioCore::setMasterVolume (float volume)
{
    masterVolume = volume;
}

void AudioCore::setTrackVolume(float volume)
{
    trackVolume = volume;
    if (mainTrackChannel && mainTrackChannel->transport)
        mainTrackChannel->transport->setGain(volume);
}

void AudioCore::setPadVolume (int padIndex, float gain)
{
    if (padIndex >= 0 && padIndex < (int)padChannels.size())
        padChannels[padIndex]->transport->setGain (gain);
}

void AudioCore::startPadRecording (int padIndex)
{
    const juce::ScopedLock sl (padRecorder.lock);
    padRecorder.writer.reset(); // Close any existing
    
    auto samplersDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                        .getChildFile("tridjs_lifeStudio")
                        .getChildFile("samplers");
    samplersDir.createDirectory();
    
    auto timeStr = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
    auto file = samplersDir.getChildFile("Pad_" + juce::String(padIndex + 1) + "_" + timeStr + ".wav");
    
    juce::WavAudioFormat wavFormat;
    if (auto outStream = std::make_unique<juce::FileOutputStream>(file))
    {
        if (outStream->openedOk())
        {
            double sr = (currentSampleRate > 0) ? currentSampleRate : 44100.0;
            if (auto writer = wavFormat.createWriterFor(outStream.release(), sr, 2, 16, {}, 0))
            {
                padRecorder.writer.reset(writer);
                padRecorder.file = file;
                padRecorder.isRecording.store(true);
            }
        }
    }
}

void AudioCore::stopPadRecording (int padIndex, std::function<void(juce::File)> onFinished)
{
    const juce::ScopedLock sl (padRecorder.lock);
    padRecorder.isRecording.store(false);
    
    if (padRecorder.writer != nullptr)
    {
        padRecorder.writer.reset(); // This flushes and closes the file
        if (onFinished) onFinished(padRecorder.file);
    }
}

void AudioCore::setStemMuted(int stemIndex, bool muted)
{
    if (stemIndex >= 0 && stemIndex < NUM_STEMS) {
        stemMuted[stemIndex] = muted;
        stemGains[stemIndex].setTargetValue(muted ? 0.0f : 1.0f);
    }
}

bool AudioCore::isStemMuted(int stemIndex) const
{
    if (stemIndex >= 0 && stemIndex < NUM_STEMS)
        return stemMuted[stemIndex];
    return false;
}
void AudioCore::startMasterRecording()
{
    const juce::ScopedLock sl(masterRecorder.lock);
    if (masterRecorder.isRecording.load()) return;

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    masterRecorder.file = tempDir.getChildFile("temp_master_rec.wav");
    
    if (masterRecorder.file.existsAsFile())
        masterRecorder.file.deleteFile();

    auto* wavFormat = formatManager.findFormatForFileExtension("wav");
    
    if (wavFormat != nullptr) {
        auto* writer = wavFormat->createWriterFor(
            new juce::FileOutputStream(masterRecorder.file),
            currentSampleRate, 2, 24, {}, 0);

        if (writer != nullptr) {
            masterRecorder.writer.reset(writer);
            masterRecorder.isRecording = true;
        }
    }
}

void AudioCore::stopMasterRecording(std::function<void(juce::File)> onFinished)
{
    juce::File result;
    {
        const juce::ScopedLock sl(masterRecorder.lock);
        masterRecorder.isRecording = false;
        masterRecorder.writer.reset();
        result = masterRecorder.file;
    }
    
    if (onFinished)
        onFinished(result);
}
