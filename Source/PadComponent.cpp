#include "PadComponent.h"

PadComponent::PadComponent (int padIndex) : index (padIndex)
{
    micIcon = juce::ImageFileFormat::loadFrom(juce::File("C:\\TridjsMIDI\\mic.png"));
    loopIcon = juce::ImageFileFormat::loadFrom(juce::File("C:\\TridjsMIDI\\loop.png"));
    ejectIcon = juce::ImageFileFormat::loadFrom(juce::File("C:\\TridjsMIDI\\ejt.png"));

    recordBtn.setImages(false, true, true, micIcon, 1.0f, {}, micIcon, 1.0f, juce::Colours::white.withAlpha(0.2f), micIcon, 1.0f, juce::Colours::red.withAlpha(0.6f));
    recordBtn.onClick = [this] {
        isRecording = !isRecording;
        if (isRecording) { isActive = false; if (onPlayStateChanged) onPlayStateChanged(index, false); }
        if (onRecordRequested) onRecordRequested(index);
        repaint();
    };
    addAndMakeVisible (recordBtn);

    loopBtn.setImages(false, true, true, loopIcon, 1.0f, {}, loopIcon, 1.0f, juce::Colours::white.withAlpha(0.2f), loopIcon, 1.0f, juce::Colours::orange.withAlpha(0.6f));
    loopBtn.setClickingTogglesState(true);
    loopBtn.onClick = [this] {
        isLoopActive = loopBtn.getToggleState();
        if (onLoopToggled) onLoopToggled(index, isLoopActive);
        
        // If we unmark loop while playing, stop immediately
        if (!isLoopActive && isActive) {
            isActive = false;
            if (onPlayStateChanged) onPlayStateChanged(index, false);
        }
        repaint();
    };
    addAndMakeVisible (loopBtn);

    ejectBtn.setImages(false, true, true, ejectIcon, 1.0f, {}, ejectIcon, 1.0f, juce::Colours::white.withAlpha(0.2f), ejectIcon, 1.0f, juce::Colours::lightgrey.withAlpha(0.6f));
    ejectBtn.onClick = [this] {
        loadedFilename = ""; 
        currentFilePath = ""; // CLEAR PATH FOR PERSISTENCE
        bool wasActive = isActive;
        isActive = false; 
        isRecording = false;
        if (wasActive && onPlayStateChanged) onPlayStateChanged(index, false);
        if (onEjectRequested) onEjectRequested(index);
        repaint();
    };
    addAndMakeVisible (ejectBtn);

    volumeSlider.setSliderStyle(juce::Slider::LinearBarVertical);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setAlpha(0.0f); // Make the slider invisible but functional
    volumeSlider.onValueChange = [this] { 
        if (onVolumeChanged) onVolumeChanged(index, (float)volumeSlider.getValue()); 
        repaint();
    };
    addAndMakeVisible(volumeSlider);
}

void PadComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto cornerSize = 14.0f;
    
    // Choose theme color based on index
    juce::Colour themeColor = colorCyan;
    if (index % 4 == 1) themeColor = juce::Colours::white.withAlpha(0.6f);
    else if (index % 4 == 2) themeColor = colorOrange;
    else if (index % 4 == 3) themeColor = colorNeon;

    // 1. Draw Outer Glow if Active or Recording
    if (isActive || isRecording)
    {
        float glowSize = 4.0f;
        if (isRecording) {
            float blink = (float)std::abs(std::sin(juce::Time::getMillisecondCounterHiRes() * 0.008));
            glowSize += 2.0f * blink;
            themeColor = juce::Colours::red;
        }

        for (int i = 1; i <= 5; ++i) {
            g.setColour(themeColor.withAlpha(0.1f / (float)i));
            g.drawRoundedRectangle(area.expanded((float)i * 1.5f), cornerSize + (float)i, 2.0f);
        }
    }

    // 2. Background Gradient
    juce::ColourGradient grad(colorBgStart, area.getCentreX(), area.getY(),
                              colorBgEnd, area.getCentreX(), area.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(area, cornerSize);

    // 3. Subtle Border
    if (isActive) {
        g.setColour(themeColor.withAlpha(0.8f));
        g.drawRoundedRectangle(area.reduced(0.5f), cornerSize, 1.5f);
    } else {
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawRoundedRectangle(area.reduced(0.5f), cornerSize, 1.0f);
    }

    // 4. Indicator Bar (Vertical gradient bar on the right)
    auto indicatorArea = area.removeFromRight(12).reduced(4, 8);
    float val = (float)volumeSlider.getValue();
    
    // Track background
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(indicatorArea, 2.0f);

    // Progress bar
    auto progressArea = indicatorArea.withTrimmedTop(indicatorArea.getHeight() * (1.0f - val));
    juce::ColourGradient barGrad(colorCyan, progressArea.getCentreX(), progressArea.getBottom(),
                                 colorOrange, progressArea.getCentreX(), progressArea.getY(), false);
    g.setGradientFill(barGrad);
    g.fillRoundedRectangle(progressArea, 2.0f);

    // 5. Main Text (Always show filename or Pad X)
    juce::String textToDisplay = loadedFilename.isEmpty() ? "PAD " + juce::String(index + 1) : loadedFilename;
    g.setColour(juce::Colours::white.withAlpha(isActive ? 1.0f : 0.7f));
    g.setFont(juce::Font("Space Grotesk", 18.0f, juce::Font::bold).withExtraKerningFactor(0.1f));
    g.drawText(textToDisplay.toUpperCase(), getLocalBounds().reduced(15, 20), juce::Justification::centred);

    // 6. RGB Indicator (Bottom right inside pad, small square + label)
    if (currentRgbColor != juce::Colours::transparentBlack) {
        auto b = getLocalBounds().reduced(12, 12);
        auto rgbArea = b.removeFromBottom(25).removeFromRight(120);
        
        // Small square
        g.setColour(currentRgbColor);
        g.fillRoundedRectangle(rgbArea.removeFromRight(10).withSizeKeepingCentre(10, 10).toFloat(), 2.0f);
        
        // Label
        if (rgbLabel.isNotEmpty()) {
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(rgbLabel.toUpperCase(), rgbArea.removeFromLeft(rgbArea.getWidth() - 15), juce::Justification::centredRight);
        }
    }

    // 7. Recording Dot
    if (isRecording) {
        float blink = (float)std::abs(std::sin(juce::Time::getMillisecondCounterHiRes() * 0.01));
        g.setColour(juce::Colours::red.withAlpha(0.5f + 0.5f * blink));
        g.fillEllipse(area.getX() + 10, area.getY() + 10, 10, 10);
    }
}

void PadComponent::resized()
{
    auto area = getLocalBounds();
    
    // Position invisible volume slider on the right edge where the indicator is drawn
    volumeSlider.setBounds(area.removeFromRight(15).reduced(2, 10));

    auto controlsArea = area.removeFromBottom(25).reduced(5, 0);
    recordBtn.setBounds(controlsArea.removeFromLeft(25));
    loopBtn.setBounds(controlsArea.removeFromLeft(25));
    ejectBtn.setBounds(controlsArea.removeFromLeft(25));
}

void PadComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) { if (onRightClick) onRightClick(); return; }
    if (onLeftClick && onLeftClick()) return;

    isMouseDown = true;
    isActive = !isActive;

    if (onPlayStateChanged) {
        if (isActive) {
            // If starting one-shot (loop off), force restart from 0
            if (!isLoopActive) onPlayStateChanged(index, false);
            onPlayStateChanged(index, true);
        } else {
            onPlayStateChanged(index, false); // Stop immediately
        }
    }
    
    repaint();
}

void PadComponent::mouseUp (const juce::MouseEvent& e)
{
    isMouseDown = false;
    repaint();
}

void PadComponent::setLoadedFile (const juce::File& file) { loadedFilename = file.getFileName(); currentFilePath = file.getFullPathName(); repaint(); }
void PadComponent::setPlayStateExternally (bool playing) { if (isActive != playing) { isActive = playing; repaint(); } }
void PadComponent::setLoopStateExternally (bool looping) { isLoopActive = looping; loopBtn.setToggleState(looping, juce::dontSendNotification); repaint(); }
void PadComponent::setRgbInfo(const juce::String& label, juce::Colour color) { rgbLabel = label; currentRgbColor = color; repaint(); }

void PadComponent::eject() { ejectBtn.triggerClick(); }
void PadComponent::triggerRecord() { recordBtn.triggerClick(); }

bool PadComponent::isInterestedInFileDrag (const juce::StringArray& files) { return true; }
void PadComponent::filesDropped (const juce::StringArray& files, int x, int y) { if (files.size() > 0 && onFileDropped) onFileDropped(index, juce::File(files[0])); }
void PadComponent::fileDragEnter (const juce::StringArray& files, int x, int y) { isDraggingOver = true; repaint(); }
void PadComponent::fileDragExit (const juce::StringArray& files) { isDraggingOver = false; repaint(); }

bool PadComponent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) 
{ 
    return details.description.toString() == "AudioFileSelected"; 
}
void PadComponent::itemDropped (const juce::DragAndDropTarget::SourceDetails& details) 
{ 
    if (auto* tree = dynamic_cast<juce::FileTreeComponent*>(details.sourceComponent.get()))
    {
        auto file = tree->getSelectedFile();
        if (file.existsAsFile() && onFileDropped)
            onFileDropped(index, file);
    }
}
void PadComponent::itemDragEnter (const juce::DragAndDropTarget::SourceDetails& details) { isDraggingOver = true; repaint(); }
void PadComponent::itemDragExit (const juce::DragAndDropTarget::SourceDetails& details) { isDraggingOver = false; repaint(); }
