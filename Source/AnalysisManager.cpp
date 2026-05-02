#include "AnalysisManager.h"
#include "TrackDatabase.h"
#include <vector>

AnalysisManager::AnalysisManager(TrackDatabase& db) 
    : Thread("AnalysisThread"), database(db)
{
    formatManager.registerBasicFormats();
    startThread(juce::Thread::Priority::background);
}

AnalysisManager::~AnalysisManager()
{
    stopAnalysis();
}

void AnalysisManager::queueTrack(const juce::File& file)
{
    juce::ScopedLock sl(queueLock);
    queue.add(file);
    waitEvent.signal();
}

void AnalysisManager::stopAnalysis()
{
    shouldStop = true;
    waitEvent.signal();
    stopThread(2000);
}

void AnalysisManager::run()
{
    while (!threadShouldExit() && !shouldStop)
    {
        analyzeNext();
        waitEvent.wait(500);
    }
}

void AnalysisManager::analyzeNext()
{
    juce::File file;
    {
        juce::ScopedLock sl(queueLock);
        if (queue.isEmpty()) return;
        file = queue[0];
        queue.remove(0);
    }

    {
        juce::ScopedLock sl(pathLock);
        currentAnalyzingPath = file.getFullPathName();
    }

    // Skip if already in DB with analysis
    TrackDatabase::Track t;
    if (database.getTrackByPath(file.getFullPathName(), t)) {
        TrackDatabase::Analysis a;
        if (database.loadAnalysis(t.id, a)) {
            {
                juce::ScopedLock sl(pathLock);
                currentAnalyzingPath = "";
            }
            return; 
        }
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) return;

    // Basic metadata
    t.path = file.getFullPathName();
    t.name = file.getFileNameWithoutExtension();
    t.artist = "Unknown Artist"; // In a real app we'd read ID3 tags here
    t.createdAt = juce::Time::getCurrentTime();
    t.lastPlayed = t.createdAt;

    // DSP Analysis
    t.bpm = detectBpm(reader.get());
    t.key = detectKey(reader.get());
    
    int trackId = database.addOrUpdateTrack(t);
    
    TrackDatabase::Analysis a;
    a.trackId = trackId;
    generateWaveformData(reader.get(), a.waveform);
    a.beatgrid = "{\"version\":1,\"markers\":[]}"; // Placeholder
    
    database.saveAnalysis(a);
    
    if (onAnalysisFinished) 
        juce::MessageManager::callAsync([this] { if (onAnalysisFinished) onAnalysisFinished(); });

    {
        juce::ScopedLock sl(pathLock);
        currentAnalyzingPath = "";
    }
}

bool AnalysisManager::isAnalyzing(const juce::String& path)
{
    juce::ScopedLock sl(pathLock);
    return currentAnalyzingPath == path;
}

double AnalysisManager::detectBpm(juce::AudioFormatReader* reader)
{
    // Simplified: Return 128 or use basic logic
    // A real implementation would use FFT or autocorrelation
    return 124.0 + (std::rand() % 10);
}

juce::String AnalysisManager::detectKey(juce::AudioFormatReader* reader)
{
    const char* keys[] = {"Am", "Em", "Bm", "F#m", "C#m", "G#m", "D#m", "Bbm", "Fm", "Cm", "Gm", "Dm"};
    return keys[std::rand() % 12];
}

void AnalysisManager::generateWaveformData(juce::AudioFormatReader* reader, juce::MemoryBlock& dest)
{
    // Generate a low-res thumbnail blob
    int numSamples = (int)reader->lengthInSamples;
    int samplesPerPixel = numSamples / 1000;
    if (samplesPerPixel < 1) samplesPerPixel = 1;
    
    int numPixels = numSamples / samplesPerPixel;
    dest.setSize(numPixels * sizeof(float));
    float* out = (float*)dest.getData();
    
    juce::AudioBuffer<float> buffer(reader->numChannels, samplesPerPixel);
    for (int i = 0; i < numPixels; ++i) {
        reader->read(&buffer, 0, samplesPerPixel, (juce::int64)i * samplesPerPixel, true, true);
        out[i] = buffer.getMagnitude(0, samplesPerPixel);
        if (threadShouldExit()) break;
    }
}
