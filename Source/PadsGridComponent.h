#pragma once

#include <JuceHeader.h>
#include "LanguageManager.h"
#include "PadComponent.h"
#include <memory>
#include <vector>

class PadsGridComponent : public juce::Component,
                          public juce::ChangeListener
{
public:
    PadsGridComponent();
    ~PadsGridComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void updateLanguage();
    
    std::vector<std::unique_ptr<PadComponent>>& getPads() { return pads; }
    
    std::function<void(double)> onPitchChanged;
    void updateBpmDisplay (double bpm);
    void updatePitchFromExtern (float value);

private:
    std::vector<std::unique_ptr<PadComponent>> pads;

    // Tempo Control Side Panel
    juce::Slider pitchSlider;
    juce::Label bpmLcd { {}, "120.00" };
    juce::TextButton resetBtn { "" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadsGridComponent)
};
