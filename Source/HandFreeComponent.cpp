#include "HandFreeComponent.h"

HandFreeComponent::HandFreeComponent(AudioCore& engine, InputManager& input, RgbManager& rgb, 
                                   juce::AudioDeviceManager& deviceManager, TrackBrowserComponent* browser)
    : audioEngine(engine),
      inputManager(input),
      rgbManager(rgb),
      header(engine),
      fxRack(deviceManager),
      browserPtr(browser)
{
    if (browserPtr)
        bottomPanel.setContent(browserPtr);
    
    // 1. Setup the callbacks first
    stems.onStemMuteChanged = [this](int idx, bool muted) {
        audioEngine.setStemMuted(idx, muted);
    };

    for (int i = 0; i < 9; ++i) {
        if (auto* pad = gridPads.getPads()[i].get()) {
            pad->onPlayStateChanged = [this, i](int, bool state) {
                if (state) audioEngine.playPad(i);
                else audioEngine.stopPad(i);
            };
            pad->onRecordRequested = [this, i](int) {
                if (audioEngine.isPadRecording(i)) {
                    audioEngine.stopPadRecording(i, [this](const juce::File& f) {
                        if (browserPtr && f.existsAsFile()) {
                            browserPtr->refresh();
                        }
                    });
                } else {
                    audioEngine.startPadRecording(i);
                }
            };
            pad->onLoopToggled = [this, i](int, bool state) {
                audioEngine.setPadLoop(i, state);
            };
            pad->onEjectRequested = [this, i](int) {
                audioEngine.ejectPad(i);
            };
            pad->onVolumeChanged = [this, i](int, float vol) {
                audioEngine.setPadVolume(i, vol);
            };
            pad->onFileDropped = [this, i](int, const juce::File& f) {
                audioEngine.loadAudioFile(f, i, false);
            };
        }
    }

    fxRack.onExpandedChanged = [this](bool expanded) {
        resized(); // Just trigger layout, internal animation handles the rest
    };

    // 2. Initialize state and force a sync
    fxRack.setExpanded(false);
    currentRackWidth = 35.0f;
    targetRackWidth = 35.0f;

    addAndMakeVisible(header);
    addAndMakeVisible(stems);
    addAndMakeVisible(gridPads);
    addAndMakeVisible(fxRack);
    addAndMakeVisible(footer);
    addAndMakeVisible(bottomPanel);

    bottomPanel.onClick = [this] {
        setExpanded(!isExpanded());
        
        if (isExpanded()) {
            auto availableHeight = getHeight() - 150 - 40 - 50; 
            targetBottomHeight = (float)availableHeight * 0.45f;
        } else {
            targetBottomHeight = 25.0f;
        }
        startTimerHz(60);
    };

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
    
    int rackWidth = fxRack.getWidth();
    fxRack.setBounds(body.removeFromRight(rackWidth));
    
    stems.setBounds(body.removeFromTop(50));
    
    // Bottom panel and Pads Grid
    bottomPanel.setBounds(body.removeFromBottom((int)currentBottomHeight).reduced(2, 0));
    
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
    float diffBottom = targetBottomHeight - currentBottomHeight;
    bool bottomMoving = std::abs(diffBottom) > 0.5f;
    if (bottomMoving) currentBottomHeight += diffBottom * 0.25f;
    else currentBottomHeight = targetBottomHeight;

    if (!bottomMoving) {
        stopTimer();
    }
    
    resized();
}

void HandFreeComponent::setExpanded(bool expanded)
{
    panelExpanded = expanded;
    repaint();
}
