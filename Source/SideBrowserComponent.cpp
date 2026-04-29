#include "SideBrowserComponent.h"

SideBrowserComponent::SideBrowserComponent()
    : thread ("FileBrowserThread"),
      fileFilter ("*.wav;*.mp3", "*", "Audio Files"),
      directoryList (&fileFilter, thread),
      fileTree (directoryList)
{
    thread.startThread (juce::Thread::Priority::normal);

    addAndMakeVisible (toggleBtn);
    toggleBtn.addListener (this);
    toggleBtn.setColour (juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
    toggleBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::lightgrey);

    addAndMakeVisible (fileTree);
    fileTree.setColour(juce::TreeView::backgroundColourId, juce::Colour ((juce::uint32)0xff1e1e1e));

    // Carrega a raiz do disco C: para evitar a lista vazia
    directoryList.setDirectory (juce::File("C:\\"), true, true);

    // Permite que o FileTree dispare eventos de "Drag" para arquivos selecionados nele
    fileTree.setDragAndDropDescription("AudioFileSelected");
}

SideBrowserComponent::~SideBrowserComponent()
{
    toggleBtn.removeListener (this);
    thread.stopThread (2000);
}

void SideBrowserComponent::buttonClicked (juce::Button* b)
{
    if (b == &toggleBtn)
    {
        setExpanded(!expanded);
    }
}

void SideBrowserComponent::setExpanded(bool shouldExpand)
{
    if (expanded == shouldExpand) return;
    expanded = shouldExpand;
    toggleBtn.setButtonText (expanded ? "<" : ">");
    
    if (onExpandedChanged)
        onExpandedChanged (expanded);
        
    resized();
}

void SideBrowserComponent::paint (juce::Graphics& g)
{
    // Fundo global do painel
    g.fillAll (juce::Colour ((juce::uint32)0xff1a1a1a)); 
    
    // Divisória sutil na margem direita para separar da engine de pads
    g.setColour (juce::Colour(0xff333333));
    g.drawLine((float)getWidth(), 0.0f, (float)getWidth(), (float)getHeight(), 1.5f);
}

void SideBrowserComponent::resized()
{
    auto area = getLocalBounds();
    
    // O botão expansor ocupa toda altura lateral (no lado DIREITO para expandir à direita)
    auto buttonArea = area.removeFromRight (expanded ? 18 : getWidth());
    toggleBtn.setBounds (buttonArea);

    if (expanded)
    {
        // Se a Sidebar estiver aberta, a árvore engole o espaço remanescente
        fileTree.setBounds (area);
        fileTree.setVisible (true);
    }
    else
    {
        fileTree.setVisible (false);
    }
}
