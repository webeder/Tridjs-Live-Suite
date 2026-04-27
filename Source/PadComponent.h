#pragma once

#include <JuceHeader.h>

class PadComponent : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     public juce::DragAndDropTarget
{
public:
    PadComponent (int padIndex);
    ~PadComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    std::function<void()> onLeftClick;
    std::function<void()> onRightClick;

    std::function<void(int padIndex, const juce::File& file)> onFileDropped;
    std::function<void(int padIndex, bool isPlaying)> onPlayStateChanged;
    std::function<void(int padIndex, bool isLooping)> onLoopToggled;
    std::function<void(int padIndex)> onEjectRequested;
    std::function<void(int padIndex)> onRecordRequested;
    std::function<void(int padIndex, float gain)> onVolumeChanged;

    void setLoadedFile (const juce::File& file);
    juce::String getLoadedFilePath() const { return currentFilePath; }
    void setInputLevel (float level) { lastInputLevel = level; }
    bool isRecordingMode() const { return isRecording; }

    void setRgbInfo(const juce::String& label, juce::Colour color);

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;

    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragEnter (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit (const juce::DragAndDropTarget::SourceDetails& details) override;

private:
    int index;
    bool isDraggingOver = false;
    bool isMouseDown = false;
    bool isActive = false;
    bool isRecording = false;
    bool isLoopActive = false;
    juce::String loadedFilename;
    juce::String currentFilePath;

    juce::ImageButton recordBtn, ejectBtn, loopBtn;
    juce::Image micIcon, loopIcon, ejectIcon;
    juce::Colour customBaseColor { (juce::uint32)0xff1a1a1a };
    juce::Slider volumeSlider;
    float playProgress = 0.0f;
    float lastInputLevel = 0.0f;
    juce::Colour currentRgbColor = juce::Colours::transparentBlack;
    juce::String rgbLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadComponent)
};
