#pragma once
#include <JuceHeader.h>
#include "VocalVstChain.h"
#include "PluginScannerManager.h"
#include "VstFloatingWindow.h"

class VstControlWidget : public juce::Component
{
public:
    VstControlWidget(VocalVstChain& chain, PluginScannerManager& manager) 
        : vstChain(chain), vstManager(manager)
    {
        addAndMakeVisible(vstButton);
        vstButton.setButtonText("VOCAL");
        vstButton.setTooltip("Toggle Vocal FX Chain Bypass");
        vstButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
        vstButton.setConnectedEdges(juce::Button::ConnectedOnBottom);
        
        vstButton.onClick = [this] {
            bool newState = !vstChain.bypass.load();
            vstChain.bypass.store(newState);
            updateButtonState();
        };

        addAndMakeVisible(editBtn);
        editBtn.setButtonText("FX EDIT");
        editBtn.setTooltip("Open Reaper-style FX Chain Editor");
        editBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
        editBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::cyan);
        editBtn.setConnectedEdges(juce::Button::ConnectedOnTop);
        editBtn.onClick = [this] {
            showEditor();
        };
        
        updateButtonState();
    }

    ~VstControlWidget() override
    {
        floatingWindow.reset();
    }

    void showEditor()
    {
        if (floatingWindow == nullptr)
        {
            floatingWindow = std::make_unique<VstFloatingWindow>(vstChain, vstManager);
        }
        
        floatingWindow->setVisible(true);
        floatingWindow->toFront(true);
    }

    void updateButtonState()
    {
        bool active = !vstChain.bypass.load();
        vstButton.setColour(juce::TextButton::buttonColourId, active ? juce::Colours::green.withAlpha(0.6f) : juce::Colour(0xff333333));
        vstButton.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::white : juce::Colours::grey);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        vstButton.setBounds(area.removeFromTop(getHeight() * 0.5f).reduced(1, 0));
        editBtn.setBounds(area.reduced(1, 0));
    }

private:
    VocalVstChain& vstChain;
    PluginScannerManager& vstManager;
    juce::TextButton vstButton;
    juce::TextButton editBtn;
    std::unique_ptr<VstFloatingWindow> floatingWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VstControlWidget)
};
