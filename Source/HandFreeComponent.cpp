#include "HandFreeComponent.h"

HandFreeComponent::HandFreeComponent(AudioCore& engine, InputManager& input, RgbManager& rgb, juce::AudioDeviceManager& deviceManager)
    : audioEngine(engine),
      inputManager(input),
      rgbManager(rgb),
      header(engine.getThumbnail()),
      fxRack(deviceManager)
{
    // 1. Setup the callbacks first
    fxRack.onExpandedChanged = [this](bool expanded) {
        resized(); // Just trigger layout, internal animation handles the rest
    };

    sideBrowser.onExpandedChanged = [this](bool expanded) {
        targetBrowserWidth = expanded ? 250.0f : 35.0f;
        startTimerHz(60);
    };

    // 2. Initialize state and force a sync
    fxRack.setExpanded(false);
    sideBrowser.setExpanded(false);
    currentRackWidth = 35.0f;
    targetRackWidth = 35.0f;
    currentBrowserWidth = 35.0f;
    targetBrowserWidth = 35.0f;

    addAndMakeVisible(header);
    addAndMakeVisible(stems);
    addAndMakeVisible(gridPads);
    addAndMakeVisible(fxRack);
    addAndMakeVisible(footer);
    addAndMakeVisible(sideBrowser);

    // Pitch Components Setup
    pitchSlider.setName("PitchSlider");
    pitchSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    pitchSlider.setRange(-1.0, 1.0, 0.001);
    pitchSlider.setValue(0.0);
    pitchSlider.onValueChange = [this] {
        float val = (float)pitchSlider.getValue();
        float percent = val * 6.0f;
        pitchValue.setButtonText((percent >= 0 ? "+" : "") + juce::String(percent, 1) + "%");
        audioEngine.setGlobalPitchRatio((double)val);
    };
    addAndMakeVisible(pitchSlider);

    pitchLabel.setFont(juce::Font("Roboto", 10.0f, juce::Font::bold));
    pitchLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    pitchLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pitchLabel);

    pitchValue.setButtonText("0.0%");
    pitchValue.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    pitchValue.setColour(juce::TextButton::textColourOffId, juce::Colours::orange);
    pitchValue.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    pitchValue.setColour(juce::TextButton::textColourOnId, juce::Colours::orange.brighter());
    pitchValue.onClick = [this] { resetPitch(); };
    addAndMakeVisible(pitchValue);

    fxRack.onExpandedChanged = [this](bool expanded) {
        targetRackWidth = expanded ? 320.0f : 35.0f;
        startTimerHz(60); // Start animation timer
    };

    // Note: The wiring (callbacks) will be moved here or kept in Main if it depends on Main state.
    // However, the user wants to encapsulate "everything". 
    // I'll put most wiring here, but some might need to be exposed.
}

HandFreeComponent::~HandFreeComponent() {}

void HandFreeComponent::paint(juce::Graphics& g)
{
    // Background is handled by MainComponent or here
}

void HandFreeComponent::resized()
{
    auto area = getLocalBounds();
    header.setBounds(area.removeFromTop(150));
    footer.setBounds(area.removeFromBottom(40));
    auto body = area;
    
    int browserW = (int)currentBrowserWidth;
    sideBrowser.setBounds(body.removeFromLeft(browserW));
    
    int rackWidth = fxRack.getWidth();
    fxRack.setBounds(body.removeFromRight(rackWidth));
    
    stems.setBounds(body.removeFromTop(50));
    
    // Pitch slider on the right of pads
    auto pArea = body.removeFromRight(55).reduced(2, 20);
    pitchLabel.setBounds(pArea.removeFromTop(20));
    pitchValue.setBounds(pArea.removeFromTop(25));
    pitchSlider.setBounds(pArea);

    gridPads.setBounds(body);
}

void HandFreeComponent::updatePitchValue(float v)
{
    pitchSlider.setValue(v, juce::dontSendNotification);
    float percent = v * 6.0f;
    pitchValue.setButtonText((percent >= 0 ? "+" : "") + juce::String(percent, 1) + "%");
    audioEngine.setGlobalPitchRatio((double)v);
}

void HandFreeComponent::resetPitch()
{
    pitchSlider.setValue(0.0, juce::sendNotification);
    if (onResetPitchRequested) onResetPitchRequested();
}

void HandFreeComponent::navegarParaAba(int tabIndex)
{
    fxRack.tabs.setCurrentTabIndex(tabIndex);
    if (!fxRack.isExpanded()) {
        fxRack.setExpanded(true);
    } else {
        targetRackWidth = 320.0f;
        startTimerHz(60);
    }
}

void HandFreeComponent::timerCallback()
{
    float diffBrowser = targetBrowserWidth - currentBrowserWidth;
    bool browserMoving = std::abs(diffBrowser) > 0.5f;

    if (browserMoving) currentBrowserWidth += diffBrowser * 0.35f;
    else currentBrowserWidth = targetBrowserWidth;

    if (!browserMoving) {
        stopTimer();
    }
    
    resized();
}
