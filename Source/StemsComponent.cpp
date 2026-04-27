#include "StemsComponent.h"

StemsComponent::StemsComponent()
{
    // Setup VOCAL button (index 0)
    vocalBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x33228b22));
    vocalBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00ff00));
    vocalBtn.setClickingTogglesState(true);
    vocalBtn.setToggleState(false, juce::dontSendNotification);
    vocalBtn.onClick = [this] {
        bool muted = vocalBtn.getToggleState(); // ON = muted
        if (onStemMuteChanged) onStemMuteChanged(0, muted);
    };
    addAndMakeVisible(vocalBtn);

    // Setup DRUMS button (index 1)
    drumsBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x33b8860b));
    drumsBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffffd700));
    drumsBtn.setClickingTogglesState(true);
    drumsBtn.setToggleState(false, juce::dontSendNotification);
    drumsBtn.onClick = [this] {
        bool muted = drumsBtn.getToggleState();
        if (onStemMuteChanged) onStemMuteChanged(1, muted);
    };
    addAndMakeVisible(drumsBtn);

    // Setup BASS button (index 2)
    bassBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x338b0000));
    bassBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffff0000));
    bassBtn.setClickingTogglesState(true);
    bassBtn.setToggleState(false, juce::dontSendNotification);
    bassBtn.onClick = [this] {
        bool muted = bassBtn.getToggleState();
        if (onStemMuteChanged) onStemMuteChanged(2, muted);
    };
    addAndMakeVisible(bassBtn);
}

void StemsComponent::paint(juce::Graphics& g)
{
    // Fundo do painel transparente ou com slight dark
    juce::Colour baseColor = juce::Colour((juce::uint32)0x22000000);
    
    if (isDraggingOver)
        baseColor = juce::Colour((juce::uint32)0x5500d1b2); // Cyan highlight for drag
        
    g.fillAll(baseColor);
    
    if (loadedTrackName.isNotEmpty())
    {
        g.setColour(juce::Colours::white);
        g.setFont(20.0f);
        auto textRect = getLocalBounds().withSizeKeepingCentre(400, 30);
        g.drawText("Main Track: " + loadedTrackName, textRect, juce::Justification::centred, true);
    }
}

void StemsComponent::resized()
{
    auto area = getLocalBounds().reduced(10); // Margem externa

    int numButtons = 3;
    int spacing = 15; // Espaçamento entre botões
    int buttonWidth = (area.getWidth() - (spacing * (numButtons - 1))) / numButtons;

    vocalBtn.setBounds(area.removeFromLeft(buttonWidth));
    area.removeFromLeft(spacing);
    
    drumsBtn.setBounds(area.removeFromLeft(buttonWidth));
    area.removeFromLeft(spacing);
    
    bassBtn.setBounds(area.removeFromLeft(buttonWidth));
}

bool StemsComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto file : files) {
        if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".mp3"))
            return true;
    }
    return false;
}

void StemsComponent::filesDropped (const juce::StringArray& files, int, int)
{
    isDraggingOver = false;
    for (auto file : files) {
        if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".mp3")) {
            juce::File f(file);
            loadedTrackName = f.getFileName();
            // TODO: Load into engine
            break;
        }
    }
    repaint();
}

void StemsComponent::fileDragEnter (const juce::StringArray&, int, int)
{
    isDraggingOver = true;
    repaint();
}

void StemsComponent::fileDragExit (const juce::StringArray&)
{
    isDraggingOver = false;
    repaint();
}

bool StemsComponent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    return details.description.toString() == "AudioFileSelected";
}

void StemsComponent::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    isDraggingOver = false;
    if (details.sourceComponent != nullptr)
    {
        if (auto* tree = dynamic_cast<juce::FileTreeComponent*>(details.sourceComponent.get()))
        {
            auto selectedFile = tree->getSelectedFile();
            if (selectedFile.existsAsFile())
            {
                loadedTrackName = selectedFile.getFileName();
                // TODO: Load into engine (Stems separation)
            }
        }
    }
    repaint();
}

void StemsComponent::itemDragEnter (const juce::DragAndDropTarget::SourceDetails& details)
{
    if (details.description.toString() == "AudioFileSelected")
    {
        isDraggingOver = true;
        repaint();
    }
}

void StemsComponent::itemDragExit (const juce::DragAndDropTarget::SourceDetails& details)
{
    isDraggingOver = false;
    repaint();
}
