#include "MainComponent.h"

MainComponent::MainComponent() 
    : header (audioEngine.getThumbnail()),
      fxRack (deviceManager),
      rgbManager (inputManager.getSerialManager())
{
    setLookAndFeel (&customTheme);
  setSize(1024, 768);

  loadAllSettings();

  // Initialize Inputs
  inputManager.onNoteOn = [this](int note, float vel) {
      int learningIdx = fxRack.getLearningRowIndex();
      if (learningIdx != -1) {
          midiMappings[learningIdx] = "Note " + juce::String(note);
          juce::MessageManager::callAsync([this, learningIdx, note] {
              fxRack.updateMappingDisplay(learningIdx, "Note " + juce::String(note));
          });
          return;
      }

      juce::String target = "Note " + juce::String(note);
      for (auto const& [row, val] : midiMappings) {
          if (val == target) {
              applyMidiAction(row, vel);
              if (row < 9) rgbManager.triggerRgb(rgbManager.getPadMapping(row), true);
              else if (row >= 9 && row <= 14) rgbManager.triggerRgb(rgbManager.getFxMapping(row-9), true);
          }
      }
  };

  inputManager.onNoteOff = [this](int note) {
      juce::String target = "Note " + juce::String(note);
      for (auto const& [row, val] : midiMappings) {
          if (val == target) {
              applyMidiAction(row, 0.0f);
              if (row < 9) rgbManager.triggerRgb(rgbManager.getPadMapping(row), false);
              else if (row >= 9 && row <= 14) rgbManager.triggerRgb(rgbManager.getFxMapping(row-9), false);
          }
      }
  };

  inputManager.onCC = [this](int cc, float val) {
      int learningIdx = fxRack.getLearningRowIndex();
      if (learningIdx != -1) {
          midiMappings[learningIdx] = "CC " + juce::String(cc);
          juce::MessageManager::callAsync([this, learningIdx, cc] {
              fxRack.updateMappingDisplay(learningIdx, "CC " + juce::String(cc));
          });
          return;
      }
      for (auto const& [rowIdx, id] : midiMappings) {
          if (id == "CC " + juce::String(cc)) applyMidiAction(rowIdx, val);
      }
  };

  inputManager.onStatusMessage = [this](const juce::String& msg) { fxRack.appendSerialLog("[SYSTEM] " + msg); };
  inputManager.onRawDataReceived = [this](const juce::String& data) { fxRack.appendSerialLog("RX: " + data); };
  inputManager.getSerialManager().onRawDataSent = [this](const juce::String& data) { fxRack.appendSerialLog("TX: " + data); };

  addAndMakeVisible(header);
  addAndMakeVisible(sideBrowser);
  addAndMakeVisible(stems);
  addAndMakeVisible(gridPads);
  addAndMakeVisible(fxRack);
  addAndMakeVisible(footer);

  // RGB Assignment Logic
  auto handleAssign = [this](int targetType, int targetIdx) {
      int selPalette = fxRack.lightingPaletteList.getSelectedRow();
      juce::Colour color = fxRack.getSelectedColor();
      
      RgbMapping m;
      if (selPalette != -1) {
          m.type = RgbMapping::Preset;
          m.presetName = rgbManager.getLightingEffects()[selPalette].displayName;
      } else {
          m.type = RgbMapping::FixedColor;
          m.fixedColor = color;
      }

      if (targetType == 0) { // Pads
          rgbManager.setPadMapping(targetIdx, m);
          gridPads.getPads()[targetIdx]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
      } else { // FX
          rgbManager.setFxMapping(targetIdx, m);
          fxRack.fxSlots[targetIdx]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
      }
  };

  auto handleClear = [this](int targetType, int targetIdx) {
      if (targetType == 0) {
          rgbManager.clearPadMapping(targetIdx);
          gridPads.getPads()[targetIdx]->setRgbInfo("", juce::Colours::transparentBlack);
      } else {
          rgbManager.clearFxMapping(targetIdx);
          fxRack.fxSlots[targetIdx]->setRgbInfo("", juce::Colours::transparentBlack);
      }
  };

  fxRack.onAssignRgbRequested = handleAssign;
  fxRack.onClearRgbRequested = handleClear;

  fxRack.onMappingUpdated = [this](int row, const juce::String& id) {
      if (id.isEmpty()) midiMappings.erase(row);
      else midiMappings[row] = id;
      saveAllSettings();
  };

  fxRack.onExpandedChanged = [this](bool) { resized(); };

  fxRack.onSaveGlobalPreset = [this](const juce::String& name) {
      rgbManager.saveGlobalPreset(name);
      fxRack.globalPresetListModel.items = rgbManager.getGlobalPresetNames();
      fxRack.globalPresetList.updateContent();
  };
  
  fxRack.onLoadGlobalPreset = [this](int row) {
      auto names = rgbManager.getGlobalPresetNames();
      if (row >= 0 && row < names.size()) {
          rgbManager.loadGlobalPreset(names[row]);
          for (int i=0; i<9; ++i) {
             auto const& m = rgbManager.getPadMapping(i);
             gridPads.getPads()[i]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
          }
          for (int i=0; i<6; ++i) {
             auto const& m = rgbManager.getFxMapping(i);
             fxRack.fxSlots[i]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
          }
      }
  };

  fxRack.onDeleteGlobalPreset = [this](int row) {
      auto names = rgbManager.getGlobalPresetNames();
      if (row >= 0 && row < names.size()) {
          rgbManager.deleteGlobalPreset(names[row]);
          fxRack.globalPresetListModel.items = rgbManager.getGlobalPresetNames();
          fxRack.globalPresetList.updateContent();
      }
  };

  // Wire Pads
  for (int i = 0; i < (int)gridPads.getPads().size(); ++i) {
    auto& pad = gridPads.getPads()[i];
    pad->onLeftClick = [this, i, handleAssign] { 
        if (fxRack.isRgbTabActive()) {
            handleAssign(0, i); 
            return true; // Click consumed by RGB assignment
        }
        return false; // Click not consumed
    };
    pad->onRightClick = [this, i, handleClear] {
        juce::PopupMenu m;
        m.addItem(1, "Remover RGB");
        m.showMenuAsync(juce::PopupMenu::Options(), [this, i, handleClear](int result){
            if (result == 1) handleClear(0, i);
        });
    };
    pad->onPlayStateChanged = [this, i](int, bool playing) {
      if (playing) audioEngine.playPad(i);
      else audioEngine.stopPad(i);
      rgbManager.triggerRgb(rgbManager.getPadMapping(i), playing);
    };
    pad->onLoopToggled = [this, i](int, bool looping) { audioEngine.setPadLoop(i, looping); };
    pad->onEjectRequested = [this, i](int) {
        audioEngine.ejectPad(i);
        saveAllSettings();
    };
    pad->onFileDropped = [this, i](int, const juce::File &f) {
        bool shouldLoop = gridPads.getPads()[i]->isLoopMode();
        if (audioEngine.loadAudioFile(f, i, shouldLoop)) {
            gridPads.getPads()[i]->setLoadedFile(f);
            saveAllSettings();
        }
    };
    pad->onRecordRequested = [this, i](int) {
        if (gridPads.getPads()[i]->isRecordingMode()) audioEngine.startPadRecording(i);
        else audioEngine.stopPadRecording(i, [this, i](juce::File recordedFile){
            juce::Timer::callAfterDelay(300, [this, i, recordedFile]{
                juce::MessageManager::callAsync([this, i, recordedFile]{
                    audioEngine.stopPad(i); // Stop any current playback
                    bool shouldLoop = gridPads.getPads()[i]->isLoopMode();
                    if (audioEngine.loadAudioFile(recordedFile, i, shouldLoop)) {
                        gridPads.getPads()[i]->setLoadedFile(recordedFile);
                        saveAllSettings();
                    }
                });
            });
        });
    };
    pad->onVolumeChanged = [this](int idx, float v) { audioEngine.setPadVolume(idx, v); };
  }

  // Wire FX Rack
  fxRack.onFxToggled = [this](int idx, bool on) { 
      audioEngine.setFxEnabled(idx, on); 
      rgbManager.triggerRgb(rgbManager.getFxMapping(idx), on);
  };
  fxRack.onFxAmountChanged = [this](int idx, float amt) { audioEngine.setFxAmount(idx, amt); };

  // Setup Palette UI
  juce::StringArray paletteNames;
  for (const auto& e : rgbManager.getLightingEffects()) paletteNames.add(e.displayName);
  fxRack.paletteListModel.items = paletteNames;
  fxRack.lightingPaletteList.updateContent();

  // Standard components (header, footer, etc)
  header.onPlay = [this] { audioEngine.playMainTrack(); };
  header.onStop = [this] { audioEngine.stopMainTrack(); };
  header.onSeek = [this](double pos) { audioEngine.seekMainTrack(pos); };
  header.onMasterVolumeChanged = [this](float v) { audioEngine.setMasterVolume(v); };
  header.onTrackVolumeChanged = [this](float v) { audioEngine.setTrackVolume(v); };
  header.onPitchChanged = [this](float p) { audioEngine.setGlobalPitchRatio((double)p); };
  header.onFileDropped = [this](const juce::File& file) { audioEngine.loadMainTrack(file); };
  header.onEject = [this] { audioEngine.ejectMainTrack(); };
  header.onLoopSet = [this](double start, double duration) { audioEngine.setMainTrackLoopRange(start, duration); };
  header.onLoopEnabled = [this](bool enabled) { audioEngine.setMainTrackLoopEnabled(enabled); };

  stems.onStemMuteChanged = [this](int idx, bool muted) { audioEngine.setStemMuted(idx, muted); };
  fxRack.onMidiDeviceIndexChanged = [this](int idx) {
      auto devices = juce::MidiInput::getAvailableDevices();
      if (idx > 0 && (idx-1) < devices.size()) inputManager.setMidiInput(devices[idx-1]);
  };
  fxRack.onInputModeChanged = [this](int modeIdx) { inputManager.setInputMode(static_cast<InputManager::InputMode>(modeIdx)); };
  fxRack.onSerialPortChanged = [this](const juce::String& port) { inputManager.openSerialPort(port); };

  setAudioChannels (2, 2);
  startTimer(50);
}

MainComponent::~MainComponent() {
    saveAllSettings(); // SALVAR TUDO AO SAIR
    stopTimer();
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate) {
    audioEngine.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) {
    audioEngine.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources() {
    audioEngine.releaseResources();
}

void MainComponent::timerCallback() {
    header.updateTransportInfo(audioEngine.getMainTrackPosition(), audioEngine.getMainTrackLength(), audioEngine.isMainTrackPlaying());
    header.updatePeakLevel(audioEngine.getCurrentPeakLevel());
    header.updateBpmDisplay(audioEngine.getMainTrackBpm());
    
    // Auto-reset Pad borders when audio finishes (One-shot mode)
    auto& pads = gridPads.getPads();
    for (int i = 0; i < (int)pads.size(); ++i) {
        pads[i]->setPlayStateExternally(audioEngine.isPadPlaying(i));
    }
    
    footer.setPlaying(audioEngine.isMainTrackPlaying());
    footer.setBpm(audioEngine.getMainTrackBpm());

    inputManager.setLearning(fxRack.getLearningRowIndex() != -1);
}

void MainComponent::paint (juce::Graphics& g) {
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized() {
  auto area = getLocalBounds();
  header.setBounds(area.removeFromTop(130));
  footer.setBounds(area.removeFromBottom(40));
  auto body = area;
  sideBrowser.setBounds(body.removeFromLeft(sideBrowser.getComponentWidth()));
  stems.setBounds(body.removeFromTop(50));
  
  int rackWidth = fxRack.isExpanded() ? 320 : 35;
  fxRack.setBounds(body.removeFromRight(rackWidth));
  gridPads.setBounds(body);
}

void MainComponent::loadAllSettings() {
    // 1. Audio Device
    persistence.loadAudioDeviceState(deviceManager);

    // 2. Volumes and General
    if (auto* xml = persistence.loadSettings()) {
        if (auto* settingsNode = xml->getChildByName("SETTINGS")) {
            float mv = (float)settingsNode->getDoubleAttribute("masterVolume", 1.0);
            float tv = (float)settingsNode->getDoubleAttribute("trackVolume", 1.0);
            audioEngine.setMasterVolume(mv);
            audioEngine.setTrackVolume(tv);
            header.updateMasterVolumeFromExtern(mv);
            header.updateTrackVolumeFromExtern(tv);
            
            int mode = settingsNode->getIntAttribute("inputMode", 0);
            inputManager.setInputMode(static_cast<InputManager::InputMode>(mode));
            fxRack.setInputMode(mode);
            
            juce::String port = settingsNode->getStringAttribute("serialPort");
            if (port.isNotEmpty()) {
                inputManager.openSerialPort(port);
                fxRack.setSerialPort(port);
            }
        }
        delete xml;
    }

    // 3. Pads Assignments
    if (auto* xml = persistence.loadPadAssignments()) {
        if (auto* padsNode = xml->getChildByName("PAD_ASSIGNMENTS")) {
            for (auto* node = padsNode->getFirstChildElement(); node != nullptr; node = node->getNextElement()) {
                int idx = node->getIntAttribute("idx");
                juce::String file = node->getStringAttribute("file");
                bool shouldLoop = node->getBoolAttribute("loop");
                if (file.isNotEmpty()) {
                    audioEngine.loadAudioFile(juce::File(file), idx, shouldLoop);
                    gridPads.getPads()[idx]->setLoadedFile(juce::File(file));
                    gridPads.getPads()[idx]->setLoopStateExternally(shouldLoop);
                }
            }
        }
        delete xml;
    }

    // 4. MIDI/Serial Mappings
    auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto mapperFile = appDir.getChildFile("Mappers").getChildFile("default_mapping.xml");
    if (mapperFile.existsAsFile()) {
        loadMidiMappingFromFile(mapperFile);
    }

    // 5. RGB Settings
    if (auto* xml = persistence.loadRgbSettings()) {
        if (auto* rgbRoot = xml->getChildByName("RGB_SETTINGS")) {
            if (auto* padsNode = rgbRoot->getChildByName("PAD_MAPPINGS")) {
                for (auto* node = padsNode->getFirstChildElement(); node != nullptr; node = node->getNextElement()) {
                    int idx = node->getIntAttribute("index");
                    RgbMapping m;
                    m.type = static_cast<RgbMapping::Type>(node->getIntAttribute("type"));
                    m.presetName = node->getStringAttribute("preset");
                    m.fixedColor = juce::Colour::fromString(node->getStringAttribute("color"));
                    rgbManager.setPadMapping(idx, m);
                    gridPads.getPads()[idx]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
                }
            }
            if (auto* fxNode = rgbRoot->getChildByName("FX_MAPPINGS")) {
                for (auto* node = fxNode->getFirstChildElement(); node != nullptr; node = node->getNextElement()) {
                    int idx = node->getIntAttribute("index");
                    RgbMapping m;
                    m.type = static_cast<RgbMapping::Type>(node->getIntAttribute("type"));
                    m.presetName = node->getStringAttribute("preset");
                    m.fixedColor = juce::Colour::fromString(node->getStringAttribute("color"));
                    rgbManager.setFxMapping(idx, m);
                    fxRack.fxSlots[idx]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
                }
            }
        }
        delete xml;
    }
}

void MainComponent::saveAllSettings() {
    // 1. Audio Device
    persistence.saveAudioDeviceState(deviceManager);

    // 2. Volumes and General
    persistence.saveSettings(audioEngine.getMasterVolume(), audioEngine.getTrackVolume(), 
                             (int)inputManager.getInputMode(), inputManager.getSerialManager().getCurrentPort());

    // 3. Pads
    std::vector<PersistenceManager::PadState> padStates;
    for (auto& pad : gridPads.getPads()) {
        padStates.push_back({ pad->getLoadedFilePath(), pad->isLoopMode() });
    }
    persistence.savePadAssignments(padStates);

    // 4. Mappings
    persistence.saveMidiMapping("default_mapping", "Combined", midiMappings);

    // 5. RGB
    persistence.saveRgbSettings(rgbManager.getLightingEffects(), rgbManager.getAllPadMappings(), rgbManager.getAllFxMappings());
}

void MainComponent::applyMidiAction(int rowIdx, float value) {
    bool pressed = value > 0.0f;
    
    // 0-8: Pad Play
    if (rowIdx < 9) { 
        if (pressed) audioEngine.playPad(rowIdx);
        else audioEngine.stopPad(rowIdx);
    } 
    // 9-14: FX Slots
    else if (rowIdx >= 9 && rowIdx <= 14) {
        audioEngine.setFxEnabled(rowIdx - 9, pressed);
        fxRack.updateFxDisplay(rowIdx - 9, 1.0f, pressed);
    }
    // 15-18: Transport
    else if (rowIdx == 15 && pressed) audioEngine.playMainTrack();
    else if (rowIdx == 16 && pressed) audioEngine.stopMainTrack();
    else if (rowIdx == 17 && pressed) audioEngine.seekMainTrack(0.0);
    else if (rowIdx == 18 && pressed) audioEngine.ejectMainTrack();
    // 19-20: Pitch (Fixed increment for now)
    else if (rowIdx == 19 && pressed) header.incrementPitch(0.01);
    else if (rowIdx == 20 && pressed) header.incrementPitch(-0.01);
    // 21-29: Pad Eject
    else if (rowIdx >= 21 && rowIdx <= 29 && pressed) {
        int pIdx = rowIdx - 21;
        gridPads.getPads()[pIdx]->eject();
    }
    // 30-38: Pad Loop
    else if (rowIdx >= 30 && rowIdx <= 38 && pressed) {
        int pIdx = rowIdx - 30;
        bool newState = !gridPads.getPads()[pIdx]->isLoopMode();
        gridPads.getPads()[pIdx]->setLoopStateExternally(newState);
        audioEngine.setPadLoop(pIdx, newState);
    }
    // 39-47: Pad Record
    else if (rowIdx >= 39 && rowIdx <= 47 && pressed) {
        int pIdx = rowIdx - 39;
        gridPads.getPads()[pIdx]->triggerRecord();
    }
}

void MainComponent::loadMidiMappingFromFile (const juce::File& file)
{
    if (auto* xml = persistence.loadMidiMapping(file)) {
        if (auto* mapNode = xml->getChildByName("MIDI_MAP")) {
            midiMappings.clear();
            for (auto* node = mapNode->getFirstChildElement(); node != nullptr; node = node->getNextElement()) {
                int row = node->getIntAttribute("row");
                juce::String id = node->getStringAttribute("id");
                midiMappings[row] = id;
                fxRack.updateMappingDisplay(row, id);
            }
        }
        delete xml;
    }
}
