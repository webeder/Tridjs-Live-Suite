#include "RgbManager.h"

RgbManager::RgbManager(SerialManager& serial) : serialManager(serial)
{
    // Latest default lighting effects with friendly names (UTF-8)
    addLightingEffect({"Trovão Rápido", false, {}, "trovaofast"});
    addLightingEffect({"Trovão Sunset", false, {}, "trovaoslow"});
    addLightingEffect({"Arco-íris Fade", false, {}, "fadeiris"});
    addLightingEffect({"Arco-íris Flow", false, {}, "arcoiris"});
    addLightingEffect({"Batida Centro", false, {}, "beat"});
    addLightingEffect({"Colisão", false, {}, "impacto"});
    addLightingEffect({"Fader Duplo", false, {}, "faderduplo"});
    addLightingEffect({"Super Máquina", false, {}, "rodar"});
}

void RgbManager::addLightingEffect(const RgbPreset& effect)
{
    for (auto& e : lightingEffects) {
        if (e.displayName == effect.displayName) {
            e = effect;
            return;
        }
    }
    lightingEffects.push_back(effect);
}

void RgbManager::setPadMapping(int padIndex, const RgbMapping& mapping)
{
    padMappings[padIndex] = mapping;
}

void RgbManager::clearPadMapping(int padIndex)
{
    padMappings.erase(padIndex);
}

const RgbMapping& RgbManager::getPadMapping(int padIndex) const
{
    static RgbMapping empty;
    if (padMappings.count(padIndex)) return padMappings.at(padIndex);
    return empty;
}

void RgbManager::setFxMapping(int fxIndex, const RgbMapping& mapping)
{
    fxMappings[fxIndex] = mapping;
}

void RgbManager::clearFxMapping(int fxIndex)
{
    fxMappings.erase(fxIndex);
}

const RgbMapping& RgbManager::getFxMapping(int fxIndex) const
{
    static RgbMapping empty;
    if (fxMappings.count(fxIndex)) return fxMappings.at(fxIndex);
    return empty;
}

void RgbManager::triggerRgb(const RgbMapping& mapping, bool active)
{
    if (mapping.type == RgbMapping::Disabled) return;

    if (!active)
    {
        sendRgbCommand("parar");
        return;
    }

    switch (mapping.type)
    {
        case RgbMapping::Preset:
        {
            for (const auto& e : lightingEffects)
            {
                if (e.displayName == mapping.presetName)
                {
                    if (e.isFixedColor) sendColor(e.color);
                    else sendRgbCommand(e.command);
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
        default: break;
    }
}

void RgbManager::saveGlobalPreset(const juce::String& name)
{
    if (name.isEmpty()) return;
    
    RgbGlobalPreset gp;
    gp.name = name;
    gp.padMappings = padMappings;
    gp.fxMappings = fxMappings;
    
    for (auto& p : globalPresets) {
        if (p.name == name) {
            p = gp;
            return;
        }
    }
    globalPresets.push_back(gp);
}

void RgbManager::loadGlobalPreset(const juce::String& name)
{
    for (const auto& p : globalPresets) {
        if (p.name == name) {
            padMappings = p.padMappings;
            fxMappings = p.fxMappings;
            return;
        }
    }
}

void RgbManager::deleteGlobalPreset(const juce::String& name)
{
    globalPresets.erase(std::remove_if(globalPresets.begin(), globalPresets.end(),
        [&](const RgbGlobalPreset& p) { return p.name == name; }), globalPresets.end());
}

juce::StringArray RgbManager::getGlobalPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : globalPresets) names.add(p.name);
    return names;
}

void RgbManager::sendRgbCommand(const juce::String& cmd)
{
    serialManager.sendString(cmd);
}

void RgbManager::sendColor(juce::Colour c)
{
    juce::String cmd = juce::String(c.getRed()) + "," + 
                       juce::String(c.getGreen()) + "," + 
                       juce::String(c.getBlue());
    sendRgbCommand(cmd);
}
