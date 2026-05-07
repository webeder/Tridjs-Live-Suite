#pragma once

#include <JuceHeader.h>
#include "HeaderComponent.h"
#include "StemsComponent.h"
#include "PadsGridComponent.h"
#include "FxRackComponent.h"
#include "FooterComponent.h"
#include "AudioCore.h"
#include "InputManager.h"
#include "RgbManager.h"
#include "TrackDatabase.h"
#include "AnalysisManager.h"
#include "TrackBrowserComponent.h"

class BottomPanelComponent : public juce::Component
{
public:
    BottomPanelComponent() { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff121212));
        g.fillRoundedRectangle(area, 5.0f);
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawRoundedRectangle(area, 5.0f, 1.0f);
        float barW = 40.0f; float barH = 4.0f;
        g.setColour(juce::Colours::grey.withAlpha(0.5f));
        g.fillRoundedRectangle(area.getCentreX() - barW/2, 6.0f, barW, barH, 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(12.0f);
        g.drawText("MUSIC BROWSER", area.withHeight(25), juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent&) override { if (onClick) onClick(); }
    
    void setContent(juce::Component* content) { 
        browserContainer = content; 
        if (browserContainer) addAndMakeVisible(browserContainer);
        resized();
    }
    
    void resized() override {
        if (browserContainer) {
            auto area = getLocalBounds();
            area.removeFromTop(25); // Drag bar space
            if (browserContainer && browserContainer->getParentComponent() == this)
                browserContainer->setBounds(area);
        }
    }

    std::function<void()> onClick;

private:
    juce::Component* browserContainer = nullptr;
};

class HandFreeComponent : public juce::Component, private juce::Timer
{
public:
    HandFreeComponent(AudioCore& engine, InputManager& input, RgbManager& rgb, 
                      juce::AudioDeviceManager& deviceManager, TrackBrowserComponent* browser);
    ~HandFreeComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updatePitchValue(float v);
    void resetPitch();
    void navegarParaAba(int tabIndex);

    void setExpanded(bool expanded);
    bool isExpanded() const { return panelExpanded; }

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
    BottomPanelComponent bottomPanel;
    
    // Data (Now managed by MainComponent)
    TrackBrowserComponent* browserPtr = nullptr;
    
    // Middle Pitch
    juce::Slider pitchSlider;
    juce::Label pitchLabel { {}, "PITCH" };
    juce::TextButton pitchValue { "0.0%" };
private:
    bool panelExpanded = false;
    float currentRackWidth = 35.0f;
    float targetRackWidth = 35.0f;
    float currentBottomHeight = 25.0f;
    float targetBottomHeight = 25.0f;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HandFreeComponent)
};
