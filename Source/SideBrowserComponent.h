#pragma once

#include <JuceHeader.h>
#include <functional>

class SideBrowserComponent : public juce::Component,
                             private juce::Button::Listener
{
public:
    SideBrowserComponent();
    ~SideBrowserComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Called whenever the user clicks the side toggle arrow
    std::function<void(bool /*isExpanded*/)> onExpandedChanged;
    
    bool isExpanded() const { return expanded; }
    int getComponentWidth() const { return expanded ? 250 : 25; }

private:
    void buttonClicked (juce::Button* b) override;

    bool expanded = false; // Começa fechado por padrão (fina barra de 25px)
    juce::TextButton toggleBtn { ">" };

    juce::TimeSliceThread thread;
    juce::WildcardFileFilter fileFilter;
    juce::DirectoryContentsList directoryList;
    juce::FileTreeComponent fileTree;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SideBrowserComponent)
};
