#include "RgbManager.h"

RgbManager::RgbManager(SerialManager &serial) : serialManager(serial) {
  // Latest default lighting effects with friendly names (UTF-8)
  addLightingEffect({juce::String::fromUTF8("Vai e Vem"), false, {}, "rodar"});
  addLightingEffect(
      {juce::String::fromUTF8("Arco-íris"), false, {}, "arcoiris"});
  addLightingEffect({juce::String::fromUTF8("Trovão"), false, {}, "trovao"});
  addLightingEffect({juce::String::fromUTF8("Trovão Lento"), false, {}, "trovaoslow"});
  addLightingEffect({juce::String::fromUTF8("Fader"), false, {}, "fader"});
  addLightingEffect({juce::String::fromUTF8("Impacto"), false, {}, "impacto"});
  addLightingEffect(
      {juce::String::fromUTF8("Fader Duplo"), false, {}, "faderduplo"});
  addLightingEffect({juce::String::fromUTF8("Beat"), false, {}, "beat"});
}

void RgbManager::addLightingEffect(const RgbPreset &effect) {
  for (auto &e : lightingEffects) {
    if (e.displayName == effect.displayName) {
      e = effect;
      return;
    }
  }
  lightingEffects.push_back(effect);
}

void RgbManager::setPadMapping(int padIndex, const RgbMapping &mapping) {
  padMappings[padIndex] = mapping;
}

void RgbManager::clearPadMapping(int padIndex) { padMappings.erase(padIndex); }

const RgbMapping &RgbManager::getPadMapping(int padIndex) const {
  static RgbMapping empty;
  if (padMappings.count(padIndex))
    return padMappings.at(padIndex);
  return empty;
}

void RgbManager::setFxMapping(int fxIndex, const RgbMapping &mapping) {
  fxMappings[fxIndex] = mapping;
}

void RgbManager::clearFxMapping(int fxIndex) { fxMappings.erase(fxIndex); }

const RgbMapping &RgbManager::getFxMapping(int fxIndex) const {
  static RgbMapping empty;
  if (fxMappings.count(fxIndex))
    return fxMappings.at(fxIndex);
  return empty;
}

void RgbManager::triggerRgb(const RgbMapping &mapping, bool active) {
  if (mapping.type == RgbMapping::Disabled)
    return;

  if (!active) {
    sendRgbCommand("parar");
    return;
  }

  switch (mapping.type) {
  case RgbMapping::Preset: {
    for (const auto &e : lightingEffects) {
      if (e.displayName == mapping.presetName) {
        if (e.isFixedColor)
          sendColor(e.color);
        else
          sendRgbCommand(e.command);
        break;
      }
    }
    break;
  }
  case RgbMapping::FixedColor:
    sendColor(mapping.fixedColor);
    break;
  case RgbMapping::DirectCommand:
    sendRgbCommand(mapping.directCommand);
    break;
  default:
    break;
  }
}

void RgbManager::saveGlobalPreset(const juce::String &name) {
  if (name.isEmpty())
    return;

  RgbGlobalPreset gp;
  gp.name = name;
  gp.padMappings = padMappings;
  gp.fxMappings = fxMappings;

  for (auto &p : globalPresets) {
    if (p.name == name) {
      p = gp;
      return;
    }
  }
  globalPresets.push_back(gp);
}

void RgbManager::loadGlobalPreset(const juce::String &name) {
  for (const auto &p : globalPresets) {
    if (p.name == name) {
      padMappings = p.padMappings;
      fxMappings = p.fxMappings;
      return;
    }
  }
}

void RgbManager::deleteGlobalPreset(const juce::String &name) {
  globalPresets.erase(
      std::remove_if(globalPresets.begin(), globalPresets.end(),
                     [&](const RgbGlobalPreset &p) { return p.name == name; }),
      globalPresets.end());
}

juce::StringArray RgbManager::getGlobalPresetNames() const {
  juce::StringArray names;
  for (const auto &p : globalPresets)
    names.add(p.name);
  return names;
}

void RgbManager::sendRgbCommand(const juce::String &cmd) {
  juce::String textCmd;
  if (cmd == "rodar") textCmd = "100";
  else if (cmd == "arcoiris") textCmd = "101";
  else if (cmd == "trovao") textCmd = "102";
  else if (cmd == "trovaoslow") textCmd = "103";
  else if (cmd == "fader") textCmd = "104";
  else if (cmd == "impacto") textCmd = "105";
  else if (cmd == "faderduplo") textCmd = "106";
  else if (cmd == "beat") textCmd = "107";
  else if (cmd == "parar") textCmd = "108";
  else if (cmd == "ajuda") textCmd = "ajuda";

  if (textCmd.isNotEmpty()) {
    if (useMidiOutput) {
        int note = textCmd.getIntValue();
        if (note > 0) {
            uint8_t midiData[3] = { 0x90, (uint8_t)(note & 0x7F), 127 };
            serialManager.sendRawData((const char*)midiData, 3);
        }
    } else {
        serialManager.sendString(textCmd);
    }
  }
}

void RgbManager::sendColor(juce::Colour c) {
  if (useMidiOutput) {
      // Enviar como CC: R=109, G=110, B=111 (0-127)
      uint8_t r = (uint8_t)(c.getRed() >> 1);
      uint8_t g = (uint8_t)(c.getGreen() >> 1);
      uint8_t b = (uint8_t)(c.getBlue() >> 1);
      
      uint8_t ccR[3] = { 0xB0, 109, r };
      uint8_t ccG[3] = { 0xB0, 110, g };
      uint8_t ccB[3] = { 0xB0, 111, b };
      
      serialManager.sendRawData((const char*)ccR, 3);
      serialManager.sendRawData((const char*)ccG, 3);
      serialManager.sendRawData((const char*)ccB, 3);
  } else {
      // Escala normal 0-255 enviada como texto: R,G,B
      juce::String rgbText = juce::String((int)c.getRed()) + "," +
                             juce::String((int)c.getGreen()) + "," +
                             juce::String((int)c.getBlue());

      serialManager.sendString(rgbText);
  }
}
