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
            : name(fxName), index(slotIndex), hasDoubleControl(isDubEcho)
        {
            toggleBtn.setButtonText(name);
            toggleBtn.setClickingTogglesState(true);
            toggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff333333));
            toggleBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour((juce::uint32)0xff00aaaa));
            toggleBtn.onClick = [this] {
                if (onFxToggled) onFxToggled(index, toggleBtn.getToggleState());
            };
            addAndMakeVisible(toggleBtn);
            
            knob1.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob1.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            knob1.setRange(0.0, 1.0, 0.01);
            knob1.setValue(0.5);
            knob1.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00d1b2));
            knob1.onValueChange = [this] {
                if (onFxAmountChanged) onFxAmountChanged(index, (float)knob1.getValue());
            };
            addAndMakeVisible(knob1);

            if (hasDoubleControl) {
                knob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                knob2.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                knob2.setRange(0.0, 1.0, 0.01);
                knob2.setValue(0.5);
                knob2.setColour(juce::Slider::thumbColourId, juce::Colour(0xffff0000));
                addAndMakeVisible(knob2);
            }
        }

        void updateUI(float amt, bool on) {
            knob1.setValue(amt, juce::dontSendNotification);
            toggleBtn.setToggleState(on, juce::dontSendNotification);
        }

        void setRgbInfo(const juce::String& label, juce::Colour color) {
            rgbLabel = label;
            rgbColor = color;
            toggleBtn.setColour(juce::TextButton::textColourOffId, color.getBrightness() < 0.5f ? juce::Colours::white : juce::Colours::black);
            repaint();
        }

        void mouseDown(const juce::MouseEvent& e) override {
            if (e.mods.isRightButtonDown()) {
                if (onRightClick) onRightClick();
            } else {
                if (onLeftClick) onLeftClick();
            }
        }

        void resized() override {
            auto area = getLocalBounds().reduced(5);
            toggleBtn.setBounds(area.removeFromTop(30));
            if (hasDoubleControl) {
                auto w = area.getWidth() / 2;
                knob1.setBounds(area.removeFromLeft(w));
                knob2.setBounds(area);
            } else {
                knob1.setBounds(area);
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
            label.setBounds(area.removeFromLeft(60));
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
