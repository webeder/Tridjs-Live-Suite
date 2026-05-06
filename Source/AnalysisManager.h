#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>

#include "TrackDatabase.h"

class AnalysisManager : public juce::Thread {
public:
  AnalysisManager(TrackDatabase &db);
  ~AnalysisManager();

  void queueTrack(const juce::File &file);
  void stopAnalysis();

  bool isAnalyzing(const juce::String &path);

  std::function<void()> onAnalysisFinished;

public:
  // Progress reporting
  float getAnalysisProgress(const juce::String &path);

private:
  void run() override;
  void analyzeNext();

  juce::String currentAnalyzingPath;
  juce::CriticalSection pathLock;

  TrackDatabase &database;
  juce::Array<juce::File> queue;
  juce::CriticalSection queueLock;
  juce::WaitableEvent waitEvent;
  std::atomic<bool> shouldStop{false};

  juce::AudioFormatManager formatManager;

  // Internal analysis helpers
  double detectBpm(juce::AudioFormatReader *reader);
  juce::String detectKey(juce::AudioFormatReader *reader);
  void generateWaveformData(juce::AudioFormatReader *reader,
                            juce::MemoryBlock &dest);
  void generateStems(juce::AudioFormatReader *reader, int trackId,
                     const juce::String &originalPath,
                     TrackDatabase::Analysis &analysis);

private:
  float currentProgress = 0.0f;
};
