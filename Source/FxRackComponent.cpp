#include "FxRackComponent.h"
#include "SerialManager.h"

FxRackComponent::FxRackComponent(juce::AudioDeviceManager& deviceManager)
{
    // Setup Tabs
    addAndMakeVisible(tabs);
    tabs.addTab("EFFECTS", juce::Colours::transparentBlack, &effectsContent, false);
    tabs.addTab("RGB", juce::Colours::transparentBlack, &rgbContent, false);
    tabs.addTab("LEARN", juce::Colours::transparentBlack, &midiLearnContent, false);
    tabs.addTab("SERIAL", juce::Colours::transparentBlack, &serialContent, false);
    tabs.addTab("CONFIG", juce::Colours::transparentBlack, &configContent, false);

    addAndMakeVisible(toggleBtn);
    toggleBtn.onClick = [this] { setExpanded(!expanded); };

    diskIcon = juce::ImageFileFormat::loadFrom(juce::File("C:\\TridjsMIDI\\disk.png"));
    openIcon = juce::ImageFileFormat::loadFrom(juce::File("C:\\TridjsMIDI\\open.png"));

    // ===== CONFIG TAB =====
    configIcon = juce::ImageFileFormat::loadFrom(juce::File("C:\\TridjsMIDI\\config.png"));
    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
        deviceManager, 0, 2, 2, 2, true, true, true, true);
    configContent.addAndMakeVisible(*deviceSelector);

    // ===== FX SLOTS (EFFECTS TAB) =====
    juce::StringArray fxNames = { "Delay", "Echo", "Reverb", "Flange", "Space", "Dub Echo" };
    for (int i = 0; i < fxNames.size(); ++i) {
        auto slot = std::make_unique<FxSlot>(fxNames[i], i, i == 5);
        slot->onFxToggled = [this](int idx, bool on) { if (onFxToggled) onFxToggled(idx, on); };
        slot->onFxAmountChanged = [this](int idx, float amt) { if (onFxAmountChanged) onFxAmountChanged(idx, amt); };
        slot->onLeftClick = [this, i] { if (onAssignRgbRequested) onAssignRgbRequested(1, i); };
        slot->onRightClick = [this, i] { if (onClearRgbRequested) onClearRgbRequested(1, i); };
        effectsContent.addAndMakeVisible(*slot);
        fxSlots.push_back(std::move(slot));
    }

    // ===== RGB TAB =====
    colorSelector.setName("RGB Color Selector");
    colorSelector.setColour(juce::ColourSelector::backgroundColourId, juce::Colours::transparentBlack);
    rgbContent.addAndMakeVisible(colorSelector);

    lightingPaletteList.setModel(&paletteListModel);
    lightingPaletteList.setColour(juce::ListBox::backgroundColourId, juce::Colours::black.withAlpha(0.2f));
    rgbContent.addAndMakeVisible(lightingPaletteList);

    globalPresetList.setModel(&globalPresetListModel);
    globalPresetList.setColour(juce::ListBox::backgroundColourId, juce::Colours::black.withAlpha(0.2f));
    rgbContent.addAndMakeVisible(globalPresetList);

    rgbContent.addAndMakeVisible(presetNameLabel);
    rgbContent.addAndMakeVisible(presetNameEdit);

    saveGlobalPresetBtn.onClick = [this] { if (onSaveGlobalPreset) onSaveGlobalPreset(presetNameEdit.getText()); };
    rgbContent.addAndMakeVisible(saveGlobalPresetBtn);

    loadGlobalPresetBtn.onClick = [this] { if (onLoadGlobalPreset) onLoadGlobalPreset(globalPresetList.getSelectedRow()); };
    rgbContent.addAndMakeVisible(loadGlobalPresetBtn);

    deleteGlobalPresetBtn.onClick = [this] { if (onDeleteGlobalPreset) onDeleteGlobalPreset(globalPresetList.getSelectedRow()); };
    rgbContent.addAndMakeVisible(deleteGlobalPresetBtn);

    removeRgbBrushBtn.onClick = [this] { lightingPaletteList.deselectAllRows(); };
    rgbContent.addAndMakeVisible(removeRgbBrushBtn);

    // FX Target buttons inside RGB tab
    for (int i = 0; i < fxNames.size(); ++i) {
        auto btn = std::make_unique<juce::TextButton>(fxNames[i]);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colours::cyan.withAlpha(0.1f));
        btn->onClick = [this, i] { if (onAssignRgbRequested) onAssignRgbRequested(1, i); };
        targetListContent.addAndMakeVisible(*btn);
        fxTargetBtns.push_back(std::move(btn));
    }
    targetViewport.setViewedComponent(&targetListContent, false);
    rgbContent.addAndMakeVisible(targetViewport);

    // ===== MIDI LEARN TAB =====
    juce::StringArray rowNames;
    for (int i = 0; i < 9; ++i)  rowNames.add("PAD " + juce::String(i + 1));
    for (int i = 0; i < 6; ++i)  rowNames.add(fxNames[i]);

    for (int i = 0; i < rowNames.size(); ++i) {
        auto row = std::make_unique<MidiMappingRow>(rowNames[i]);
        int localIdx = i;
        row->onLearnToggled = [this, localIdx](bool isOn) {
            // Turn off all others when one is toggled on
            if (isOn) {
                for (int j = 0; j < (int)midiRows.size(); ++j) {
                    if (j != localIdx) midiRows[j]->setLearnState(false);
                }
            }
        };
        row->onManualEntry = [this, localIdx](const juce::String& text) {
            if (onMappingUpdated) onMappingUpdated(localIdx, text);
        };
        row->onClear = [this, localIdx] {
            midiRows[localIdx]->setMappingValue("---");
            if (onMappingUpdated) onMappingUpdated(localIdx, "");
        };
        mappingListContent.addAndMakeVisible(*row);
        midiRows.push_back(std::move(row));
    }

    mappingViewport.setViewedComponent(&mappingListContent, false);
    midiLearnContent.addAndMakeVisible(mappingViewport);

    // MIDI Device selector
    deviceManagerCombo.addItem("-- Select MIDI --", 1);
    auto midiDevices = juce::MidiInput::getAvailableDevices();
    for (int i = 0; i < midiDevices.size(); ++i)
        deviceManagerCombo.addItem(midiDevices[i].name, i + 2);
    deviceManagerCombo.setSelectedId(1);
    deviceManagerCombo.onChange = [this] {
        if (onMidiDeviceIndexChanged) onMidiDeviceIndexChanged(deviceManagerCombo.getSelectedId() - 1);
    };
    midiLearnContent.addAndMakeVisible(deviceManagerCombo);

    saveMidiBtn.setImages(false, true, true, diskIcon, 1.0f, {}, diskIcon, 1.0f, juce::Colours::white.withAlpha(0.2f), diskIcon, 1.0f, juce::Colours::cyan.withAlpha(0.2f));
    saveMidiBtn.onClick = [this] { if (onSaveMappingRequested) onSaveMappingRequested(); };
    midiLearnContent.addAndMakeVisible(saveMidiBtn);

    openMidiBtn.setImages(false, true, true, openIcon, 1.0f, {}, openIcon, 1.0f, juce::Colours::white.withAlpha(0.2f), openIcon, 1.0f, juce::Colours::cyan.withAlpha(0.2f));
    openMidiBtn.onClick = [this] { if (onOpenMappingRequested) onOpenMappingRequested(); };
    midiLearnContent.addAndMakeVisible(openMidiBtn);

    // ===== SERIAL TAB =====
    inputModeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    serialContent.addAndMakeVisible(inputModeLabel);

    inputModeCombo.addItem("MIDI", 1);
    inputModeCombo.addItem("SERIAL", 2);
    inputModeCombo.setSelectedId(1);
    inputModeCombo.onChange = [this] {
        if (onInputModeChanged) onInputModeChanged(inputModeCombo.getSelectedId() - 1);
    };
    serialContent.addAndMakeVisible(inputModeCombo);

    serialPortLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    serialContent.addAndMakeVisible(serialPortLabel);

    auto ports = SerialManager::listAvailablePorts();
    serialPortCombo.addItem("-- Select Port --", 1);
    for (int i = 0; i < ports.size(); ++i)
        serialPortCombo.addItem(ports[i], i + 2);
    serialPortCombo.setSelectedId(1);
    serialPortCombo.onChange = [this] {
        if (serialPortCombo.getSelectedId() > 1 && onSerialPortChanged)
            onSerialPortChanged(serialPortCombo.getText());
    };
    serialContent.addAndMakeVisible(serialPortCombo);

    serialLog.setMultiLine(true);
    serialLog.setReadOnly(true);
    serialLog.setColour(juce::TextEditor::backgroundColourId, juce::Colour((juce::uint32)0xff0a0a0a));
    serialLog.setColour(juce::TextEditor::textColourId, juce::Colours::lime);
    serialLog.setFont(juce::Font("Consolas", 11.0f, juce::Font::plain));
    serialContent.addAndMakeVisible(serialLog);
}

void FxRackComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour ((juce::uint32)0xff1a1a1a));
    
    // Divisória sutil na margem esquerda (lado dos pads)
    g.setColour (juce::Colour(0xff333333));
    g.drawLine(0.0f, 0.0f, 0.0f, (float)getHeight(), 1.5f);
}

void FxRackComponent::resized()
{
    auto area = getLocalBounds();
    toggleBtn.setBounds(area.removeFromLeft(20).withSizeKeepingCentre(20, 60));
    
    if (!expanded) {
        tabs.setVisible(false);
        return;
    }
    tabs.setVisible(true);
    tabs.setBounds(area);

    // ===== EFFECTS TAB =====
    auto fxArea = effectsContent.getLocalBounds().reduced(10);
    int slotHeight = fxArea.getHeight() / 6;
    for (auto& slot : fxSlots) slot->setBounds(fxArea.removeFromTop(slotHeight));

    // ===== RGB TAB (Clean Grid Layout) =====
    auto rgbArea = rgbContent.getLocalBounds().reduced(8);
    
    // Top Section: Color Selector and Palette List
    auto topRgb = rgbArea.removeFromTop(180);
    colorSelector.setBounds(topRgb.removeFromLeft(topRgb.getWidth() / 2).reduced(2));
    lightingPaletteList.setBounds(topRgb.reduced(2));

    rgbArea.removeFromTop(10); // spacing

    // Middle Section: Session Management
    auto midRgb = rgbArea.removeFromTop(160);
    auto leftMid = midRgb.removeFromLeft(midRgb.getWidth() / 2).reduced(2);
    
    presetNameLabel.setBounds(leftMid.removeFromTop(20));
    presetNameEdit.setBounds(leftMid.removeFromTop(28));
    leftMid.removeFromTop(5);
    
    auto btnRow1 = leftMid.removeFromTop(32);
    saveGlobalPresetBtn.setBounds(btnRow1.removeFromLeft(btnRow1.getWidth() / 2).reduced(2));
    loadGlobalPresetBtn.setBounds(btnRow1.reduced(2));
    
    auto btnRow2 = leftMid.removeFromTop(32);
    deleteGlobalPresetBtn.setBounds(btnRow2.removeFromLeft(btnRow2.getWidth() / 2).reduced(2));
    removeRgbBrushBtn.setBounds(btnRow2.reduced(2));
    
    globalPresetList.setBounds(midRgb.reduced(2));

    rgbArea.removeFromTop(10); // spacing

    // Bottom Section: FX Target List
    targetViewport.setBounds(rgbArea);
    targetListContent.setBounds(0, 0, targetViewport.getWidth() - 20, (int)fxTargetBtns.size() * 32);
    int curY = 0;
    for (auto& btn : fxTargetBtns) {
        btn->setBounds(5, curY, targetListContent.getWidth() - 10, 28);
        curY += 30;
    }

    // ===== LEARN TAB =====
    auto learnArea = midiLearnContent.getLocalBounds().reduced(5);
    auto learnTop = learnArea.removeFromTop(35);
    deviceManagerCombo.setBounds(learnTop.removeFromLeft(learnTop.getWidth() - 80));
    saveMidiBtn.setBounds(learnTop.removeFromLeft(40).reduced(2));
    openMidiBtn.setBounds(learnTop.reduced(2));

    mappingViewport.setBounds(learnArea);
    mappingListContent.setBounds(0, 0, mappingViewport.getWidth() - 10, (int)midiRows.size() * 35);
    int rowY = 0;
    for (auto& row : midiRows) {
        row->setBounds(0, rowY, mappingListContent.getWidth(), 30);
        rowY += 35;
    }

    // ===== SERIAL TAB =====
    auto serialArea = serialContent.getLocalBounds().reduced(10);
    inputModeLabel.setBounds(serialArea.removeFromTop(20));
    inputModeCombo.setBounds(serialArea.removeFromTop(30).reduced(0, 2));
    serialPortLabel.setBounds(serialArea.removeFromTop(20));
    serialPortCombo.setBounds(serialArea.removeFromTop(30).reduced(0, 2));
    serialArea.removeFromTop(10);
    serialLog.setBounds(serialArea);

    // ===== CONFIG TAB =====
    auto configArea = configContent.getLocalBounds().reduced(5);
    deviceSelector->setBounds(configArea);
}

void FxRackComponent::setExpanded(bool shouldExpand)
{
    expanded = shouldExpand;
    toggleBtn.setButtonText(expanded ? ">" : "<");
    if (onExpandedChanged) onExpandedChanged(expanded);
    if (auto* p = getParentComponent()) p->resized();
    resized();
}

bool FxRackComponent::isRgbTabActive() const { return tabs.getCurrentTabName() == "RGB"; }
juce::Colour FxRackComponent::getSelectedColor() const { return colorSelector.getCurrentColour(); }

void FxRackComponent::updateMappingDisplay(int rowIdx, const juce::String& text) {
    if (rowIdx >= 0 && rowIdx < (int)midiRows.size())
        midiRows[rowIdx]->setMappingValue(text);
}

int FxRackComponent::getLearningRowIndex() const {
    for (int i = 0; i < (int)midiRows.size(); ++i) {
        if (midiRows[i]->isLearning()) return i;
    }
    return -1;
}

juce::String FxRackComponent::getCurrentDeviceName() const { return deviceManagerCombo.getText(); }

void FxRackComponent::updateFxDisplay (int fxIndex, float amount, bool enabled) {
    if (fxIndex >= 0 && fxIndex < (int)fxSlots.size())
        fxSlots[fxIndex]->updateUI(amount, enabled);
}

void FxRackComponent::appendSerialLog(const juce::String& text) {
    juce::MessageManager::callAsync([this, text] {
        serialLog.moveCaretToEnd();
        serialLog.insertTextAtCaret(text + "\n");
        if (serialLog.getText().length() > 5000)
            serialLog.setText(serialLog.getText().getLastCharacters(2000));
    });
}

void FxRackComponent::setInputMode(int mode) {
    inputModeCombo.setSelectedId(mode + 1, juce::dontSendNotification);
}

void FxRackComponent::setSerialPort(const juce::String& port) {
    for (int i = 0; i < serialPortCombo.getNumItems(); ++i) {
        juce::String itemText = serialPortCombo.getItemText(i);
        if (itemText == port || itemText.startsWith(port + " - ")) {
            serialPortCombo.setSelectedItemIndex(i, juce::dontSendNotification);
            return;
        }
    }
}
