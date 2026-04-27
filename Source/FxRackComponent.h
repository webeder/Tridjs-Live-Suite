#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <functional>

// Componente XY Pad Profissional com Mola e RGB Serial
class FX_Rack_Alpha : public juce::Component, private juce::Timer
{
public:
    enum Mode { Ladder, UltraMulti };

    FX_Rack_Alpha() {
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    }
    
    void setMode(Mode m) { 
        mode = m; 
        curX = 0.5f; 
        curY = 0.5f; 
        updateRgb();
        repaint(); 
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        
        // Fundo e Grade
        g.fillAll(juce::Colour(0xff0a0a0a));
        g.setColour(juce::Colour(0xff1a1a1a));
        for (float x = 0; x < getWidth(); x += getWidth() / 10.0f) g.drawVerticalLine((int)x, 0.0f, (float)getHeight());
        for (float y = 0; y < getHeight(); y += getHeight() / 10.0f) g.drawHorizontalLine((int)y, 0.0f, (float)getWidth());

        // Bolinha Neon
        float bx = curX * getWidth();
        float by = (1.0f - curY) * getHeight();
        float size = 22.0f;

        // Glow
        for (int i = 6; i > 0; --i) {
            g.setColour(juce::Colour(0xffd4ff00).withAlpha(0.04f * i));
            g.fillEllipse(bx - (size + i * 10) / 2, by - (size + i * 10) / 2, size + i * 10, size + i * 10);
        }

        g.setColour(juce::Colour(0xffd4ff00)); 
        g.fillEllipse(bx - size / 2, by - size / 2, size, size);
        
        // Labels de Eixos
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.setFont(14.0f);
        
        if (mode == Ladder) {
            g.drawText("CUTOFF", 10, getHeight()/2 - 10, getWidth() - 20, 20, juce::Justification::left);
            g.drawText("RESONANCE", getWidth()/2 - 50, 10, 100, 20, juce::Justification::centred);
        } else {
            if (curX < 0.45f) g.drawText("FLANGER", 10, getHeight()/2 - 10, 80, 20, juce::Justification::left);
            if (curX > 0.55f) g.drawText("ECHO", getWidth() - 90, getHeight()/2 - 10, 80, 20, juce::Justification::right);
            if (curY > 0.55f) g.drawText("HP FILTER", getWidth()/2 - 40, 10, 80, 20, juce::Justification::centred);
            if (curY < 0.45f) g.drawText("LP FILTER", getWidth()/2 - 40, getHeight() - 30, 80, 20, juce::Justification::centred);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override { 
        stopTimer(); 
        updatePos(e); 
        if (onActiveChanged) onActiveChanged(true); 
    }
    void mouseDrag(const juce::MouseEvent& e) override { updatePos(e); }
    void mouseUp(const juce::MouseEvent&) override { startTimer(16); } 

    void timerCallback() override {
        // Interpolação suave para o centro (0.5, 0.5)
        curX += (0.5f - curX) * 0.2f;
        curY += (0.5f - curY) * 0.2f;
        
        if (std::abs(curX - 0.5f) < 0.005f && std::abs(curY - 0.5f) < 0.005f) {
            curX = 0.5f; curY = 0.5f;
            stopTimer();
            if (onActiveChanged) onActiveChanged(false);
        }
        
        if (onXyChanged) onXyChanged(curX, curY);
        repaint();
    }

    void updatePos(const juce::MouseEvent& e) {
        curX = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
        curY = juce::jlimit(0.0f, 1.0f, 1.0f - (float)e.y / getHeight());
        if (onXyChanged) onXyChanged(curX, curY);
        updateRgb();
        repaint();
    }

    void updateRgb() {
        int r = 0, g = 0, b = 0;
        if (mode == Ladder) {
            r = 0; g = (int)(curX * 255); b = 255; // Azul p/ Ciano
        } else {
            if (curX < 0.48f) g = (int)((0.5f - curX) * 2.0f * 255);
            else if (curX > 0.52f) r = (int)((curX - 0.5f) * 2.0f * 255);
            b = (int)(std::abs(curY - 0.5f) * 2.0f * 255);
        }

        // Threshold de 3 unidades para evitar spam serial
        if (std::abs(r - lastR) > 3 || std::abs(g - lastG) > 3 || std::abs(b - lastB) > 3) {
            lastR = r; lastG = g; lastB = b;
            if (onSerialRgbRequested) onSerialRgbRequested(r, g, b);
        }
    }

    std::function<void(float, float)> onXyChanged;
    std::function<void(bool)> onActiveChanged;
    std::function<void(int, int, int)> onSerialRgbRequested;

private:
    float curX = 0.5f, curY = 0.5f;
    Mode mode = UltraMulti;
    int lastR = -10, lastG = -10, lastB = -10;
};

// Container para a aba FX TOUCH
class FxTouchTabContent : public juce::Component
{
public:
    FxTouchTabContent() { 
        addAndMakeVisible(touchPad); 
        addAndMakeVisible(modeBtn);
        modeBtn.setButtonText("ULTRA MULTI-FX");
        modeBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd4ff00));
        modeBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    }
    
    void resized() override { 
        auto area = getLocalBounds();
        modeBtn.setBounds(area.removeFromTop(30).reduced(80, 2));
        touchPad.setBounds(area.reduced(10)); 
    }
    
    FX_Rack_Alpha touchPad;
    juce::TextButton modeBtn;
};

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

    // XY Pad & FX Touch
    FxTouchTabContent touchTabContent;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxRackComponent)
};
