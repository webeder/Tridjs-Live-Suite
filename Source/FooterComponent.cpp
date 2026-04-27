#include "FooterComponent.h"

FooterComponent::FooterComponent()
{
    // Configure Divisions
    for (auto* btn : {&btn4x4, &btn8x8, &btn16x16}) {
        btn->setClickingTogglesState(true);
        btn->setRadioGroupId(199, juce::dontSendNotification); // Mutually exclusive
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff1e1e1e));
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour((juce::uint32)0xff00d1b2)); // Neon cyan
        addAndMakeVisible(btn);
    }
    btn4x4.setToggleState(true, juce::dontSendNotification); // Default 4x4

    // Configure BPM LCD
    bpmTitleLabel.setJustificationType(juce::Justification::centredRight);
    bpmTitleLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(bpmTitleLabel);

    bpmValueLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    bpmValueLabel.setJustificationType(juce::Justification::centredLeft);
    bpmValueLabel.setColour(juce::Label::textColourId, juce::Colour((juce::uint32)0xffffa500)); // Orange neon LCD
    addAndMakeVisible(bpmValueLabel);

    // Metronome Timer (UI refresh)
    startTimer(50); // 50ms pulse refresh rate

    // Load App Icon
    juce::File iconFile("C:\\TridjsMIDI\\icone.png");
    if (iconFile.existsAsFile())
        appIcon = juce::ImageFileFormat::loadFrom(iconFile);
}

FooterComponent::~FooterComponent()
{
    stopTimer();
}

void FooterComponent::setBpm(double newBpm)
{
    currentBpm = newBpm;
    bpmValueLabel.setText(juce::String(currentBpm, 2), juce::dontSendNotification);
}

void FooterComponent::setPlaying(bool playing)
{
    isPlaying = playing;
    if (!isPlaying) {
        ledIntensity = 0.0f;
        repaint();
    }
}

void FooterComponent::timerCallback()
{
    if (!isPlaying) return;

    // Simulate a metronome beat decay for visual engine
    if (ledIntensity > 0.0f) {
        ledIntensity -= 0.1f;
        repaint();
    }
    
    // Fake the pulse purely for UI metronome feeling
    static int beatCounter = 0;
    beatCounter += 50;
    if (beatCounter > (60000.0 / currentBpm)) {
        beatCounter = 0;
        ledIntensity = 1.0f; // Flash on beat
        repaint();
    }
}

void FooterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour((juce::uint32)0xff121212)); // Footer base

    // Top border
    g.setColour(juce::Colour((juce::uint32)0xff2a2a2a));
    g.drawLine(0.0f, 0.0f, (float)bounds.getWidth(), 0.0f, 2.0f);

    // Draw Metronome Heartbeat LED
    auto tempBounds = bounds;
    auto ledRect = tempBounds.removeFromLeft(80).withSizeKeepingCentre(30, 30).toFloat();
    
    juce::Colour ledColorBase = juce::Colour((juce::uint32)0xffff0000); // Red
    juce::Colour ledColorActive = ledColorBase.withAlpha(ledIntensity);

    // Outer glow
    g.setColour(ledColorActive.withAlpha(ledIntensity * 0.4f));
    g.fillEllipse(ledRect.expanded(8.0f * ledIntensity));

    // Inner Core
    g.setColour(ledIntensity > 0.2f ? ledColorActive.brighter() : ledColorBase.darker(0.8f));
    g.fillEllipse(ledRect);

    // Draw Icon and Version on Bottom Right
    auto rightArea = bounds.removeFromRight(150);
    if (appIcon.isValid())
    {
        auto iconRect = rightArea.removeFromRight(40).withSizeKeepingCentre(32, 32);
        
        // Aplica o efeito de pulsação no canal alfa do ícone se estiver tocando
        float alpha = isPlaying ? (0.6f + (ledIntensity * 0.4f)) : 1.0f;
        
        g.setOpacity(alpha);
        g.drawImageWithin(appIcon, iconRect.getX(), iconRect.getY(), iconRect.getWidth(), iconRect.getHeight(),
                          juce::RectanglePlacement::centred);
        g.setOpacity(1.0f);
    }
    g.setColour(juce::Colours::grey);
    g.setFont(juce::Font(14.0f));
    g.drawText(versionText, rightArea.reduced(5), juce::Justification::centredRight, true);
}

void FooterComponent::resized()
{
    auto area = getLocalBounds().reduced(5);
    
    // Skip the LED drawn area
    area.removeFromLeft(80);

    // Skip the Icon/Version drawn area on right
    area.removeFromRight(150);

    // Time division toggles in the center left
    auto divisionArea = area.removeFromLeft(200);
    int divWidth = divisionArea.getWidth() / 3;
    btn4x4.setBounds(divisionArea.removeFromLeft(divWidth).reduced(2));
    btn8x8.setBounds(divisionArea.removeFromLeft(divWidth).reduced(2));
    btn16x16.setBounds(divisionArea.removeFromLeft(divWidth).reduced(2));

    // BPM Synth on right
    auto bpmArea = area.removeFromRight(220);
    bpmValueLabel.setBounds(bpmArea.removeFromRight(100));
    bpmTitleLabel.setBounds(bpmArea);
}
