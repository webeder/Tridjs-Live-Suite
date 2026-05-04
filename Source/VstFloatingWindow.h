#pragma once
#include <JuceHeader.h>
#include "VocalVstChain.h"
#include "PluginScannerManager.h"

class VstFloatingWindow : public juce::DocumentWindow,
                          public juce::ListBoxModel
{
public:
    VstFloatingWindow(VocalVstChain& chain, PluginScannerManager& manager)
        : DocumentWindow("FX: Vocal Chain", juce::Colour(0xff1a1a1a), allButtons),
          vstChain(chain), vstManager(manager)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        
        content.addAndMakeVisible(listBox);
        listBox.setModel(this);
        
        content.addAndMakeVisible(addBtn);
        addBtn.setButtonText("Add");
        addBtn.onClick = [this] { showAddMenu(); };

        content.addAndMakeVisible(removeBtn);
        removeBtn.setButtonText("Remove");
        removeBtn.onClick = [this] {
            int selected = listBox.getSelectedRow();
            if (selected >= 0)
            {
                // Must destroy the editor BEFORE removing the plugin to prevent crashes
                if (currentEditor != nullptr)
                {
                    content.removeChildComponent(currentEditor);
                    activeEditor.reset();
                    currentEditor = nullptr;
                    content.currentEditor = nullptr;
                }

                vstChain.removePlugin(selected);
                listBox.updateContent();
                
                // Auto-select the next available plugin and force the editor to update
                int nextSelected = std::min(selected, vstChain.getNumPlugins() - 1);
                if (nextSelected >= 0)
                {
                    listBox.selectRow(nextSelected, juce::dontSendNotification);
                    updateEditor();
                }
                else
                {
                    listBox.deselectAllRows();
                }
                
                resized();
            }
        };

        content.addAndMakeVisible(rescanBtn);
        rescanBtn.setButtonText("Rescan");
        rescanBtn.setTooltip("Scan computer for new VST/VST3 plugins");
        rescanBtn.onClick = [this] { 
            vstManager.rescan(); 
            // After rescan, we might want to automatically show the add menu if it was empty
        };

        setContentNonOwned(&content, false);
        setCentreRelative(0.5f, 0.5f);
        setSize(800, 500);
    }

    // ListBoxModel
    int getNumRows() override { return vstChain.getNumPlugins(); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected) g.fillAll(juce::Colours::cyan.withAlpha(0.2f));
        
        if (auto* p = vstChain.getPlugin(row))
        {
            bool isBypassed = p->isSuspended();
            
            // Checkbox rect
            juce::Rectangle<int> cbRect(5, (height - 14) / 2, 14, 14);
            g.setColour(isBypassed ? juce::Colours::grey : juce::Colours::cyan);
            g.drawRect(cbRect, 1.0f);
            if (!isBypassed)
                g.fillRect(cbRect.reduced(3));
                
            g.setColour(isBypassed ? juce::Colours::grey : juce::Colours::white);
            g.drawText(p->getName(), 25, 0, width - 30, height, juce::Justification::centredLeft);
        }
    }
    
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (e.x < 25)
        {
            if (auto* p = vstChain.getPlugin(row))
            {
                p->suspendProcessing(!p->isSuspended());
                listBox.repaintRow(row);
            }
        }
    }

    void selectedRowsChanged(int lastSelectedRow) override
    {
        updateEditor();
    }


    void updateEditor()
    {
        if (currentEditor != nullptr)
        {
            content.removeChildComponent(currentEditor);
            activeEditor.reset();
            currentEditor = nullptr;
            content.currentEditor = nullptr;
        }

        int row = listBox.getSelectedRow();
        
        if (auto* p = vstChain.getPlugin(row))
        {
            if (auto* editor = p->createEditorIfNeeded())
            {
                activeEditor.reset(editor);
                currentEditor = activeEditor.get();
                content.currentEditor = currentEditor;
                content.addAndMakeVisible(currentEditor);
            }
        }
        
        resized();
    }

    void showAddMenu()
    {
        juce::PopupMenu m;
        auto& list = vstManager.getKnownPluginList();
        auto types = list.getTypes();

        if (types.isEmpty())
        {
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,
                "Lista de Plugins Vazia",
                "Nenhum plugin foi encontrado. Deseja realizar um escaneamento agora?\n(Isso pode levar alguns minutos)",
                "Sim, Escanear", "Cancelar", nullptr,
                juce::ModalCallbackFunction::create([this](int result) {
                    if (result == 1) vstManager.rescan();
                }));
            return;
        }

        int i = 1;
        for (auto& desc : types)
        {
            m.addItem(i++, desc.name);
        }
        
        m.showMenuAsync(juce::PopupMenu::Options(), [this](int result) {
            if (result > 0)
            {
                auto& list = vstManager.getKnownPluginList();
                auto desc = list.getTypes()[result - 1];
                vstChain.addPlugin(desc);
                listBox.updateContent();
            }
        });
    }

    void closeButtonPressed() override { setVisible(false); }

    struct Content : public juce::Component
    {
        void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff1a1a1a)); }
        void resized() override
        {
            auto area = getLocalBounds();
            auto left = area.removeFromLeft(200);
            
            auto btns = left.removeFromBottom(40);
            int bw = btns.getWidth() / 3;
            addBtn->setBounds(btns.removeFromLeft(bw).reduced(2));
            removeBtn->setBounds(btns.removeFromLeft(bw).reduced(2));
            rescanBtn->setBounds(btns.reduced(2));
            
            listBox->setBounds(left);
            
            if (currentEditor != nullptr)
                currentEditor->setBounds(area);
        }
        
        juce::ListBox* listBox;
        juce::TextButton* addBtn;
        juce::TextButton* removeBtn;
        juce::TextButton* rescanBtn;
        juce::AudioProcessorEditor* currentEditor = nullptr;
    };

    void resized() override
    {
        content.listBox = &listBox;
        content.addBtn = &addBtn;
        content.removeBtn = &removeBtn;
        content.rescanBtn = &rescanBtn;
        content.currentEditor = currentEditor;
        DocumentWindow::resized();
        content.resized();
    }

private:
    VocalVstChain& vstChain;
    PluginScannerManager& vstManager;
    
    Content content;
    juce::ListBox listBox;
    juce::TextButton addBtn, removeBtn, rescanBtn;
    std::unique_ptr<juce::AudioProcessorEditor> activeEditor;
    juce::AudioProcessorEditor* currentEditor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VstFloatingWindow)
};
