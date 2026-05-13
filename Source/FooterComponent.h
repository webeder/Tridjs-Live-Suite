#pragma once
#include <JuceHeader.h>

class FooterComponent : public juce::Component, public juce::Timer, public juce::ChangeListener
{
public:
    FooterComponent();
    ~FooterComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void updateLanguage();
    void timerCallback() override;

    void setBpm(double newBpm);
    void setPlaying(bool playing);

private:
    double currentBpm = 120.0;
    bool isLedOn = false;
    float ledIntensity = 1.0f;
    bool isPlaying = false;

    // Time division toggles
    juce::TextButton btn4x4 { "4x4" };
    juce::TextButton btn8x8 { "8x8" };
    juce::TextButton btn16x16 { "16x16" };

    // BPM Sync Dashboard
    juce::Label bpmTitleLabel { {}, "" };
    juce::Label bpmValueLabel { {}, "120.00" };

    juce::Image appIcon;
    juce::String versionText = "v 1.0.0";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterComponent)
};
