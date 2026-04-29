#include "MainComponent.h"

MainComponent::MainComponent() 
    : rgbManager (inputManager.getSerialManager())
{
    setLookAndFeel (&customTheme);
    
    // Initialize HandFree Layout
    handFreeComp = std::make_unique<HandFreeComponent>(audioEngine, inputManager, rgbManager, deviceManager);
    addAndMakeVisible(handFreeComp.get());
    
    // Initialize Mixer Placeholder (hidden by default)
    addChildComponent(mixerPlaceholder);

    // Setup MenuBar
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(menuBar.get());

    // Command Manager setup
    commandManager.registerAllCommandsForTarget(this);
    addKeyListener(commandManager.getKeyMappings());
    setWantsKeyboardFocus(true);

    setSize(1024, 768);
    loadAllSettings();

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
      audioEngine.setFxEnabled(idx, on); 
      rgbManager.triggerRgb(rgbManager.getFxMapping(idx), on);
  };
  handFreeComp->fxRack.onFxAmountChanged = [this](int idx, float amt) { audioEngine.setFxAmount(idx, amt); };

  // Wire XY Pad
  handFreeComp->fxRack.touchTabContent.touchPad.onXyChanged = [this](float x, float y) {
      audioEngine.setXyFilter(x, y);
  };
  handFreeComp->fxRack.touchTabContent.touchPad.onActiveChanged = [this](bool active) {
      audioEngine.setXyFilterEnabled(active);
  };
  handFreeComp->fxRack.touchTabContent.touchPad.onSerialRgbRequested = [this](int r, int g, int b) {
      rgbManager.sendColor(juce::Colour((juce::uint8)r, (juce::uint8)g, (juce::uint8)b));
  };

  handFreeComp->fxRack.touchTabContent.modeBtn.onClick = [this] {
      bool isUltra = handFreeComp->fxRack.touchTabContent.modeBtn.getButtonText() == "ULTRA MULTI-FX";
      if (isUltra) {
          handFreeComp->fxRack.touchTabContent.modeBtn.setButtonText("LADDER FILTER");
          handFreeComp->fxRack.touchTabContent.touchPad.setMode(FX_Rack_Alpha::Ladder);
          audioEngine.setXyMode(AudioCore::XyMode::Ladder);
      } else {
          handFreeComp->fxRack.touchTabContent.modeBtn.setButtonText("ULTRA MULTI-FX");
          handFreeComp->fxRack.touchTabContent.touchPad.setMode(FX_Rack_Alpha::UltraMulti);
          audioEngine.setXyMode(AudioCore::XyMode::UltraMulti);
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
  handFreeComp->header.onFileDropped = [this](const juce::File& file) { audioEngine.loadMainTrack(file); };
  handFreeComp->header.onEject = [this] { audioEngine.ejectMainTrack(); };
  handFreeComp->header.onLoopSet = [this](double start, double duration) { audioEngine.setMainTrackLoopRange(start, duration); };
  handFreeComp->header.onLoopEnabled = [this](bool enabled) { audioEngine.setMainTrackLoopEnabled(enabled); };

  handFreeComp->stems.onStemMuteChanged = [this](int idx, bool muted) { audioEngine.setStemMuted(idx, muted); };
  handFreeComp->fxRack.onMidiDeviceIndexChanged = [this](int idx) {
      auto devices = juce::MidiInput::getAvailableDevices();
      if (idx > 0 && (idx-1) < devices.size()) inputManager.setMidiInput(devices[idx-1]);
  };
  handFreeComp->fxRack.onInputModeChanged = [this](int modeIdx) { 
      auto mode = static_cast<InputManager::InputMode>(modeIdx);
      inputManager.setInputMode(mode); 
      rgbManager.setMidiOutput(mode == InputManager::InputMode::MIDI);
  };
  handFreeComp->fxRack.onSerialPortChanged = [this](const juce::String& port) { inputManager.openSerialPort(port); };

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
    if (currentMode == LayoutMode::HandFree && handFreeComp != nullptr) {
        handFreeComp->header.updateTransportInfo(audioEngine.getMainTrackPosition(), audioEngine.getMainTrackLength(), audioEngine.isMainTrackPlaying());
        handFreeComp->header.updatePeakLevel(audioEngine.getCurrentPeakLevel());
        handFreeComp->header.updateBpmDisplay(audioEngine.getMainTrackBpm());
        
        auto& pads = handFreeComp->gridPads.getPads();
        for (int i = 0; i < (int)pads.size(); ++i) {
            pads[i]->setPlayStateExternally(audioEngine.isPadPlaying(i));
        }
        
        handFreeComp->footer.setPlaying(audioEngine.isMainTrackPlaying());
        handFreeComp->footer.setBpm(audioEngine.getMainTrackBpm());

        inputManager.setLearning(handFreeComp->fxRack.getLearningRowIndex() != -1);
    }
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
    else if (currentMode == LayoutMode::Mixer)
        mixerPlaceholder.setBounds(area);
}

// MenuBarModel Implementation
juce::StringArray MainComponent::getMenuBarNames() {
    return { "Geral", "Layout", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) {
    juce::PopupMenu menu;
    if (menuName == "Geral") {
        menu.addCommandItem(&commandManager, navFx);
        menu.addCommandItem(&commandManager, navFxTouch);
        menu.addCommandItem(&commandManager, navRgb);
        menu.addCommandItem(&commandManager, navLearn);
        menu.addCommandItem(&commandManager, navSerial);
        menu.addCommandItem(&commandManager, navConfig);
    } else if (menuName == "Layout") {
        menu.addItem(1, "DJ Hand Free", true, currentMode == LayoutMode::HandFree);
        menu.addItem(2, "DJ Mixer (Em breve)", true, currentMode == LayoutMode::Mixer);
    } else if (menuName == "Help") {
        menu.addItem(10, "Sobre");
        menu.addItem(11, "Licença e Uso");
        menu.addSeparator();
        menu.addItem(20, "❤️ Doar / Apoiar");
    }
    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int topLevelMenuIndex) {
    if (menuItemID == 1) setLayoutMode(0);
    else if (menuItemID == 2) setLayoutMode(1);
    else if (menuItemID == 10) showAboutWindow();
    else if (menuItemID == 11) showLicenseWindow();
    else if (menuItemID == 20) showDonateWindow();
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
        result.setInfo("FX", "Navegar para aba de FX", "Navigation", 0);
        result.addDefaultKeypress('f', juce::ModifierKeys::commandModifier);
        break;
    case navFxTouch:
        result.setInfo("FX Touch", "Navegar para aba de FX Touch", "Navigation", 0);
        result.addDefaultKeypress('t', juce::ModifierKeys::commandModifier);
        break;
    case navRgb:
        result.setInfo("RGB", "Navegar para aba de RGB", "Navigation", 0);
        result.addDefaultKeypress('r', juce::ModifierKeys::commandModifier);
        break;
    case navLearn:
        result.setInfo("Learn", "Navegar para aba de MIDI Learn", "Navigation", 0);
        result.addDefaultKeypress('l', juce::ModifierKeys::commandModifier);
        break;
    case navSerial:
        result.setInfo("Serial", "Navegar para aba de Serial", "Navigation", 0);
        result.addDefaultKeypress(',', juce::ModifierKeys::commandModifier);
        result.addDefaultKeypress('m', juce::ModifierKeys::commandModifier);
        break;
    case navConfig:
        result.setInfo("Config", "Navegar para aba de Configurações", "Navigation", 0);
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
    
    if (handFreeComp != nullptr) handFreeComp->setVisible(currentMode == LayoutMode::HandFree);
    mixerPlaceholder.setVisible(currentMode == LayoutMode::Mixer);
    
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
    
    juce::String msg = "Tridjs Live Suite\n\n"
                      "Versão: Beta 1.0\n"
                      "Autor: DJ Exder (Coletivo TriDJS)\n"
                      "Segurança da Informação: DJ Christian Mauro";

    // AlertWindow instance to allow custom components (like the icon at the top)
    auto* aw = new juce::AlertWindow("Sobre", "", juce::MessageBoxIconType::NoIcon);
    
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
    juce::String msg = "Uso: Software gratuito para uso pessoal e profissional.\n\n"
                      "Proibição: É estritamente proibida a venda deste software.\n\n"
                      "Modificações (GitHub): Em caso de modificação do código-fonte disponível no GitHub, "
                      "é obrigatório manter e dar os devidos créditos ao desenvolvedor original (DJ Exder) "
                      "e ao Coletivo TriDJS.\n\n"
                      "Direitos: Direitos reservados ao Coletivo TriDJS (Marca Registrada).";

    auto opts = juce::MessageBoxOptions()
        .withTitle("Licença e Uso")
        .withMessage(msg)
        .withButton("OK")
        .withButton("Visitar Site");

    juce::AlertWindow::showAsync(opts, [this](int result) {
        if (result == 1) { // Second button (Visitar Site)
            juce::URL("https://www.tridjs.com.br").launchInDefaultBrowser();
        }
    });
}

void MainComponent::showDonateWindow() {
    juce::String msg = "\"Fortaleça a Revolução DJ Hand Free!\"\n\n"
                      "O Tridjs Live Suite nasceu com a missão de quebrar as barreiras entre o artista e a tecnologia. "
                      "Nosso objetivo é fomentar a cultura da música eletrônica, oferecendo ferramentas autorais que "
                      "permitem uma performance única e expressiva.\n\n"
                      "Ao apoiar este projeto, você ajuda o Coletivo TriDJs a manter o software gratuito para todos, "
                      "a investir em novos sensores para a tecnologia de gestos e a fortalecer a cena musical no Brasil e no mundo.\n\n"
                      "Sua doação é um investimento na liberdade criativa.\n\n"
                      "AVISO DE SEGURANÇA: Por segurança, todas as contribuições são processadas exclusivamente através do nosso site oficial.";

    auto opts = juce::MessageBoxOptions()
        .withTitle("Apoie o Movimento TriDJS")
        .withMessage(msg)
        .withButton("Talvez Depois")
        .withButton("Fazer parte dessa história (Site Seguro)");

    juce::AlertWindow::showAsync(opts, [](int result) {
        if (result == 1) { // Second button
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
