#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <functional>

class FxRackComponent : public juce::Component
{
public:
    FxRackComponent(juce::AudioDeviceManager& deviceManager);
    ~FxRackComponent() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    
    bool isRgbTabActive() const;
    juce::Colour getSelectedColor() const;
    
    // MIDI Learn access
    void updateMappingDisplay(int rowIdx, const juce::String& text);
    int getLearningRowIndex() const;
    juce::String getCurrentDeviceName() const;

    void updateFxDisplay (int fxIndex, float amount, bool enabled);
    void appendSerialLog(const juce::String& text);
    void setInputMode(int mode);
    void setSerialPort(const juce::String& port);
    
    bool isExpanded() const { return expanded; }
    void setExpanded(bool shouldExpand);
    std::function<void(bool)> onExpandedChanged;

    // Public callbacks
    std::function<void(int, bool)> onFxToggled;
    std::function<void(int, float)> onFxAmountChanged;
    std::function<void(int)> onMidiDeviceIndexChanged;
    std::function<void(int)> onInputModeChanged;
    std::function<void(const juce::String&)> onSerialPortChanged;
    std::function<void(int, const juce::String&)> onMappingUpdated;
    std::function<void()> onSaveMappingRequested;
    std::function<void()> onOpenMappingRequested;

    // RGB Callbacks
    std::function<void(const juce::String&)> onSaveGlobalPreset;
    std::function<void(int)> onLoadGlobalPreset;
    std::function<void(int)> onDeleteGlobalPreset;
    std::function<void(int, int)> onAssignRgbRequested; // targetType (0=Pad, 1=FX), targetIdx
    std::function<void(int, int)> onClearRgbRequested; // targetType, targetIdx

    class FxSlot : public juce::Component
    {
    public:
    FxSlot(const juce::String& fxName, int slotIndex, bool isDubEcho = false)
            : name(fxName.toUpperCase()), index(slotIndex), hasDoubleControl(isDubEcho)
        {
            // O botão invisível agora cobre o fundo para o clique de ativação
            toggleBtn.setButtonText("");
            toggleBtn.setClickingTogglesState(true);
            toggleBtn.setAlpha(0.0f); // Invisível mas funcional
            toggleBtn.onClick = [this] {
                if (onFxToggled) onFxToggled(index, toggleBtn.getToggleState());
                repaint();
            };
            addAndMakeVisible(toggleBtn);
            
            knob1.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob1.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            knob1.setRange(0.0, 1.0, 0.01);
            knob1.setValue(0.5);
            knob1.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::cyan);
            knob1.onValueChange = [this] {
                if (onFxAmountChanged) onFxAmountChanged(index, (float)knob1.getValue());
            };
            addAndMakeVisible(knob1);

            if (hasDoubleControl) {
                knob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                knob2.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                knob2.setRange(0.0, 1.0, 0.01);
                knob2.setValue(0.5);
                knob2.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
                addAndMakeVisible(knob2);
            }
        }

        void updateUI(float amt, bool on) {
            knob1.setValue(amt, juce::dontSendNotification);
            toggleBtn.setToggleState(on, juce::dontSendNotification);
            repaint();
        }

        void setRgbInfo(const juce::String& label, juce::Colour color) {
            rgbLabel = label;
            rgbColor = color;
            repaint();
        }

        void paint(juce::Graphics& g) override {
            auto area = getLocalBounds().reduced(2).toFloat();
            bool isOn = toggleBtn.getToggleState();
            
            // Fundo do Pad (estilo imagem 2)
            g.setColour(juce::Colour(0xff121212));
            g.fillRoundedRectangle(area, 10.0f);
            
            // Borda de ativação (Neon Glow)
            if (isOn) {
                g.setColour(juce::Colours::cyan.withAlpha(0.8f));
                g.drawRoundedRectangle(area, 10.0f, 2.0f);
                
                // Glow interno leve
                g.setGradientFill(juce::ColourGradient(juce::Colours::cyan.withAlpha(0.15f), area.getCentreX(), area.getCentreY(),
                                                      juce::Colours::transparentBlack, area.getWidth(), area.getHeight(), true));
                g.fillRoundedRectangle(area, 10.0f);
            } else {
                g.setColour(juce::Colours::white.withAlpha(0.1f));
                g.drawRoundedRectangle(area, 10.0f, 1.0f);
            }

            // Nome do Efeito no Topo
            g.setColour(isOn ? juce::Colours::white : juce::Colours::lightgrey);
            g.setFont(juce::Font("Roboto", 14.0f, juce::Font::bold));
            g.drawText(name, area.removeFromTop(40).withTrimmedTop(10), juce::Justification::centred);
            
            // Indicador RGB se houver
            if (rgbColor != juce::Colours::transparentBlack) {
                g.setColour(rgbColor);
                g.fillRoundedRectangle(area.getRight() - 25, area.getBottom() - 25, 15, 15, 3.0f);
            }
        }

        void mouseDown(const juce::MouseEvent& e) override {
            if (e.mods.isRightButtonDown()) {
                if (onRightClick) onRightClick();
            } else {
                if (onLeftClick) onLeftClick();
            }
        }

        void resized() override {
            auto area = getLocalBounds();
            toggleBtn.setBounds(area); // Cobre tudo para clique
            
            area.removeFromTop(40); // Espaço do nome
            auto knobArea = area.reduced(15);
            
            if (hasDoubleControl) {
                auto left = knobArea.removeFromLeft(knobArea.getWidth() / 2);
                knob1.setBounds(left.reduced(5));
                knob2.setBounds(knobArea.reduced(5));
            } else {
                knob1.setBounds(knobArea);
            }
        }
        
        std::function<void(int, bool)> onFxToggled;
        std::function<void(int, float)> onFxAmountChanged;
        std::function<void()> onLeftClick;
        std::function<void()> onRightClick;
        
    private:
        juce::String name;
        int index;
        bool hasDoubleControl;
        juce::TextButton toggleBtn;
        juce::Slider knob1;
        juce::Slider knob2;
        juce::String rgbLabel;
        juce::Colour rgbColor = juce::Colours::transparentBlack;
    };

    class MidiMappingRow : public juce::Component
    {
    public:
        MidiMappingRow(const juce::String& lblName) {
            label.setText(lblName, juce::dontSendNotification);
            valueBox.setText("---", juce::dontSendNotification);
            valueBox.setEditable(true, true, true);
            label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            valueBox.setColour(juce::Label::textColourId, juce::Colours::cyan);
            valueBox.setJustificationType(juce::Justification::centred);
            learnBtn.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff333333));
            learnBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
            learnBtn.setClickingTogglesState(true);
            learnBtn.onClick = [this] { if (onLearnToggled) onLearnToggled(learnBtn.getToggleState()); };
            valueBox.onTextChange = [this] { if (onManualEntry) onManualEntry(valueBox.getText()); };
            clearBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentWhite);
            clearBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::red.withAlpha(0.6f));
            clearBtn.onClick = [this] { if (onClear) onClear(); };
            addAndMakeVisible(label); addAndMakeVisible(valueBox); addAndMakeVisible(learnBtn); addAndMakeVisible(clearBtn);
        }
        void setMappingValue(const juce::String& text) { valueBox.setText(text, juce::dontSendNotification); }
        void setLearnState(bool isOn) { learnBtn.setToggleState(isOn, juce::dontSendNotification); }
        bool isLearning() const { return learnBtn.getToggleState(); }
        void resized() override {
            auto area = getLocalBounds().reduced(0, 2);
            label.setBounds(area.removeFromLeft(120)); // Aumentado para não esmagar o texto
            clearBtn.setBounds(area.removeFromRight(20).reduced(0, 5));
            learnBtn.setBounds(area.removeFromRight(50).reduced(0, 2));
            valueBox.setBounds(area);
        }
        std::function<void(bool)> onLearnToggled;
        std::function<void(const juce::String&)> onManualEntry;
        std::function<void()> onClear;
    private:
        juce::Label label; juce::Label valueBox;
        juce::TextButton learnBtn { "LEARN" }; juce::TextButton clearBtn { "X" };
    };

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::TextButton toggleBtn { "<" };
    bool expanded = true;
    juce::Component effectsContent;
    juce::Component rgbContent;
    juce::ColourSelector colorSelector;
    juce::ListBox lightingPaletteList;
    juce::ListBox globalPresetList;
    juce::TextButton saveGlobalPresetBtn { "SAVE SESSION" };
    juce::TextButton loadGlobalPresetBtn { "LOAD" };
    juce::TextButton deleteGlobalPresetBtn { "DELETE" };
    juce::TextButton removeRgbBrushBtn { "REMOVE EFFECT" };
    juce::TextEditor presetNameEdit;
    juce::Label presetNameLabel { {}, "SESSION NAME:" };
    juce::Label midiDeviceLabel;
    juce::ComboBox commandCombo;
    juce::Viewport targetViewport;
    juce::Component targetListContent;
    std::vector<std::unique_ptr<juce::TextButton>> fxTargetBtns;
    juce::Component midiLearnContent;
    juce::ComboBox deviceManagerCombo;
    juce::ImageButton saveMidiBtn, openMidiBtn;
    juce::Image diskIcon, openIcon;
    juce::Viewport mappingViewport;
    juce::Component mappingListContent;
    std::vector<std::unique_ptr<MidiMappingRow>> midiRows;
    std::vector<std::unique_ptr<FxSlot>> fxSlots;
    juce::Component serialContent;
    juce::ComboBox inputModeCombo, serialPortCombo;
    juce::TextEditor serialLog;
    juce::Label inputModeLabel { {}, "INPUT MODE" };
    juce::Label serialPortLabel { {}, "SERIAL PORT (115200)" };
    juce::Component configContent;
    juce::ImageButton saveConfigBtn, openConfigBtn;
    juce::Image configIcon;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;

    struct PaletteListModel : public juce::ListBoxModel {
        PaletteListModel(FxRackComponent& p) : owner(p) {}
        int getNumRows() override { return (int)items.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override {
            if (selected) g.fillAll(juce::Colours::cyan.withAlpha(0.3f));
            g.setColour(juce::Colours::white);
            g.drawText(items[row], 5, 0, width, height, juce::Justification::centredLeft);
        }
        FxRackComponent& owner; juce::StringArray items;
    } paletteListModel { *this };

    struct GlobalPresetListModel : public juce::ListBoxModel {
        GlobalPresetListModel(FxRackComponent& p) : owner(p) {}
        int getNumRows() override { return (int)items.size(); }
        void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override {
            if (selected) g.fillAll(juce::Colours::orange.withAlpha(0.3f));
            g.setColour(juce::Colours::white);
            g.drawText(items[row], 5, 0, width, height, juce::Justification::centredLeft);
        }
        FxRackComponent& owner; juce::StringArray items;
    } globalPresetListModel { *this };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxRackComponent)
};
