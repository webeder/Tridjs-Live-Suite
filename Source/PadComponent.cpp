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
        repaint();
    };
    addAndMakeVisible (loopBtn);

    ejectBtn.setImages(false, true, true, ejectIcon, 1.0f, {}, ejectIcon, 1.0f, juce::Colours::white.withAlpha(0.2f), ejectIcon, 1.0f, juce::Colours::lightgrey.withAlpha(0.6f));
    ejectBtn.onClick = [this] {
        loadedFilename = ""; isActive = false; isRecording = false;
        if (onEjectRequested) onEjectRequested(index);
        repaint();
    };
    addAndMakeVisible (ejectBtn);

    volumeSlider.setSliderStyle(juce::Slider::LinearBarVertical);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setColour(juce::Slider::trackColourId, juce::Colours::cyan.withAlpha(0.3f));
    volumeSlider.onValueChange = [this] { if (onVolumeChanged) onVolumeChanged(index, (float)volumeSlider.getValue()); };
    addAndMakeVisible(volumeSlider);
}

void PadComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto cornerSize = 12.0f;
    juce::Colour baseColour = customBaseColor;
    if (isDraggingOver) baseColour = baseColour.brighter(0.2f);
    else if (isMouseDown) baseColour = baseColour.darker(0.2f);
    
    if (isRecording) {
        float blink = (float)std::abs(std::sin(juce::Time::getMillisecondCounterHiRes() * 0.008));
        baseColour = juce::Colour (0xffff0000).withAlpha(0.1f + 0.3f * blink);
        g.setColour (juce::Colour (0xffff0000).withAlpha(0.4f * blink)); 
        g.fillRoundedRectangle (area.expanded (2.0f + 2.0f * blink), cornerSize + 2.0f);
    } 
    else if (isActive) {
        baseColour = juce::Colour (0xff00d1b2).withAlpha(0.6f);
        g.setColour (baseColour.withAlpha(0.3f)); 
        g.fillRoundedRectangle (area.expanded (3.0f), cornerSize + 3.0f);
    }

    g.setColour (baseColour);
    g.fillRoundedRectangle (area, cornerSize);
    g.setColour (juce::Colours::grey.withAlpha(0.3f));
    g.drawRoundedRectangle (area.reduced (1.0f), cornerSize, 1.5f);

    if (currentRgbColor != juce::Colours::transparentBlack) {
        auto rgbArea = area.removeFromTop(16).removeFromLeft(60).translated(4, 4);
        g.setColour(currentRgbColor.withAlpha(0.7f));
        g.fillRoundedRectangle(rgbArea, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(rgbLabel, rgbArea, juce::Justification::centred);
    }

    if (loadedFilename.isNotEmpty()) {
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(12.0f));
        g.drawText(loadedFilename, getLocalBounds().reduced(5, 40), juce::Justification::centred);
    }
}

void PadComponent::resized()
{
    auto area = getLocalBounds().reduced(5);
    auto bottom = area.removeFromBottom(25);
    recordBtn.setBounds(bottom.removeFromLeft(25));
    loopBtn.setBounds(bottom.removeFromLeft(25));
    ejectBtn.setBounds(bottom.removeFromLeft(25));
    volumeSlider.setBounds(area.removeFromRight(15).reduced(2));
}

void PadComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) { if (onRightClick) onRightClick(); return; }
    
    if (onLeftClick) onLeftClick();

    isMouseDown = true;
    isActive = !isActive;
    if (onPlayStateChanged) onPlayStateChanged (index, isActive);
    repaint();
}

void PadComponent::mouseUp (const juce::MouseEvent& e)
{
    isMouseDown = false;
    repaint();
}

void PadComponent::setLoadedFile (const juce::File& file) { loadedFilename = file.getFileName(); currentFilePath = file.getFullPathName(); repaint(); }
void PadComponent::setRgbInfo(const juce::String& label, juce::Colour color) { rgbLabel = label; currentRgbColor = color; repaint(); }

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
