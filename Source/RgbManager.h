#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>
#include "SerialManager.h"

struct RgbPreset {
    juce::String displayName;
    bool isFixedColor = false;
    juce::Colour color;
    juce::String command; // e.g. "trovaofast"
};

struct RgbMapping {
    enum Type { Preset, FixedColor, DirectCommand, Disabled };
    Type type = Disabled;
    juce::String presetName;
    juce::Colour fixedColor = juce::Colours::black;
    juce::String directCommand;
};

struct RgbGlobalPreset {
    juce::String name;
    std::map<int, RgbMapping> padMappings;
    std::map<int, RgbMapping> fxMappings;
};

class RgbManager
{
public:
    RgbManager(SerialManager& serial);
    ~RgbManager() = default;

    // Lighting Palette (Available effects)
    void addLightingEffect(const RgbPreset& effect);
    const std::vector<RgbPreset>& getLightingEffects() const { return lightingEffects; }
    
    // Mappings
    void setPadMapping(int padIndex, const RgbMapping& mapping);
    void clearPadMapping(int padIndex);
    const RgbMapping& getPadMapping(int padIndex) const;
    const std::map<int, RgbMapping>& getAllPadMappings() const { return padMappings; }
    
    void setFxMapping(int fxIndex, const RgbMapping& mapping);
    void clearFxMapping(int fxIndex);
    const RgbMapping& getFxMapping(int fxIndex) const;
    const std::map<int, RgbMapping>& getAllFxMappings() const { return fxMappings; }

    void triggerRgb(const RgbMapping& mapping, bool active);
    
    // Global Presets (Sessions)
    void saveGlobalPreset(const juce::String& name);
    void loadGlobalPreset(const juce::String& name);
    void deleteGlobalPreset(const juce::String& name);
    juce::StringArray getGlobalPresetNames() const;
    
    void setMidiOutput(bool useMidi) { useMidiOutput = useMidi; }
    bool isMidiOutput() const { return useMidiOutput; }

    void sendRgbCommand(const juce::String& cmd);
    void sendColor(juce::Colour c);

private:
    SerialManager& serialManager;
    std::vector<RgbPreset> lightingEffects;
    std::map<int, RgbMapping> padMappings;
    std::map<int, RgbMapping> fxMappings;
    
    std::vector<RgbGlobalPreset> globalPresets;
    bool useMidiOutput = false;
};
