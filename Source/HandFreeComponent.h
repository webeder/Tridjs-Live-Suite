#pragma once

#include <JuceHeader.h>
#include "HeaderComponent.h"
#include "StemsComponent.h"
#include "PadsGridComponent.h"
#include "FxRackComponent.h"
#include "SideBrowserComponent.h"
#include "FooterComponent.h"
#include "AudioCore.h"
#include "InputManager.h"
#include "RgbManager.h"

class HandFreeComponent : public juce::Component, private juce::Timer
{
public:
    HandFreeComponent(AudioCore& engine, InputManager& input, RgbManager& rgb, juce::AudioDeviceManager& deviceManager);
    ~HandFreeComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updatePitchValue(float v);
    void resetPitch();
    void navegarParaAba(int tabIndex);

    // Callbacks ou métodos para expor funcionalidades ao MainComponent se necessário
    std::function<void()> onResetPitchRequested;

    AudioCore& audioEngine;
    InputManager& inputManager;
    RgbManager& rgbManager;

    HeaderComponent header;
    StemsComponent stems;
    PadsGridComponent gridPads;
    FxRackComponent fxRack;
    FooterComponent footer;
    SideBrowserComponent sideBrowser;
    
    // Middle Pitch
    juce::Slider pitchSlider;
    juce::Label pitchLabel { {}, "PITCH" };
    juce::TextButton pitchValue { "0.0%" };
private:
    float currentRackWidth = 35.0f;
    float targetRackWidth = 35.0f;
    float currentBrowserWidth = 35.0f;
    float targetBrowserWidth = 35.0f;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HandFreeComponent)
};
