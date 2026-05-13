#pragma once

#include <JuceHeader.h>
#include "LanguageManager.h"
#include <functional>

class StemsComponent : public juce::Component,
                       public juce::FileDragAndDropTarget,
                       public juce::DragAndDropTarget,
                       public juce::ChangeListener
{
public:
    StemsComponent();
    ~StemsComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void updateLanguage();

    // Callbacks for stem mute/unmute: index 0=vocal, 1=drums, 2=bass
    std::function<void(int, bool)> onStemMuteChanged;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;

    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragEnter (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit (const juce::DragAndDropTarget::SourceDetails& details) override;

private:
    bool isDraggingOver = false;
    juce::String loadedTrackName;
    juce::TextButton vocalBtn { "" };
    juce::TextButton drumsBtn { "" };
    juce::TextButton bassBtn  { "" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemsComponent)
};
