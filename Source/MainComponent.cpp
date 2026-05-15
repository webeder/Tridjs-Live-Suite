#include "MainComponent.h"
#include "Controllers/ControllerSandboxWindow.h"
#include "LanguageManager.h"
#include "ResourceHelper.h"

MainComponent::MainComponent() 
    : rgbManager (inputManager.getSerialManager()),
      jsEngine (std::make_unique<MixxxJSEngine>(controllerRouter))
{
    setLookAndFeel (&customTheme);
    juce::LookAndFeel::setDefaultLookAndFeel(&customTheme);
    
    // Initialize Language — robust multi-path lookup
    auto langDir = LanguageManager::getLangDirectory();
    auto langFile = langDir.getChildFile("pt_br.xml");
    
    if (langFile.existsAsFile())
        LanguageManager::getInstance().loadLanguage(langFile);
    else
        DBG("WARNING: pt_br.xml not found — UI will use key names as fallback text");
    
    LanguageManager::getInstance().addChangeListener(this);

    // Initialize Data
    audioEngine.setDatabase(&trackDb);
    browser = std::make_unique<TrackBrowserComponent>(trackDb, analysisManager, driveManager);

    // Initialize HandFree Layout
    handFreeComp = std::make_unique<HandFreeComponent>(audioEngine, inputManager, rgbManager, deviceManager, browser.get());
    addAndMakeVisible(handFreeComp.get());
    
    // Initialize Mixer Layout (hidden by default)
    mixerComp = std::make_unique<MixerComponent>(audioEngine, browser.get());
    addChildComponent(mixerComp.get());

    // Setup MenuBar
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(menuBar.get());

    // Command Manager setup
    commandManager.registerAllCommandsForTarget(this);
    addKeyListener(commandManager.getKeyMappings());
    setWantsKeyboardFocus(true);

    setSize(1024, 768);
    loadAllSettings();

    // Register inputManager as a global MIDI callback for the deviceManager
    deviceManager.addMidiInputCallback({}, &inputManager);

  // Initialize Inputs
  inputManager.onNoteOn = [this](int note, float vel) {
      int learningIdx = handFreeComp->fxRack.getLearningRowIndex();
      if (learningIdx != -1) {
          midiMappings[learningIdx] = "Note " + juce::String(note);
          juce::MessageManager::callAsync([this, learningIdx, note] {
              handFreeComp->fxRack.updateMappingDisplay(learningIdx, "Note " + juce::String(note));
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
      int learningIdx = handFreeComp->fxRack.getLearningRowIndex();
      if (learningIdx != -1) {
          midiMappings[learningIdx] = "CC " + juce::String(cc);
          juce::MessageManager::callAsync([this, learningIdx, cc] {
              handFreeComp->fxRack.updateMappingDisplay(learningIdx, "CC " + juce::String(cc));
          });
          return;
      }
      for (auto const& [rowIdx, id] : midiMappings) {
          if (id == "CC " + juce::String(cc)) applyMidiAction(rowIdx, val);
      }
  };

  inputManager.onStatusMessage = [this](const juce::String& msg) { handFreeComp->fxRack.appendSerialLog("[SYSTEM] " + msg); };
  inputManager.onRawDataReceived = [this](const juce::String& data) { handFreeComp->fxRack.appendSerialLog("RX: " + data); };
  inputManager.getSerialManager().onRawDataSent = [this](const juce::String& data) { handFreeComp->fxRack.appendSerialLog("TX: " + data); };

  inputManager.onMessageReceived = [this](const juce::MidiMessage& msg) {
      // Debug log for incoming MIDI
      juce::Logger::writeToLog("MIDI IN: " + juce::String::toHexString(msg.getRawData(), msg.getRawDataSize()));

      // 1. Convert raw MIDI to internal ControllerInputEvent using the Mixxx XML parser
      int status = msg.getRawData()[0];
      int midino = msg.getRawData()[1];
      int val7bit = msg.getRawData()[2];

      auto ctrl = mixxxParser.getControl(status, midino);
      
      ControllerInputEvent ev;
      ev.type = ControllerEventType::Unknown;

      // Handle 14-bit MIDI
      if (ctrl.is14BitMSB)
      {
          msbState[status][midino] = val7bit;
          // Trigger coarse update immediately for responsiveness
          ev.value = val7bit / 127.0f;
      }
      else if (ctrl.is14BitLSB)
      {
          int msbMidino = midino - 0x20;
          if (msbState.count(status) && msbState[status].count(msbMidino))
          {
              int msb = msbState[status][msbMidino];
              int val14bit = (msb << 7) | val7bit;
              ev.value = val14bit / 16383.0f;
              msbState[status].erase(msbMidino);
          }
          else 
          {
              // LSB arrived without MSB, treat as fine adjustment of 0
              ev.value = val7bit / 16383.0f; 
          }
      }
      else
      {
          ev.value = val7bit / 127.0f;
      }

      // Map Deck Index
      int chNum = (status & 0x0F) + 1;
      bool isMaster = ctrl.group.containsIgnoreCase("Master");
      
      if (ctrl.group.startsWithIgnoreCase("[Channel"))
          chNum = ctrl.group.substring(8, ctrl.group.length() - 1).getIntValue();
      else if (ctrl.group.containsIgnoreCase("[Channel")) {
          // Handle complex groups like [EqualizerRack1_[Channel1]_Effect1]
          int startIdx = ctrl.group.indexOfIgnoreCase("[Channel") + 8;
          int endIdx = ctrl.group.indexOf(startIdx, "]");
          if (endIdx > startIdx) chNum = ctrl.group.substring(startIdx, endIdx).getIntValue();
      }
      
      // FLX10 Mapping: Deck 1/3 -> A, Deck 2/4 -> B
      int targetDeck = (chNum == 1 || chNum == 3) ? 0 : 1;
      ev.deckIndex = targetDeck;

      // Map Mixxx keys to our internal EventTypes
      if (ctrl.key == "play")            ev.type = ControllerEventType::Play;
      else if (ctrl.key == "cue_default") ev.type = ControllerEventType::Cue;
      else if (isMaster && ctrl.key == "volume") ev.type = ControllerEventType::MasterVolume;
      else if (ctrl.key == "volume")      ev.type = ControllerEventType::Volume;
      else if (ctrl.key == "pregain")     ev.type = ControllerEventType::Gain;
      else if (ctrl.key == "rate")        ev.type = ControllerEventType::Pitch;
      else if (ctrl.key.containsIgnoreCase("filter") || ctrl.key == "super1") ev.type = ControllerEventType::Filter;
      else if (ctrl.key.containsIgnoreCase("parameter1")) ev.type = ControllerEventType::EQLow;
      else if (ctrl.key.containsIgnoreCase("parameter2")) ev.type = ControllerEventType::EQMid;
      else if (ctrl.key.containsIgnoreCase("parameter3")) ev.type = ControllerEventType::EQHigh;
      else if (ctrl.key == "crossfader")  ev.type = ControllerEventType::Crossfader;
      else if (ctrl.key == "sync_enabled") ev.type = ControllerEventType::Sync;
      else if (ctrl.key == "loop_in")     ev.type = ControllerEventType::LoopIn;
      else if (ctrl.key == "loop_out")    ev.type = ControllerEventType::LoopOut;
      else if (ctrl.key == "reloop_toggle") ev.type = ControllerEventType::Reloop;
      else if (ctrl.key == "loop_halve")  ev.type = ControllerEventType::LoopHalve;
      else if (ctrl.key == "loop_double") ev.type = ControllerEventType::LoopDouble;
      else if (ctrl.key == "cue_set")     ev.type = ControllerEventType::Cue; // Generic cue set
      else if (ctrl.key.containsIgnoreCase("hotcue_")) {
          ev.type = ControllerEventType::HotCue;
          ev.parameter = ctrl.key.substring(7).getIntValue();
      }
      else if (ctrl.key == "slip_enabled") ev.type = ControllerEventType::Slip;
      else if (ctrl.isScriptBinding && ctrl.key.containsIgnoreCase("jog"))
      {
          ev.type = ControllerEventType::JogScratch;
          ev.value = (val7bit - 64.0f) / 10.0f;
      }

      if (ev.type != ControllerEventType::Unknown)
          controllerRouter.pushEvent(ev);

      // 2. Handle Script Bindings (Mixxx JS compatible) - QUEUED to UI thread
      if (ctrl.isScriptBinding && jsEngine)
      {
          juce::String funcName = ctrl.key;
          juce::String group = ctrl.group;
          int midiChannel = (status & 0x0F); // 0-based MIDI channel
          juce::MessageManager::callAsync([this, funcName, group, midiChannel, midino, val7bit, status]() {
              if (jsEngine)
                  jsEngine->callMixxxFunction(funcName, midiChannel, midino, val7bit, status, group);
          });
      }
  };

  loadFlx10Mapping();

  // RGB Assignment Logic
  auto handleAssign = [this](int targetType, int targetIdx) {
      int selPalette = handFreeComp->fxRack.lightingPaletteList.getSelectedRow();
      juce::Colour color = handFreeComp->fxRack.getSelectedColor();
      
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
          handFreeComp->gridPads.getPads()[targetIdx]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
      } else { // FX
          rgbManager.setFxMapping(targetIdx, m);
          handFreeComp->fxRack.fxSlots[targetIdx]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
      }
  };

  auto handleClear = [this](int targetType, int targetIdx) {
      if (targetType == 0) {
          rgbManager.clearPadMapping(targetIdx);
          handFreeComp->gridPads.getPads()[targetIdx]->setRgbInfo("", juce::Colours::transparentBlack);
      } else {
          rgbManager.clearFxMapping(targetIdx);
          handFreeComp->fxRack.fxSlots[targetIdx]->setRgbInfo("", juce::Colours::transparentBlack);
      }
  };

  handFreeComp->fxRack.onAssignRgbRequested = handleAssign;
  handFreeComp->fxRack.onClearRgbRequested = handleClear;

  handFreeComp->fxRack.onMappingUpdated = [this](int row, const juce::String& id) {
      if (id.isEmpty()) midiMappings.erase(row);
      else midiMappings[row] = id;
      saveAllSettings();
  };

  handFreeComp->fxRack.onSaveMappingRequested = [this] {
      auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
      auto mappersDir = appDir.getChildFile("Mappers");
      mappersDir.createDirectory();
      fileChooser = std::make_unique<juce::FileChooser>(
          TJS_L("FILE_CHOOSER_TITLE"), mappersDir, "*.xml");
      fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
          [this](const juce::FileChooser& fc) {
              auto f = fc.getResult();
              if (f.existsAsFile() || !f.getFullPathName().isEmpty()) {
                  auto fileName = f.getFileNameWithoutExtension();
                  if (fileName.isEmpty()) fileName = "mapping_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
                  persistence.saveMidiMapping(fileName, "Manual", midiMappings);
              }
          });
  };

  handFreeComp->fxRack.onOpenMappingRequested = [this] {
      auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
      auto mappersDir = appDir.getChildFile("Mappers");
      mappersDir.createDirectory();
      fileChooser = std::make_unique<juce::FileChooser>(
          TJS_L("FILE_CHOOSER_TITLE"), mappersDir, "*.xml");
      fileChooser->launchAsync(juce::FileBrowserComponent::openMode,
          [this](const juce::FileChooser& fc) {
              auto f = fc.getResult();
              if (f.existsAsFile())
                  loadMidiMappingFromFile(f);
          });
  };

  handFreeComp->fxRack.onExpandedChanged = [this](bool) { resized(); };

  handFreeComp->fxRack.onSaveGlobalPreset = [this](const juce::String& name) {
      rgbManager.saveGlobalPreset(name);
      handFreeComp->fxRack.globalPresetListModel.items = rgbManager.getGlobalPresetNames();
      handFreeComp->fxRack.globalPresetList.updateContent();
  };
  
  handFreeComp->fxRack.onLoadGlobalPreset = [this](int row) {
      auto names = rgbManager.getGlobalPresetNames();
      if (row >= 0 && row < names.size()) {
          rgbManager.loadGlobalPreset(names[row]);
          for (int i=0; i<9; ++i) {
             auto const& m = rgbManager.getPadMapping(i);
             handFreeComp->gridPads.getPads()[i]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
          }
          for (int i=0; i<6; ++i) {
             auto const& m = rgbManager.getFxMapping(i);
             handFreeComp->fxRack.fxSlots[i]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
          }
      }
  };

  handFreeComp->fxRack.onDeleteGlobalPreset = [this](int row) {
      auto names = rgbManager.getGlobalPresetNames();
      if (row >= 0 && row < names.size()) {
          rgbManager.deleteGlobalPreset(names[row]);
          handFreeComp->fxRack.globalPresetListModel.items = rgbManager.getGlobalPresetNames();
          handFreeComp->fxRack.globalPresetList.updateContent();
      }
  };

  // Wire Pads
  for (int i = 0; i < (int)handFreeComp->gridPads.getPads().size(); ++i) {
    auto& pad = handFreeComp->gridPads.getPads()[i];
    pad->onLeftClick = [this, i, handleAssign] { 
        if (handFreeComp->fxRack.isRgbTabActive()) {
            handleAssign(0, i); 
            return true;
        }
        return false;
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
        bool shouldLoop = handFreeComp->gridPads.getPads()[i]->isLoopMode();
        if (audioEngine.loadAudioFile(f, i, shouldLoop)) {
            handFreeComp->gridPads.getPads()[i]->setLoadedFile(f);
            saveAllSettings();
        }
    };
    pad->onRecordRequested = [this, i](int) {
        if (handFreeComp->gridPads.getPads()[i]->isRecordingMode()) audioEngine.startPadRecording(i);
        else audioEngine.stopPadRecording(i, [this, i](juce::File recordedFile){
            juce::Timer::callAfterDelay(300, [this, i, recordedFile]{
                juce::MessageManager::callAsync([this, i, recordedFile]{
                    audioEngine.stopPad(i);
                    bool shouldLoop = handFreeComp->gridPads.getPads()[i]->isLoopMode();
                    if (audioEngine.loadAudioFile(recordedFile, i, shouldLoop)) {
                        handFreeComp->gridPads.getPads()[i]->setLoadedFile(recordedFile);
                        saveAllSettings();
                    }
                });
            });
        });
    };
    pad->onVolumeChanged = [this](int idx, float v) { audioEngine.setPadVolume(idx, v); };
  }

  handFreeComp->gridPads.onPitchChanged = [this](double val) { 
      updatePitchFromExtern((float)(val / 0.06)); 
  };

  // Wire FX Rack
  handFreeComp->fxRack.onFxToggled = [this](int idx, bool on) { 
      audioEngine.setFxEnabled(2, idx, on); 
      rgbManager.triggerRgb(rgbManager.getFxMapping(idx), on);
  };
  handFreeComp->fxRack.onFxAmountChanged = [this](int idx, float amt) { audioEngine.setFxAmount(2, idx, amt); };

  // Wire XY Pad
  handFreeComp->fxRack.touchTabContent.touchPad.onXyChanged = [this](float x, float y) {
      audioEngine.setXyFilter(2, x, y);
  };
  handFreeComp->fxRack.touchTabContent.touchPad.onActiveChanged = [this](bool active) {
      audioEngine.setXyFilterEnabled(2, active);
  };
  handFreeComp->fxRack.touchTabContent.touchPad.onSerialRgbRequested = [this](int r, int g, int b) {
      rgbManager.sendColor(juce::Colour((juce::uint8)r, (juce::uint8)g, (juce::uint8)b));
  };

  handFreeComp->fxRack.touchTabContent.modeBtn.onClick = [this] {
      bool isUltra = handFreeComp->fxRack.touchTabContent.modeBtn.getButtonText() == "ULTRA MULTI-FX";
      if (isUltra) {
          handFreeComp->fxRack.touchTabContent.modeBtn.setButtonText("LADDER FILTER");
          handFreeComp->fxRack.touchTabContent.touchPad.setMode(FX_Rack_Alpha::Ladder);
          audioEngine.setXyMode(2, AudioCore::XyMode::Ladder);
      } else {
          handFreeComp->fxRack.touchTabContent.modeBtn.setButtonText("ULTRA MULTI-FX");
          handFreeComp->fxRack.touchTabContent.touchPad.setMode(FX_Rack_Alpha::UltraMulti);
          audioEngine.setXyMode(2, AudioCore::XyMode::UltraMulti);
      }
  };

  // Setup Palette UI
  juce::StringArray paletteNames;
  for (const auto& e : rgbManager.getLightingEffects()) paletteNames.add(e.displayName);
  handFreeComp->fxRack.paletteListModel.items = paletteNames;
  handFreeComp->fxRack.lightingPaletteList.updateContent();

  // Standard components
  handFreeComp->header.onPlay = [this] { audioEngine.playMainTrack(); };
  handFreeComp->header.onStop = [this] { audioEngine.stopMainTrack(); };
  handFreeComp->header.onSeek = [this](double pos) { audioEngine.seekMainTrack(pos); };
  handFreeComp->header.onMasterVolumeChanged = [this](float v) { audioEngine.setMasterVolume(v); };
  handFreeComp->header.onTrackVolumeChanged = [this](float v) { audioEngine.setTrackVolume(v); };
  
  auto loadTrackWithMetadata = [this](const juce::File& file) {
      TrackDatabase::Track t;
      bool hasMetadata = trackDb.getTrackByPath(file.getFullPathName(), t);
      
      if (currentMode == LayoutMode::HandFree) {
          handFreeComp->header.waveformDisplay.loadedTrackName = file.getFileNameWithoutExtension();
          audioEngine.setDeckCuePoint(2, 0.0);
          handFreeComp->header.waveformDisplay.isPlaying = false;
          handFreeComp->header.waveformDisplay.generateRgbWaveform(file);
          handFreeComp->header.waveformDisplay.repaint();
          if (hasMetadata) audioEngine.loadMainTrack(file, t.bpm);
          else audioEngine.loadMainTrack(file);
      } else {
          audioEngine.loadDeckA(file, hasMetadata ? t.bpm : 0.0);
      }
      analysisManager.queueTrack(file);
  };


  handFreeComp->header.onFileDropped = loadTrackWithMetadata;
  handFreeComp->header.onEject = [this] {
      audioEngine.ejectMainTrack();
      handFreeComp->header.waveformDisplay.loadedTrackName = "";
      handFreeComp->header.waveformDisplay.repaint();
  };
  handFreeComp->header.onLoopSet = [this](double start, double duration) { audioEngine.setMainTrackLoopRange(start, duration); };
  handFreeComp->header.onLoopEnabled = [this](bool enabled) { audioEngine.setMainTrackLoopEnabled(enabled); };
  handFreeComp->header.onRecordToggled = [this](bool isRecording) {
      if (isRecording) {
          audioEngine.startMasterRecording();
      } else {
          audioEngine.stopMasterRecording([this](juce::File tempFile) {
              handleMasterRecordingFinalization(tempFile);
          });
      }
  };

  handFreeComp->fxRack.onInputModeChanged = [this](int modeIdx) { 
      auto mode = static_cast<InputManager::InputMode>(modeIdx);
      inputManager.setInputMode(mode); 
      rgbManager.setMidiOutput(mode == InputManager::InputMode::MIDI);
  };
  
  handFreeComp->fxRack.onSerialPortChanged = [this](const juce::String& port) { inputManager.openSerialPort(port); };

  if (browser) {
      browser->onTrackDoubleClicked = loadTrackWithMetadata;
  }

  setAudioChannels (2, 2);
  setLayoutMode(0); // Ensure initial layout is correctly setup
  startTimer(50);
}

MainComponent::~MainComponent() {
    analysisManager.stopAnalysis(); // Stop background threads first
    if (saveOnQuit) saveAllSettings(); 
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
    if (currentMode == LayoutMode::HandFree && handFreeComp != nullptr) {
        handFreeComp->header.updateTransportInfo(audioEngine.getMainTrackPosition(), audioEngine.getMainTrackLength(), audioEngine.isMainTrackPlaying());
        handFreeComp->header.updatePeakLevel(audioEngine.getDeckPeakLevel(2));
        handFreeComp->header.updateBpmDisplay(audioEngine.getMainTrackBpm());
        
        auto& pads = handFreeComp->gridPads.getPads();
        for (int i = 0; i < (int)pads.size(); ++i) {
            pads[i]->setPlayStateExternally(audioEngine.isPadPlaying(i));
        }
        
        handFreeComp->footer.setPlaying(audioEngine.isMainTrackPlaying());
        handFreeComp->footer.setBpm(audioEngine.getMainTrackBpm());

        inputManager.setLearning(handFreeComp->fxRack.getLearningRowIndex() != -1);
    }

    processControllerEvents();
    autoDetectController();
}

void MainComponent::paint (juce::Graphics& g) {
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    
    if (menuBar != nullptr)
        menuBar->setBounds(area.removeFromTop(juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));

    if (currentMode == LayoutMode::HandFree && handFreeComp != nullptr)
        handFreeComp->setBounds(area);
    else if (currentMode == LayoutMode::Mixer && mixerComp != nullptr)
        mixerComp->setBounds(area);
}

// MenuBarModel Implementation
juce::StringArray MainComponent::getMenuBarNames() {
    return { TJS_L("MENU_GENERAL"), TJS_L("MENU_SETTINGS"), TJS_L("MENU_LAYOUT"), TJS_L("MENU_PLUGINS"), TJS_L("MENU_HELP") };
}

juce::PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) {
    juce::PopupMenu menu;
    
    // We compare against the keys or the localized names? 
    // Usually it's better to use topLevelMenuIndex to be language-independent
    if (topLevelMenuIndex == 0) {
        menu.addCommandItem(&commandManager, navFx);
        menu.addCommandItem(&commandManager, navFxTouch);
        menu.addCommandItem(&commandManager, navRgb);
        menu.addCommandItem(&commandManager, navLearn);
        menu.addCommandItem(&commandManager, navSerial);
        menu.addCommandItem(&commandManager, navConfig);
    } else if (topLevelMenuIndex == 1) {
        menu.addItem(300, TJS_L("MENU_MANAGE_HARDWARE"));
        menu.addSeparator();
        menu.addItem(301, TJS_L("MENU_RELOAD_MAPPING"));
    } else if (topLevelMenuIndex == 2) {
        menu.addItem(1, TJS_L("MENU_DJ_HAND_FREE"), true, currentMode == LayoutMode::HandFree);
        menu.addItem(2, TJS_L("MENU_DJ_MIXER"), true, currentMode == LayoutMode::Mixer);
    } else if (topLevelMenuIndex == 3) {
        menu.addItem(100, TJS_L("MENU_RESCAN_PLUGINS"));
        menu.addSeparator();
        menu.addItem(200, juce::String(juce::CharPointer_UTF8("\xf0\x9f\x8e\x9b\xef\xb8\x8f  ")) + TJS_L("MENU_CONTROLLER_SANDBOX"));
    } else if (topLevelMenuIndex == 4) {
        menu.addItem(10, TJS_L("MENU_ABOUT"));
        menu.addItem(11, TJS_L("MENU_LICENSE"));
        menu.addSeparator();
        menu.addItem(20, TJS_L("MENU_DONATE"));
    }
    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int topLevelMenuIndex) {
    if (menuItemID == 1) setLayoutMode(0);
    else if (menuItemID == 2) setLayoutMode(1);
    else if (menuItemID == 300) showControllerManager();
    else if (menuItemID == 301) loadFlx10Mapping();
    else if (menuItemID == 10) showAboutWindow();
    else if (menuItemID == 11) showLicenseWindow();
    else if (menuItemID == 20) showDonateWindow();
    else if (menuItemID == 100) audioEngine.getVstManager().rescan();
    else if (menuItemID == 200) openControllerSandbox();
}

void MainComponent::openControllerSandbox()
{
    if (!sandboxWindow || !sandboxWindow->isVisible())
    {
        sandboxWindow = std::make_unique<ControllerSandboxWindow>();
        sandboxWindow->setVisible(true);
    }
    else
    {
        sandboxWindow->toFront(true);
    }
}

// ApplicationCommandTarget implementation
juce::ApplicationCommandTarget* MainComponent::getNextCommandTarget() { return nullptr; }

void MainComponent::getAllCommands(juce::Array<juce::CommandID>& commands) {
    const juce::CommandID ids[] = { navFx, navFxTouch, navRgb, navLearn, navSerial, navConfig };
    commands.addArray(ids, juce::numElementsInArray(ids));
}

void MainComponent::getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result) {
    switch (commandID)
    {
    case navFx:
        result.setInfo("FX", TJS_L("DESC_NAV_FX"), "Navigation", 0);
        result.addDefaultKeypress('f', juce::ModifierKeys::commandModifier);
        break;
    case navFxTouch:
        result.setInfo("FX Touch", TJS_L("DESC_NAV_FX_TOUCH"), "Navigation", 0);
        result.addDefaultKeypress('t', juce::ModifierKeys::commandModifier);
        break;
    case navRgb:
        result.setInfo("RGB", TJS_L("DESC_NAV_RGB"), "Navigation", 0);
        result.addDefaultKeypress('r', juce::ModifierKeys::commandModifier);
        break;
    case navLearn:
        result.setInfo("Learn", TJS_L("DESC_NAV_LEARN"), "Navigation", 0);
        result.addDefaultKeypress('l', juce::ModifierKeys::commandModifier);
        break;
    case navSerial:
        result.setInfo("Serial", TJS_L("DESC_NAV_SERIAL"), "Navigation", 0);
        result.addDefaultKeypress(',', juce::ModifierKeys::commandModifier);
        result.addDefaultKeypress('m', juce::ModifierKeys::commandModifier);
        break;
    case navConfig:
        result.setInfo("Config", TJS_L("DESC_NAV_CONFIG"), "Navigation", 0);
        result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier);
        break;
    default:
        break;
    }
}

bool MainComponent::perform(const InvocationInfo& info) {
    switch (info.commandID)
    {
    case navFx:      navegarParaAba(0); return true;
    case navFxTouch: navegarParaAba(1); return true;
    case navRgb:     navegarParaAba(2); return true;
    case navLearn:   navegarParaAba(3); return true;
    case navSerial:  navegarParaAba(4); return true;
    case navConfig:  navegarParaAba(5); return true;
    default:
        return false;
    }
}

void MainComponent::setLayoutMode(int mode) {
    currentMode = (mode == 0) ? LayoutMode::HandFree : LayoutMode::Mixer;
    
    if (browser) {
        if (currentMode == LayoutMode::Mixer) {
            handFreeComp->bottomPanel.setContent(nullptr);
            mixerComp->addAndMakeVisible(browser.get());
            browser->setVisible(true);
            browser->toFront(false);
            mixerComp->resized(); // Force internal layout update
        } else {
            if (browser->getParentComponent() == mixerComp.get())
                mixerComp->removeChildComponent(browser.get());
            
            handFreeComp->bottomPanel.setContent(browser.get());
            browser->setVisible(true);
            browser->toFront(false);
        }
    }

    if (handFreeComp != nullptr) handFreeComp->setVisible(currentMode == LayoutMode::HandFree);
    if (mixerComp != nullptr) mixerComp->setVisible(currentMode == LayoutMode::Mixer);
    
    menuBar->repaint();
    resized();
}

void MainComponent::navegarParaAba(int tabIndex) {
    if (currentMode == LayoutMode::HandFree && handFreeComp != nullptr) {
        handFreeComp->navegarParaAba(tabIndex);
    }
}

void MainComponent::showAboutWindow() {
    auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto iconFile = appDir.getChildFile("icone.png");
    
    juce::Image icon = juce::ImageCache::getFromFile(iconFile);
    
    juce::String msg = TJS_L("ABOUT_MSG");

    // AlertWindow instance to allow custom components (like the icon at the top)
    auto* aw = new juce::AlertWindow(TJS_L("ABOUT_TITLE"), "", juce::MessageBoxIconType::NoIcon);
    
    if (icon.isValid()) {
        auto imgComp = std::make_unique<juce::ImageComponent>();
        imgComp->setImage(icon);
        imgComp->setSize(120, 120);
        
        // We'll store the component in a shared pointer or let the AlertWindow own it via a workaround
        // Actually, easiest is to use a simple ImageComponent and manage its lifecycle
        aw->addCustomComponent(imgComp.release()); // release transfers ownership if we delete it later
    }
    
    aw->addTextBlock(msg);
    aw->addButton("OK", 0);
    
    aw->enterModalState(true, juce::ModalCallbackFunction::create([aw](int) {
        // Delete custom components first since AlertWindow doesn't delete them
        for (int i = 0; i < aw->getNumCustomComponents(); ++i)
            delete aw->getCustomComponent(i);
            
        delete aw;
    }));
}

void MainComponent::showLicenseWindow() {
    juce::String msg = TJS_L("LICENSE_MSG");

    auto opts = juce::MessageBoxOptions()
        .withTitle(TJS_L("LICENSE_TITLE"))
        .withMessage(msg)
        .withButton(TJS_L("BTN_OK"))
        .withButton(TJS_L("BTN_VISIT_SITE"));

    juce::AlertWindow::showAsync(opts, [this](int result) {
        if (result == 2) { // Second button (Visitar Site)
            juce::URL("https://www.tridjs.com.br").launchInDefaultBrowser();
        }
    });
}

void MainComponent::showDonateWindow() {
    juce::String msg = TJS_L("DONATE_MSG");

    auto opts = juce::MessageBoxOptions()
        .withTitle(TJS_L("DONATE_TITLE"))
        .withMessage(msg)
        .withButton(TJS_L("BTN_MAYBE_LATER"))
        .withButton(TJS_L("BTN_BE_PART"));

    juce::AlertWindow::showAsync(opts, [](int result) {
        if (result == 2) { // Second button (Fazer parte desta história)
            juce::URL("https://www.tridjs.com.br/doar").launchInDefaultBrowser();
        }
    });
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
            handFreeComp->header.updateMasterVolumeFromExtern(mv);
            handFreeComp->header.updateTrackVolumeFromExtern(tv);
            
            int mode = settingsNode->getIntAttribute("inputMode", 0);
            inputManager.setInputMode(static_cast<InputManager::InputMode>(mode));
            handFreeComp->fxRack.setInputMode(mode);
            rgbManager.setMidiOutput(mode == (int)InputManager::InputMode::MIDI);
            
            bool logOn = settingsNode->getBoolAttribute("serialLogEnabled", true);
            handFreeComp->fxRack.logCheckbox.setToggleState(logOn, juce::dontSendNotification);
            
            juce::String port = settingsNode->getStringAttribute("serialPort");
            if (port.isNotEmpty()) {
                inputManager.openSerialPort(port);
                handFreeComp->fxRack.setSerialPort(port);
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
                    handFreeComp->gridPads.getPads()[idx]->setLoadedFile(juce::File(file));
                    handFreeComp->gridPads.getPads()[idx]->setLoopStateExternally(shouldLoop);
                }
            }
        }
        delete xml;
    }

    // 3.5 Mixer State
    if (mixerComp != nullptr) {
        PersistenceManager::MixerState mixerState;
        if (persistence.loadMixerState(mixerState)) {
            mixerComp->mixer.crossfader.setValue(mixerState.crossfader);

            auto loadDeck = [&](const PersistenceManager::DeckState& ds, int deckIdx) {
                if (ds.loadedFile.isNotEmpty() && juce::File(ds.loadedFile).existsAsFile()) {
                    if (deckIdx == 0) audioEngine.loadDeckA(juce::File(ds.loadedFile));
                    else audioEngine.loadDeckB(juce::File(ds.loadedFile));
                }

                auto& fader = (deckIdx == 0) ? mixerComp->mixer.faderA : mixerComp->mixer.faderB;
                fader.setValue(ds.volume);

                auto& knobs = (deckIdx == 0) ? mixerComp->mixer.knobsA : mixerComp->mixer.knobsB;
                knobs[0]->setValue(ds.gain);
                knobs[1]->setValue(ds.eqHigh);
                knobs[2]->setValue(ds.eqMid);
                knobs[3]->setValue(ds.eqLow);
                knobs[4]->setValue(ds.filter);

                auto& fx = (deckIdx == 0) ? mixerComp->mixer.fxA : mixerComp->mixer.fxB;
                for (int i = 0; i < 6; ++i) {
                    fx.slots[i]->knob.setValue(ds.fxSlots[i].amount);
                    fx.slots[i]->knob.getProperties().set("isActive", ds.fxSlots[i].enabled);
                    audioEngine.setFxEnabled(deckIdx, i, ds.fxSlots[i].enabled);
                    audioEngine.setFxAmount(deckIdx, i, ds.fxSlots[i].amount);
                    fx.slots[i]->knob.repaint();
                }

                auto& deckSection = (deckIdx == 0) ? mixerComp->deckA : mixerComp->deckB;
                deckSection.pitchSlider.setValue(ds.pitch);
                audioEngine.setSyncEnabled(deckIdx, ds.syncEnabled);

                if (ds.loopLength > 0.0) {
                    audioEngine.setDeckLoopRange(deckIdx, ds.loopStart, ds.loopLength);
                }
                audioEngine.setDeckLoopEnabled(deckIdx, ds.loopEnabled);
            };

            loadDeck(mixerState.deckA, 0);
            loadDeck(mixerState.deckB, 1);
        }
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
                    handFreeComp->gridPads.getPads()[idx]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
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
                    handFreeComp->fxRack.fxSlots[idx]->setRgbInfo(m.presetName, m.type == RgbMapping::FixedColor ? m.fixedColor : juce::Colours::white);
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
                             (int)inputManager.getInputMode(), inputManager.getSerialManager().getCurrentPort(),
                             handFreeComp->fxRack.logCheckbox.getToggleState());

    // 3. Pads
    std::vector<PersistenceManager::PadState> padStates;
    for (auto& pad : handFreeComp->gridPads.getPads()) {
        padStates.push_back({ pad->getLoadedFilePath(), pad->isLoopMode() });
    }
    persistence.savePadAssignments(padStates);

    // 4. Mappings
    persistence.saveMidiMapping("default_mapping", "Combined", midiMappings);

    // 5. RGB
    persistence.saveRgbSettings(rgbManager.getLightingEffects(), rgbManager.getAllPadMappings(), rgbManager.getAllFxMappings());

    // 6. Mixer State
    if (mixerComp != nullptr) {
        PersistenceManager::MixerState mixerState;
        mixerState.crossfader = (float)mixerComp->mixer.crossfader.getValue();

        auto saveDeck = [&](PersistenceManager::DeckState& ds, int deckIdx) {
            ds.loadedFile = audioEngine.getDeckFilePath(deckIdx);
            
            auto& fader = (deckIdx == 0) ? mixerComp->mixer.faderA : mixerComp->mixer.faderB;
            ds.volume = (float)fader.getValue();
            
            auto& knobs = (deckIdx == 0) ? mixerComp->mixer.knobsA : mixerComp->mixer.knobsB;
            ds.gain = (float)knobs[0]->getValue();
            ds.eqHigh = (float)knobs[1]->getValue();
            ds.eqMid = (float)knobs[2]->getValue();
            ds.eqLow = (float)knobs[3]->getValue();
            ds.filter = (float)knobs[4]->getValue();
            
            auto& fx = (deckIdx == 0) ? mixerComp->mixer.fxA : mixerComp->mixer.fxB;
            for (int i = 0; i < 6; ++i) {
                ds.fxSlots[i].amount = (float)fx.slots[i]->knob.getValue();
                ds.fxSlots[i].enabled = fx.slots[i]->knob.getProperties()["isActive"];
            }
            
            auto& deckSection = (deckIdx == 0) ? mixerComp->deckA : mixerComp->deckB;
            ds.pitch = (float)deckSection.pitchSlider.getValue();
            ds.syncEnabled = audioEngine.isSyncEnabled(deckIdx);
            ds.loopEnabled = audioEngine.isDeckLoopEnabled(deckIdx);
            ds.loopStart = audioEngine.getDeckLoopStart(deckIdx);
            ds.loopLength = audioEngine.getDeckLoopLength(deckIdx);
        };

        saveDeck(mixerState.deckA, 0);
        saveDeck(mixerState.deckB, 1);

        persistence.saveMixerState(mixerState);
    }
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
        audioEngine.setFxEnabled(2, rowIdx - 9, pressed);
        handFreeComp->fxRack.updateFxDisplay(rowIdx - 9, 1.0f, pressed);
    }
    // 15-18: Transport
    else if (rowIdx == 15 && pressed) audioEngine.playMainTrack();
    else if (rowIdx == 16 && pressed) audioEngine.stopMainTrack();
    else if (rowIdx == 17 && pressed) audioEngine.seekMainTrack(0.0);
    else if (rowIdx == 18 && pressed) audioEngine.ejectMainTrack();
    
    // 19: Master Volume
    else if (rowIdx == 19) {
        audioEngine.setMasterVolume(value);
        handFreeComp->header.updateMasterVolumeFromExtern(value);
    }
    // 20: Track Volume
    else if (rowIdx == 20) {
        audioEngine.setTrackVolume(value);
        handFreeComp->header.updateTrackVolumeFromExtern(value);
    }
    // 21: Global Pitch (Bipolar: 0.5 is neutral)
    else if (rowIdx == 21) {
        float pitchVal = (value * 2.0f) - 1.0f; // Map 0..1 to -1..1
        updatePitchFromExtern(pitchVal);
    }
    // 22: Pads Pitch
    else if (rowIdx == 22) {
        float pitchVal = (value * 2.0f) - 1.0f;
        handFreeComp->gridPads.updatePitchFromExtern(value);
        updatePitchFromExtern(pitchVal);
    }

    // 23-31: Pad Eject
    else if (rowIdx >= 23 && rowIdx <= 31 && pressed) {
        int pIdx = rowIdx - 23;
        handFreeComp->gridPads.getPads()[pIdx]->eject();
    }
    // 32-40: Pad Loop
    else if (rowIdx >= 32 && rowIdx <= 40 && pressed) {
        int pIdx = rowIdx - 32;
        bool newState = !handFreeComp->gridPads.getPads()[pIdx]->isLoopMode();
        handFreeComp->gridPads.getPads()[pIdx]->setLoopStateExternally(newState);
        audioEngine.setPadLoop(pIdx, newState);
    }
    // 41-49: Pad Record
    else if (rowIdx >= 41 && rowIdx <= 49 && pressed) {
        int pIdx = rowIdx - 41;
        handFreeComp->gridPads.getPads()[pIdx]->triggerRecord();
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
                handFreeComp->fxRack.updateMappingDisplay(row, id);
            }
        }
        delete xml;
    }
}
void MainComponent::resetPitch() {
    handFreeComp->resetPitch();
}

void MainComponent::updatePitchFromExtern(float v) {
    handFreeComp->updatePitchValue(v);
}

void MainComponent::incrementPitch(float delta) {
    // Note: I might need to implement incrementPitch in HandFreeComponent too
    // or just use updatePitchFromExtern
}

void MainComponent::handleMasterRecordingFinalization(const juce::File& tempFile) {
    if (!tempFile.existsAsFile()) return;

    auto* aw = new juce::AlertWindow("Finalizar Gravação", "Digite o nome para a gravação da Master:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("nameInput", "", "Nome do arquivo...");
    aw->addButton("Salvar", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancelar", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw, tempFile](int result) {
        if (result == 1) {
            juce::String fileName = aw->getTextEditorContents("nameInput").trim();
            juce::Time now = juce::Time::getCurrentTime();
            
            if (fileName.isEmpty()) {
                fileName = "rec_" + now.formatted("%Y%m%d_%H%M%S");
            }
            
            if (!fileName.endsWithIgnoreCase(".wav")) fileName += ".wav";

            // Pasta base especificada pelo usuário (%USERPROFILE%\Music\tridjsLiveSuite\Recordings)
            juce::File targetDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                                    .getChildFile("tridjsLiveSuite")
                                    .getChildFile("Recordings");
            
            // Pasta dinâmica por data
            juce::File dateFolder = targetDir.getChildFile(now.formatted("%Y-%m-%d"));
            
            if (!dateFolder.exists()) {
                if (!dateFolder.createDirectory()) {
                    // Fallback para pasta padrão de músicas se não houver permissão no caminho fixo
                    dateFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                                    .getChildFile("Tridjs_Recordings")
                                    .getChildFile(now.formatted("%Y-%m-%d"));
                    dateFolder.createDirectory();
                }
            }

            juce::File targetFile = dateFolder.getChildFile(fileName);
            
            // Evitar sobrescrever se o nome for duplicado
            if (targetFile.existsAsFile()) {
                 targetFile = dateFolder.getNonexistentChildFile(fileName.replace(".wav", ""), ".wav");
            }

            if (!tempFile.moveFileTo(targetFile)) {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, 
                    "Erro ao Salvar", "Não foi possível mover o arquivo para a pasta de destino.");
            }
        } else {
            // Cancelado: Deletar arquivo temporário fisicamente
            if (tempFile.existsAsFile())
                tempFile.deleteFile();
        }
        delete aw;
    }));
}

void MainComponent::showControllerManager()
{
    if (!controllerManagerWindow)
    {
        controllerManagerWindow = std::make_unique<ControllerManagerWindow>(
            persistence.getMappingsFolder(),
            [this](const juce::File& f) { loadControllerMapping(f); },
            &midiMappings,
            [this](int row, const juce::String& id) {
                if (id.isEmpty()) midiMappings.erase(row);
                else midiMappings[row] = id;
                saveAllSettings();
                handFreeComp->fxRack.updateMappingDisplay(row, id);
            }
        );
    }
    controllerManagerWindow->setVisible(true);
    controllerManagerWindow->toFront(true);
}

void MainComponent::loadControllerMapping(const juce::File& mappingFile)
{
    if (mappingFile.existsAsFile())
    {
        mixxxParser.loadXml(mappingFile);
        
        // Load companion JS script
        auto jsFile = mappingFile.withFileExtension("js");
        if (!jsFile.existsAsFile())
            jsFile = mappingFile.getParentDirectory().getChildFile(mappingFile.getFileNameWithoutExtension() + "-script.js");
            
        if (jsFile.existsAsFile() && jsEngine)
        {
            jsEngine->executeScript(jsFile.loadFileAsString());
            
            // Try to call init function (Mixxx standard)
            // The function name is usually Prefix.init where Prefix is defined in XML
            // For FLX10 it's likely DDJFLX10.init
            jsEngine->callFunction("DDJFLX10.init", 0, "none", "none");
            jsEngine->callFunction("init", 0, "none", "none"); // Fallback
        }
        
        juce::Logger::writeToLog("Controller Mapping Loaded: " + mappingFile.getFileName() + " (" + juce::String(mixxxParser.getMappingCount()) + " controls)");
    }
    else
    {
        juce::Logger::writeToLog("Controller Mapping NOT FOUND: " + mappingFile.getFullPathName());
    }
}

void MainComponent::loadFlx10Mapping()
{
    // First try the user documents folder
    auto mappingFile = persistence.getMappingsFolder().getChildFile("Pioneer-DDJ-FLX10.midi.xml");
    
    // Fallback to bundled mappings
    if (!mappingFile.existsAsFile())
    {
        auto mappingsDir = findMappingsDir();
        if (mappingsDir.exists())
            mappingFile = mappingsDir.getChildFile("FLX10").getChildFile("Pioneer-DDJ-FLX10.midi.xml");
    }

    loadControllerMapping(mappingFile);
    flx10MappingLoaded = true;
}

void MainComponent::autoDetectController()
{
    if (flx10MappingLoaded) return;

    auto devices = juce::MidiInput::getAvailableDevices();
    for (auto& d : devices)
    {
        if (d.name.containsIgnoreCase("FLX10") || d.name.containsIgnoreCase("DDJ-FLX10"))
        {
            juce::Logger::writeToLog("Auto-detected: " + d.name + " - loading FLX10 mapping");
            loadFlx10Mapping();
            break;
        }
    }
}

void MainComponent::processControllerEvents()
{
    ControllerInputEvent ev;
    while (controllerRouter.popEvent(ev))
    {
        switch (ev.type)
        {
            case ControllerEventType::Play:
                if (ev.value > 0.5f) {
                    if (audioEngine.isDeckPlaying(ev.deckIndex)) {
                        if (ev.deckIndex == 0) audioEngine.stopDeckA();
                        else if (ev.deckIndex == 1) audioEngine.stopDeckB();
                    } else {
                        if (ev.deckIndex == 0) audioEngine.playDeckA();
                        else if (ev.deckIndex == 1) audioEngine.playDeckB();
                    }
                }
                break;

            case ControllerEventType::Cue:
                if (ev.value > 0.5f) {
                    audioEngine.triggerDeckCue(ev.deckIndex);
                } else {
                    if (ev.deckIndex == 0) audioEngine.stopDeckA();
                    else if (ev.deckIndex == 1) audioEngine.stopDeckB();
                }
                break;

            case ControllerEventType::Volume:
                audioEngine.setDeckVolume(ev.deckIndex, ev.value);
                break;

            case ControllerEventType::Gain:
                audioEngine.setDeckGain(ev.deckIndex, ev.value);
                break;

            case ControllerEventType::EQHigh:
                audioEngine.setDeckEQ(ev.deckIndex, 2, ev.value);
                break;

            case ControllerEventType::EQMid:
                audioEngine.setDeckEQ(ev.deckIndex, 1, ev.value);
                break;

            case ControllerEventType::EQLow:
                audioEngine.setDeckEQ(ev.deckIndex, 0, ev.value);
                break;

            case ControllerEventType::Filter:
                audioEngine.setDeckFilter(ev.deckIndex, (ev.value * 2.0f) - 1.0f);
                break;

            case ControllerEventType::Pitch:
                // When Smart Fader is active, ignore physical pitch faders (FLX10 sends continuous
                // position updates that would fight with the Smart Fader's automatic BPM blending)
                if (audioEngine.isSmartFaderEnabled() && ev.deckIndex < 2)
                    break;
                audioEngine.setDeckPitch(ev.deckIndex, (1.0f - ev.value) * 2.0f - 1.0f);
                break;

            case ControllerEventType::Crossfader:
                audioEngine.setCrossfaderPosition(ev.value);
                break;

            case ControllerEventType::MasterVolume:
                audioEngine.setMasterVolume(ev.value);
                break;

            case ControllerEventType::Sync:
                if (ev.value > 0.5f) audioEngine.setSyncEnabled(ev.deckIndex, !audioEngine.isSyncEnabled(ev.deckIndex));
                break;

            case ControllerEventType::ScratchMode:
                audioEngine.setDeckScratching(ev.deckIndex, ev.value > 0.5f);
                break;

            case ControllerEventType::JogScratch:
                audioEngine.applyJogDelta(ev.deckIndex, ev.value * 0.04f);
                break;

            case ControllerEventType::JogBend:
                audioEngine.applyJogDelta(ev.deckIndex, ev.value * 0.02f);
                break;

            case ControllerEventType::LoopIn:
                if (ev.value > 0.5f) {
                    audioEngine.setDeckCuePoint(ev.deckIndex, audioEngine.getDeckPosition(ev.deckIndex));
                    // Start of loop range
                    audioEngine.setDeckLoopRange(ev.deckIndex, audioEngine.getDeckPosition(ev.deckIndex), audioEngine.getDeckLoopLength(ev.deckIndex));
                }
                break;

            case ControllerEventType::LoopOut:
                if (ev.value > 0.5f) {
                    double start = audioEngine.getDeckLoopStart(ev.deckIndex);
                    double current = audioEngine.getDeckPosition(ev.deckIndex);
                    if (current > start) {
                        audioEngine.setDeckLoopRange(ev.deckIndex, start, current - start);
                        audioEngine.setDeckLoopEnabled(ev.deckIndex, true);
                    }
                }
                break;

            case ControllerEventType::Reloop:
                if (ev.value > 0.5f) {
                    audioEngine.setDeckLoopEnabled(ev.deckIndex, !audioEngine.isDeckLoopEnabled(ev.deckIndex));
                }
                break;

            case ControllerEventType::LoopHalve: // Triggered by physical IN button
                if (ev.value > 0.5f && audioEngine.isDeckLoopEnabled(ev.deckIndex)) {
                    double start = audioEngine.getDeckLoopStart(ev.deckIndex);
                    double len = audioEngine.getDeckLoopLength(ev.deckIndex);
                    double newLen = len * 0.5;
                    if (newLen > 0.01) // Minimum ~10ms
                        audioEngine.setDeckLoopRange(ev.deckIndex, start, newLen);
                }
                break;

            case ControllerEventType::LoopDouble: // Triggered by physical OUT button
                if (ev.value > 0.5f && audioEngine.isDeckLoopEnabled(ev.deckIndex)) {
                    double start = audioEngine.getDeckLoopStart(ev.deckIndex);
                    double len = audioEngine.getDeckLoopLength(ev.deckIndex);
                    audioEngine.setDeckLoopRange(ev.deckIndex, start, len * 2.0);
                }
                break;

            case ControllerEventType::ScriptCall:
                if (jsEngine)
                {
                    // For script calls from router, we use the stored key
                    juce::String funcName = juce::String::fromUTF8(ev.scriptKey);
                    juce::String group = "[Channel" + juce::String(ev.deckIndex + 1) + "]";
                    jsEngine->callFunction(funcName, ev.value, group, funcName);
                }
                break;

            default:
                break;
        }
    }
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &LanguageManager::getInstance())
    {
        // Notify the menu bar model that names have changed
        this->menuItemsChanged();
    }
}
