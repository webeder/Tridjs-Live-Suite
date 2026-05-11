#pragma once

#include <JuceHeader.h>
#include <vector>

/**
 * ControllerManagerWindow
 * UI to manage Mixxx MIDI mappings (.xml and .js)
 */
class ControllerManagerWindow : public juce::DocumentWindow
{
public:
    class ContentComponent : public juce::Component, public juce::ListBoxModel
    {
    public:
        ContentComponent(const juce::File& mappingsDir, 
                        std::function<void(const juce::File&)> onActivate)
            : targetFolder(mappingsDir), onActivateCallback(onActivate)
        {
            listBox.setModel(this);
            addAndMakeVisible(listBox);
            
            importBtn.setButtonText("Importar Mapeamento (Mixxx XML)");
            importBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
            importBtn.onClick = [this] { importMapping(); };
            addAndMakeVisible(importBtn);
            
            activateBtn.setButtonText("Ativar Selecionada");
            activateBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::darkcyan);
            activateBtn.onClick = [this] {
                int row = listBox.getSelectedRow();
                if (row >= 0 && row < (int)mappingFiles.size())
                    onActivateCallback(mappingFiles[row]);
            };
            addAndMakeVisible(activateBtn);

            refreshList();
        }

        void refreshList()
        {
            mappingFiles = targetFolder.findChildFiles(juce::File::findFiles, false, "*.midi.xml");
            listBox.updateContent();
        }

        int getNumRows() override { return (int)mappingFiles.size(); }
        
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
        {
            if (rowIsSelected)
                g.fillAll(juce::Colours::cyan.withAlpha(0.2f));
                
            g.setColour(juce::Colours::white);
            g.setFont(14.0f);
            
            if (rowNumber < (int)mappingFiles.size())
                g.drawText(mappingFiles[rowNumber].getFileNameWithoutExtension().replace(".midi", ""), 
                           10, 0, width - 20, height, juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(10);
            auto bottom = area.removeFromBottom(100);
            importBtn.setBounds(bottom.removeFromTop(45).reduced(10, 5));
            activateBtn.setBounds(bottom.reduced(10, 5));
            listBox.setBounds(area);
        }

    private:
        void importMapping()
        {
            fileChooser = std::make_unique<juce::FileChooser>("Selecione o arquivo .midi.xml do Mixxx",
                                                              juce::File(), "*.midi.xml");
                                                              
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this](const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file.existsAsFile())
                    {
                        auto destXml = targetFolder.getChildFile(file.getFileName());
                        file.copyFileTo(destXml);
                        
                        auto jsFile = file.withFileExtension("js");
                        if (jsFile.existsAsFile())
                            jsFile.copyFileTo(targetFolder.getChildFile(jsFile.getFileName()));
                        else
                        {
                            auto jsAlt = file.getParentDirectory().getChildFile(file.getFileNameWithoutExtension() + "-script.js");
                            if (jsAlt.existsAsFile())
                                jsAlt.copyFileTo(targetFolder.getChildFile(jsAlt.getFileName()));
                        }
                        refreshList();
                    }
                });
        }

        juce::File targetFolder;
        juce::Array<juce::File> mappingFiles;
        std::function<void(const juce::File&)> onActivateCallback;
        juce::ListBox listBox;
        juce::TextButton importBtn, activateBtn;
        std::unique_ptr<juce::FileChooser> fileChooser;
    };

    ControllerManagerWindow(const juce::File& mappingsDir, 
                             std::function<void(const juce::File&)> onActivate)
        : DocumentWindow("Gerenciador de Controladoras",
                         juce::Colours::black,
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setResizable(false, false);
        setContentOwned(new ContentComponent(mappingsDir, onActivate), true);
        centreWithSize(400, 500);
    }

    void closeButtonPressed() override { setVisible(false); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControllerManagerWindow)
};
