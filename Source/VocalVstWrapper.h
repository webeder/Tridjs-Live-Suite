#pragma once
#include <JuceHeader.h>
#include <atomic>

class VocalVstWrapper 
{
public:
    VocalVstWrapper()
    {
        formatManager.addDefaultFormats();
    }

    ~VocalVstWrapper()
    {
        instance.reset();
    }

    void prepare(double sampleRate, int samplesPerBlock)
    {
        lastSampleRate = sampleRate;
        lastBlockSize = samplesPerBlock;

        if (instance != nullptr)
            instance->prepareToPlay(sampleRate, samplesPerBlock);
    }

    void release()
    {
        if (instance != nullptr)
            instance->releaseResources();
    }

    void processMicAudio(juce::AudioBuffer<float>& buffer)
    {
        if (bypass.load() || instance == nullptr)
            return;

        juce::MidiBuffer midi;
        instance->processBlock(buffer, midi);
    }

    bool loadPlugin(const juce::String& path)
    {
        juce::OwnedArray<juce::PluginDescription> descriptions;
        juce::KnownPluginList pluginList;
        
        juce::PluginDescription desc;
        desc.fileOrIdentifier = path;
        desc.pluginFormatName = "VST3";

        juce::String error;
        auto newInstance = formatManager.createPluginInstance(desc, lastSampleRate, lastBlockSize, error);

        if (newInstance != nullptr)
        {
            juce::ScopedLock lock(audioLock);
            instance = std::move(newInstance);
            instance->prepareToPlay(lastSampleRate, lastBlockSize);
            return true;
        }

        return false;
    }

    juce::AudioPluginInstance* getInstance() const { return instance.get(); }
    
    std::atomic<bool> bypass { true };

private:
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::CriticalSection audioLock;
    
    double lastSampleRate = 44100.0;
    int lastBlockSize = 512;
};
