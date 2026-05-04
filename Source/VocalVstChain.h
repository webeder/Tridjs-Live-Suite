#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

class VocalVstChain 
{
public:
    VocalVstChain(juce::AudioPluginFormatManager& fm) : formatManager(fm) {}

    ~VocalVstChain()
    {
        juce::ScopedLock lock(audioLock);
        plugins.clear();
    }

    void prepare(double sampleRate, int samplesPerBlock)
    {
        lastSampleRate = sampleRate;
        lastBlockSize = samplesPerBlock;

        juce::ScopedLock lock(audioLock);
        for (auto& p : plugins)
            p->prepareToPlay(sampleRate, samplesPerBlock);
    }

    void release()
    {
        juce::ScopedLock lock(audioLock);
        for (auto& p : plugins)
            p->releaseResources();
    }

    void processMicAudio(juce::AudioBuffer<float>& buffer)
    {
        juce::ScopedLock lock(audioLock);
        juce::MidiBuffer midi;
        bool isBypassed = bypass.load();
        
        for (auto& p : plugins)
        {
            if (!p->isSuspended())
            {
                if (isBypassed)
                    p->processBlockBypassed(buffer, midi);
                else
                    p->processBlock(buffer, midi);
            }
        }
    }

    bool addPlugin(const juce::PluginDescription& desc)
    {
        juce::String error;
        auto instance = formatManager.createPluginInstance(desc, lastSampleRate, lastBlockSize, error);

        if (instance != nullptr)
        {
            instance->prepareToPlay(lastSampleRate, lastBlockSize);
            
            juce::ScopedLock lock(audioLock);
            plugins.push_back(std::move(instance));
            return true;
        }

        return false;
    }

    void removePlugin(int index)
    {
        juce::ScopedLock lock(audioLock);
        if (index >= 0 && index < (int)plugins.size())
            plugins.erase(plugins.begin() + index);
    }

    int getNumPlugins() const { return (int)plugins.size(); }
    juce::AudioPluginInstance* getPlugin(int index) const 
    { 
        if (index >= 0 && index < (int)plugins.size())
            return plugins[index].get();
        return nullptr;
    }

    std::atomic<bool> bypass { false };

private:
    juce::AudioPluginFormatManager& formatManager;
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> plugins;
    juce::CriticalSection audioLock;
    
    double lastSampleRate = 44100.0;
    int lastBlockSize = 512;
};
