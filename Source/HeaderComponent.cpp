#include "HeaderComponent.h"

HeaderComponent::HeaderComponent (juce::AudioThumbnail& thumb) : waveformDisplay(thumb)
{
    addAndMakeVisible(waveformDisplay);
    waveformDisplay.onSeekToPosition = [this](double posSeconds) {
        if (onSeek) onSeek(posSeconds);
    };

    // Transport Buttons
    playBtn.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff1a6b1a));
    playBtn.onClick = [this] {
        // If CUE is set and we're stopped, play from CUE point
        if (!waveformDisplay.isPlaying && waveformDisplay.cuePoint > 0.0) {
            if (onSeek) onSeek(waveformDisplay.cuePoint);
        }
        if (onPlay) onPlay();
    };
    addAndMakeVisible(playBtn);

    stopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff666666));
    stopBtn.onClick = [this] { if (onStop) onStop(); };
    addAndMakeVisible(stopBtn);

    ejectButton.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff553333));
    ejectButton.onClick = [this] {
        waveformDisplay.loadedTrackName = "";
        waveformDisplay.currentPos = 0.0;
        waveformDisplay.cuePoint = 0.0;
        waveformDisplay.isPlaying = false;
        waveformDisplay.repaint();
        if (onEject) onEject();
    };
    addAndMakeVisible(ejectButton);

    cueBtn.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff995500));
    cueBtn.onClick = [this] {
        if (waveformDisplay.isPlaying) {
            // While playing: save current pos as cue
            waveformDisplay.cuePoint = waveformDisplay.currentPos;
        } else {
            // While stopped: seek to cue point
            if (onSeek) onSeek(waveformDisplay.cuePoint);
        }
        repaint();
    };
    addAndMakeVisible(cueBtn);

    // VU Meter
    addAndMakeVisible(vuMeter);

    // Master Knob (0 to 100)
    masterKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    masterKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterKnob.setRange(0.0, 100.0, 1.0);
    masterKnob.setValue(100.0);
    masterKnob.setColour(juce::Slider::thumbColourId, juce::Colours::cyan);
    masterKnob.onValueChange = [this] {
        float vol = (float)(masterKnob.getValue() / 100.0);
        masterValue.setText(juce::String((int)masterKnob.getValue()), juce::dontSendNotification);
        if (onMasterVolumeChanged) onMasterVolumeChanged(vol);
    };
    addAndMakeVisible(masterKnob);
    
    masterLabel.setJustificationType(juce::Justification::centred);
    masterLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    masterLabel.setFont(juce::Font(10.0f));
    addAndMakeVisible(masterLabel);
    masterValue.setJustificationType(juce::Justification::centred);
    masterValue.setColour(juce::Label::textColourId, juce::Colours::cyan);
    masterValue.setFont(juce::Font(11.0f, juce::Font::bold));
    addAndMakeVisible(masterValue);

    // Volume Track Knob (0 to 100)
    volumeKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    volumeKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeKnob.setRange(0.0, 100.0, 1.0);
    volumeKnob.setValue(100.0);
    volumeKnob.setColour(juce::Slider::thumbColourId, juce::Colours::cyan);
    volumeKnob.onValueChange = [this] {
        float vol = (float)(volumeKnob.getValue() / 100.0);
        volumeValue.setText(juce::String((int)volumeKnob.getValue()), juce::dontSendNotification);
        if (onTrackVolumeChanged) onTrackVolumeChanged(vol);
    };
    addAndMakeVisible(volumeKnob);

    volumeLabel.setJustificationType(juce::Justification::centred);
    volumeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    volumeLabel.setFont(juce::Font(10.0f));
    addAndMakeVisible(volumeLabel);
    volumeValue.setJustificationType(juce::Justification::centred);
    volumeValue.setColour(juce::Label::textColourId, juce::Colours::cyan);
    volumeValue.setFont(juce::Font(11.0f, juce::Font::bold));
    addAndMakeVisible(volumeValue);

    // LCD
    lcdClock.setFont(juce::Font(22.0f, juce::Font::bold));
    lcdClock.setColour(juce::Label::textColourId, juce::Colour((juce::uint32)0xffffa500));
    addAndMakeVisible(lcdClock);

    quantToggle.setClickingTogglesState(true);
    quantToggle.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff222222));
    quantToggle.setColour(juce::TextButton::buttonOnColourId, juce::Colours::cyan);
    addAndMakeVisible(quantToggle);

    gearBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(gearBtn);

    // Pitch Slider (-1.0 to 1.0 mapped to -8% to +8%)
    pitchSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    pitchSlider.setRange(-1.0, 1.0, 0.001);
    pitchSlider.setValue(0.0);
    pitchSlider.setColour(juce::Slider::thumbColourId, juce::Colours::orange);
    pitchSlider.onValueChange = [this] {
        float val = (float)pitchSlider.getValue();
        float percent = val * 8.0f;
        pitchValue.setText((percent >= 0 ? "+" : "") + juce::String(percent, 1) + "%", juce::dontSendNotification);
        if (onPitchChanged) onPitchChanged(val);
    };
    addAndMakeVisible(pitchSlider);

    pitchLabel.setJustificationType(juce::Justification::centred);
    pitchLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    pitchLabel.setFont(juce::Font(10.0f));
    addAndMakeVisible(pitchLabel);

    pitchValue.setJustificationType(juce::Justification::centred);
    pitchValue.setColour(juce::Label::textColourId, juce::Colours::orange);
    pitchValue.setFont(juce::Font(14.0f, juce::Font::bold));
    pitchValue.setText("0.0%", juce::dontSendNotification);
    addAndMakeVisible(pitchValue);

    bpmDisplay.setFont(juce::Font(16.0f, juce::Font::bold));
    bpmDisplay.setColour(juce::Label::textColourId, juce::Colours::cyan);
    bpmDisplay.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmDisplay);

    // Record block
    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour ((juce::uint32)0x33aa2222));
    recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red);
    recordButton.setClickingTogglesState(true);
    recordButton.onClick = [this] {
        isRecording = recordButton.getToggleState();
        if (onRecordToggled)
            onRecordToggled(isRecording);
    };
    addAndMakeVisible (recordButton);

    addAndMakeVisible(recordDuration);
    startTimer(30);
}

HeaderComponent::~HeaderComponent()
{
    stopTimer();
}

void HeaderComponent::timerCallback()
{
    waveformDisplay.repaint();
}

void HeaderComponent::updateTransportInfo(double positionSeconds, double trackLengthSeconds, bool playing)
{
    waveformDisplay.currentPos = positionSeconds;
    waveformDisplay.trackLength = trackLengthSeconds;
    waveformDisplay.isPlaying = playing;
    
    int totalSecs = (int)positionSeconds;
    int hrs = totalSecs / 3600;
    int mins = (totalSecs % 3600) / 60;
    int secs = totalSecs % 60;
    
    lcdClock.setText(juce::String::formatted("%02d:%02d:%02d", hrs, mins, secs), juce::dontSendNotification);
    
    playBtn.setColour(juce::TextButton::buttonColourId, 
        playing ? juce::Colour((juce::uint32)0xff00cc00) : juce::Colour((juce::uint32)0xff1a6b1a));
}

void HeaderComponent::updatePeakLevel(float peak)
{
    vuMeter.setLevel(peak);
}

// ---------------------------------------------------------------
// VU Meter
// ---------------------------------------------------------------

void HeaderComponent::VuMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    
    // Dark background
    g.setColour(juce::Colour((juce::uint32)0xff0a0a0a));
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Border
    g.setColour(juce::Colour((juce::uint32)0xff444444));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    
    if (level <= 0.001f) return;
    
    float clampedLevel = juce::jlimit(0.0f, 1.5f, level);
    
    // Horizontal segments: left to right, green -> yellow -> red
    int totalSegments = 24;
    float segW = bounds.getWidth() / (float)totalSegments;
    int litSegments = (int)((clampedLevel / 1.5f) * totalSegments);
    
    for (int i = 0; i < litSegments && i < totalSegments; ++i)
    {
        float ratio = (float)i / (float)totalSegments;
        juce::Colour segColor;
        
        if (ratio < 0.5f)
            segColor = juce::Colour((juce::uint32)0xff00ff55);  // Bright green
        else if (ratio < 0.75f)
            segColor = juce::Colour((juce::uint32)0xffffee00);  // Bright yellow
        else
            segColor = juce::Colour((juce::uint32)0xffff2222);  // Bright red
        
        float x = bounds.getX() + i * segW;
        g.setColour(segColor);
        g.fillRoundedRectangle(x + 1, bounds.getY() + 2, segW - 2, bounds.getHeight() - 4, 2.0f);
    }
}

// ---------------------------------------------------------------
// WaveformDisplay
// ---------------------------------------------------------------

HeaderComponent::WaveformDisplay::WaveformDisplay (juce::AudioThumbnail& thumb) : thumbnail (thumb) {}

void HeaderComponent::WaveformDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    if (isDraggingOver)
    {
        g.fillAll(juce::Colour((juce::uint32)0xff00d1b2).withAlpha(0.2f));
        g.setColour(juce::Colours::cyan);
        g.drawRect(bounds, 2);
    }
    else
    {
        juce::ColourGradient grad(juce::Colour((juce::uint32)0xff111111), 0, 0, 
                                   juce::Colour((juce::uint32)0xff222222), 0, (float)getHeight(), false);
        g.setGradientFill(grad);
        g.fillAll();
    }

    if (loadedTrackName.isNotEmpty() && thumbnail.getTotalLength() > 0.0)
    {
        auto totalLen = thumbnail.getTotalLength();
        
        // Draw waveform
        g.setColour(juce::Colour((juce::uint32)0xff00d1b2).withAlpha(isPlaying ? 0.85f : 0.5f));
        thumbnail.drawChannels(g, bounds.reduced(2), 0.0, totalLen, 1.0f);

        // Draw CUE marker (orange vertical line)
        if (cuePoint > 0.0 && totalLen > 0.0)
        {
            auto cueX = (cuePoint / totalLen) * getWidth();
            g.setColour(juce::Colours::orange);
            g.fillRect((float)cueX - 1.0f, 0.0f, 3.0f, (float)getHeight());
            g.setFont(10.0f);
            g.drawText("CUE", (int)cueX + 3, 2, 28, 12, juce::Justification::centredLeft);
        }

        // Draw playhead (white line)
        if (totalLen > 0.0)
        {
            auto xPos = (currentPos / totalLen) * getWidth();
            g.setColour(juce::Colours::white);
            g.fillRect((float)xPos - 1.0f, 0.0f, 2.0f, (float)getHeight());
        }

        // Draw track name
        g.setColour(isPlaying ? juce::Colours::cyan : juce::Colour((juce::uint32)0xffcccccc));
        g.setFont(16.0f);
        g.drawText(loadedTrackName, bounds.reduced(8, 4), juce::Justification::bottomLeft, true);
    }
    else
    {
        g.setColour(juce::Colours::grey);
        g.setFont(20.0f);
        g.drawText("READY TO LOAD", bounds, juce::Justification::centred, true);
    }
}

void HeaderComponent::WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (loadedTrackName.isEmpty() || thumbnail.getTotalLength() <= 0.0)
        return;

    double clickRatio = juce::jlimit(0.0, 1.0, (double)e.x / (double)getWidth());
    double seekPos = clickRatio * thumbnail.getTotalLength();
    
    if (onSeekToPosition)
        onSeekToPosition(seekPos);
}

// ---------------------------------------------------------------
// Layout
// ---------------------------------------------------------------

void HeaderComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour ((juce::uint32)0xff252525));
}

void HeaderComponent::resized()
{
    auto area = getLocalBounds().reduced(5);
    
    // Pitch Control (Tall Fader on the far right)
    auto pArea = area.removeFromRight(50);
    pitchLabel.setBounds(pArea.removeFromTop(15));
    pitchValue.setBounds(pArea.removeFromTop(18));
    pitchSlider.setBounds(pArea.reduced(5, 5));

    auto topArea = area.removeFromTop(75);
    
    // Waveform takes most of the remaining top
    waveformDisplay.setBounds(topArea.removeFromLeft(topArea.getWidth() - 100));

    // Record block on the right of top area - Smaller button
    auto recArea = topArea;
    recordButton.setBounds(recArea.removeFromTop(25).reduced(2));
    recordDuration.setBounds(recArea.removeFromTop(20));

    // Bottom area (controls row)
    auto bottomArea = area;
    
    // Master knob + value + label
    auto mastArea = bottomArea.removeFromLeft(70);
    masterValue.setBounds(mastArea.removeFromTop(14));
    masterKnob.setBounds(mastArea.removeFromTop(mastArea.getHeight() - 14));
    masterLabel.setBounds(mastArea);

    // Volume Track knob + value + label
    auto volArea = bottomArea.removeFromLeft(70);
    volumeValue.setBounds(volArea.removeFromTop(14));
    volumeKnob.setBounds(volArea.removeFromTop(volArea.getHeight() - 14));
    volumeLabel.setBounds(volArea);

    // Transport buttons: PLAY | STOP | CUE
    auto transportArea = bottomArea.removeFromLeft(165);
    auto btnH = transportArea.getHeight();
    playBtn.setBounds(transportArea.removeFromLeft(55).reduced(2).withHeight(btnH));
    stopBtn.setBounds(transportArea.removeFromLeft(55).reduced(2).withHeight(btnH));
    cueBtn.setBounds(transportArea.removeFromLeft(55).reduced(2).withHeight(btnH));

    // BPM Display
    bpmDisplay.setBounds(bottomArea.removeFromLeft(100).reduced(5));

    // VU Meter
    vuMeter.setBounds(bottomArea.removeFromLeft(120).reduced(2, 8));

    // Right side: Clock, Eject and Settings
    gearBtn.setBounds(bottomArea.removeFromRight(40).withSizeKeepingCentre(30, 30));
    lcdClock.setBounds(bottomArea.removeFromRight(100));
    
    // Ejetar ao lado de Quantize
    auto rightBtns = bottomArea.removeFromRight(120);
    ejectButton.setBounds(rightBtns.removeFromLeft(60).withSizeKeepingCentre(55, 30));
    quantToggle.setBounds(rightBtns.removeFromLeft(60).withSizeKeepingCentre(55, 30));
}

// ---------------------------------------------------------------
// OS File Drag And Drop
// ---------------------------------------------------------------

bool HeaderComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& file : files) {
        if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".mp3"))
            return true;
    }
    return false;
}

void HeaderComponent::filesDropped(const juce::StringArray& files, int, int)
{
    waveformDisplay.isDraggingOver = false;
    for (auto& file : files) {
        if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".mp3")) {
            juce::File f(file);
            waveformDisplay.loadedTrackName = f.getFileNameWithoutExtension();
            waveformDisplay.cuePoint = 0.0;
            waveformDisplay.repaint();
            if (onFileDropped) onFileDropped(f);
            break;
        }
    }
}

void HeaderComponent::fileDragEnter(const juce::StringArray&, int, int)
{
    waveformDisplay.isDraggingOver = true;
    waveformDisplay.repaint();
}

void HeaderComponent::fileDragExit(const juce::StringArray&)
{
    waveformDisplay.isDraggingOver = false;
    waveformDisplay.repaint();
}

// ---------------------------------------------------------------
// Internal FileTree Drag And Drop
// ---------------------------------------------------------------

bool HeaderComponent::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details)
{
    return details.description.toString() == "AudioFileSelected";
}

void HeaderComponent::itemDropped(const juce::DragAndDropTarget::SourceDetails& details)
{
    waveformDisplay.isDraggingOver = false;
    if (auto* tree = dynamic_cast<juce::FileTreeComponent*>(details.sourceComponent.get()))
    {
        auto selectedFile = tree->getSelectedFile();
        if (selectedFile.existsAsFile())
        {
            waveformDisplay.loadedTrackName = selectedFile.getFileNameWithoutExtension();
            waveformDisplay.isPlaying = false;
            waveformDisplay.cuePoint = 0.0;
            if (onFileDropped) onFileDropped(selectedFile);
        }
    }
    waveformDisplay.repaint();
}

void HeaderComponent::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
    if (details.description.toString() == "AudioFileSelected") {
        waveformDisplay.isDraggingOver = true;
        waveformDisplay.repaint();
    }
}

void HeaderComponent::itemDragExit(const juce::DragAndDropTarget::SourceDetails& details)
{
    waveformDisplay.isDraggingOver = false;
    waveformDisplay.repaint();
}

void HeaderComponent::updateMasterVolumeFromExtern(float v)
{
    masterKnob.setValue(v * 100.0, juce::dontSendNotification);
    masterValue.setText(juce::String((int)(v * 100.0)), juce::dontSendNotification);
}

void HeaderComponent::updateTrackVolumeFromExtern(float v)
{
    volumeKnob.setValue(v * 100.0, juce::dontSendNotification);
    volumeValue.setText(juce::String((int)(v * 100.0)), juce::dontSendNotification);
}

void HeaderComponent::updateBpmDisplay(double bpm)
{
    bpmDisplay.setText("BPM: " + juce::String(bpm, 1), juce::dontSendNotification);
}

