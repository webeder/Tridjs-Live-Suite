#pragma once

#include <JuceHeader.h>
#include "PadComponent.h"
#include <memory>
#include <vector>

class PadsGridComponent : public juce::Component
{
public:
    PadsGridComponent();
    ~PadsGridComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    
    std::vector<std::unique_ptr<PadComponent>>& getPads() { return pads; }
    
    std::function<void(double)> onPitchChanged;
    void updateBpmDisplay (double bpm);
    void updatePitchFromExtern (float value);

private:
    std::vector<std::unique_ptr<PadComponent>> pads;

    // Tempo Control Side Panel
    juce::Slider pitchSlider;
    juce::Label bpmLcd { {}, "120.00" };
    juce::TextButton resetBtn { "RESET" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadsGridComponent)
};
