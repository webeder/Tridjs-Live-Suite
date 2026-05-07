#pragma once
#include <JuceHeader.h>
#include <map>
#include "RgbManager.h"

class PersistenceManager
{
public:
    PersistenceManager()
    {
        createFolders();
    }

    void createFolders()
    {
        auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        appDir.getChildFile("Mappers").createDirectory();
        appDir.getChildFile("data").createDirectory();
    }

    void saveSettings(float masterVol, float trackVol, int inputMode = 0, const juce::String& serialPort = "", bool serialLogEnabled = true)
    {
        juce::XmlElement xml("project");
        xml.setAttribute("version", "1.0");
        
        auto* settings = xml.createNewChildElement("SETTINGS");
        settings->setAttribute("masterVolume", (double)masterVol);
        settings->setAttribute("trackVolume", (double)trackVol);
        settings->setAttribute("inputMode", inputMode);
        settings->setAttribute("serialPort", serialPort);
        settings->setAttribute("serialLogEnabled", serialLogEnabled);
        
        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory().getChildFile("settings.xml");
        xml.writeTo(file);
    }


    void saveDatabase(const juce::StringArray& trackPaths)
    {
        juce::XmlElement xml("project");
        xml.setAttribute("version", "1.0");
        auto* db = xml.createNewChildElement("DATABASE");
        for (auto& path : trackPaths)
        {
            auto* track = db->createNewChildElement("TRACK");
            track->setAttribute("path", path);
        }
        
        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory().getChildFile("data/database.xml");
        xml.writeTo(file);
    }

    void saveMidiMapping(const juce::String& fileName, const juce::String& deviceName, const std::map<int, juce::String>& mappings)
    {
        juce::XmlElement xml("project");
        xml.setAttribute("version", "1.0");
        auto* mapNode = xml.createNewChildElement("MIDI_MAP");
        mapNode->setAttribute("device", deviceName);
        
        for (auto const& [rowIdx, identifier] : mappings)
        {
            auto* node = mapNode->createNewChildElement("MAPPING");
            node->setAttribute("row", rowIdx);
            node->setAttribute("id", identifier);
        }
        
        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory().getChildFile("Mappers").getChildFile(fileName + ".xml");
        xml.writeTo(file);
    }

    juce::XmlElement* loadMidiMapping(const juce::File& file)
    {
        return juce::XmlDocument::parse(file).release();
    }

    void saveRgbSettings(const std::vector<RgbPreset>& presets, 
                        const std::map<int, RgbMapping>& padMappings,
                        const std::map<int, RgbMapping>& fxMappings)
    {
        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory().getChildFile("rgb_settings.xml");
        
        juce::XmlElement xml("project");
        xml.setAttribute("version", "1.0");
        auto* rgbNode = xml.createNewChildElement("RGB_SETTINGS");
        
        auto* presetsNode = rgbNode->createNewChildElement("PRESETS");
        for (const auto& p : presets)
        {
            auto* node = presetsNode->createNewChildElement("PRESET");
            node->setAttribute("name", p.displayName);
            node->setAttribute("isFixedColor", p.isFixedColor);
            node->setAttribute("color", p.color.toDisplayString(true));
            node->setAttribute("command", p.command);
        }

        auto* padsNode = rgbNode->createNewChildElement("PAD_MAPPINGS");
        for (auto const& [idx, m] : padMappings)
        {
            auto* node = padsNode->createNewChildElement("PAD");
            node->setAttribute("index", idx);
            node->setAttribute("type", (int)m.type);
            node->setAttribute("preset", m.presetName);
            node->setAttribute("color", m.fixedColor.toDisplayString(true));
            node->setAttribute("command", m.directCommand);
        }

        auto* fxNode = rgbNode->createNewChildElement("FX_MAPPINGS");
        for (auto const& [idx, m] : fxMappings)
        {
            auto* node = fxNode->createNewChildElement("FX");
            node->setAttribute("index", idx);
            node->setAttribute("type", (int)m.type);
            node->setAttribute("preset", m.presetName);
            node->setAttribute("color", m.fixedColor.toDisplayString(true));
            node->setAttribute("command", m.directCommand);
        }

        xml.writeTo(file);
    }

    juce::XmlElement* loadRgbSettings()
    {
        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory().getChildFile("rgb_settings.xml");
        if (!file.existsAsFile()) return nullptr;
        return juce::XmlDocument::parse(file).release();
    }

    juce::XmlElement* loadSettings()
    {
        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory().getChildFile("settings.xml");
        return juce::XmlDocument::parse(file).release();
    }

    void saveAudioDeviceState(juce::AudioDeviceManager& deviceManager)
    {
        if (auto xml = deviceManager.createStateXml())
        {
            auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                            .getParentDirectory().getChildFile("audio_settings.xml");
            xml->writeTo(file);
        }
    }

    void loadAudioDeviceState(juce::AudioDeviceManager& deviceManager)
    {
        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory().getChildFile("audio_settings.xml");
        if (file.existsAsFile())
        {
            if (auto xml = juce::XmlDocument::parse(file))
                deviceManager.initialise(0, 2, xml.get(), true);
        }
    }

    struct PadState { juce::String path; bool isLooping; };
    void savePadAssignments(const std::vector<PadState>& padStates)
    {
        juce::XmlElement xml("project");
        xml.setAttribute("version", "1.0");
        auto* pads = xml.createNewChildElement("PAD_ASSIGNMENTS");
        for (int i = 0; i < (int)padStates.size(); ++i) {
            auto* p = pads->createNewChildElement("PAD");
            p->setAttribute("idx", i);
            p->setAttribute("file", padStates[i].path);
            p->setAttribute("loop", padStates[i].isLooping);
        }
        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("data/pads.xml");
        xml.writeTo(file);
    }

    juce::XmlElement* loadPadAssignments()
    {
        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("data/pads.xml");
        return juce::XmlDocument::parse(file).release();
    }

    struct DeckState {
        juce::String loadedFile;
        float gain = 1.0f;
        float eqHigh = 0.0f;
        float eqMid = 0.0f;
        float eqLow = 0.0f;
        float filter = 0.0f;
        float volume = 1.0f;
        float pitch = 0.0f;
        bool syncEnabled = false;
        double loopStart = 0.0;
        double loopLength = 0.0;
        bool loopEnabled = false;
        
        struct FX { float amount = 0.5f; bool enabled = false; };
        std::vector<FX> fxSlots;
        
        DeckState() { fxSlots.resize(6); }
    };

    struct MixerState {
        DeckState deckA;
        DeckState deckB;
        float crossfader = 0.5f;
    };

    void saveMixerState(const MixerState& state)
    {
        juce::XmlElement xml("project");
        xml.setAttribute("version", "1.0");
        
        auto* mixerNode = xml.createNewChildElement("MIXER_STATE");
        mixerNode->setAttribute("crossfader", (double)state.crossfader);

        auto saveDeck = [&](const juce::String& name, const DeckState& ds) {
            auto* node = mixerNode->createNewChildElement(name);
            node->setAttribute("loadedFile", ds.loadedFile);
            node->setAttribute("gain", (double)ds.gain);
            node->setAttribute("eqHigh", (double)ds.eqHigh);
            node->setAttribute("eqMid", (double)ds.eqMid);
            node->setAttribute("eqLow", (double)ds.eqLow);
            node->setAttribute("filter", (double)ds.filter);
            node->setAttribute("volume", (double)ds.volume);
            node->setAttribute("pitch", (double)ds.pitch);
            node->setAttribute("syncEnabled", ds.syncEnabled);
            node->setAttribute("loopStart", ds.loopStart);
            node->setAttribute("loopLength", ds.loopLength);
            node->setAttribute("loopEnabled", ds.loopEnabled);
            
            auto* fxNode = node->createNewChildElement("FX_SLOTS");
            for (size_t i = 0; i < ds.fxSlots.size(); ++i) {
                auto* slot = fxNode->createNewChildElement("FX");
                slot->setAttribute("idx", (int)i);
                slot->setAttribute("amount", (double)ds.fxSlots[i].amount);
                slot->setAttribute("enabled", ds.fxSlots[i].enabled);
            }
        };

        saveDeck("DECK_A", state.deckA);
        saveDeck("DECK_B", state.deckB);

        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory().getChildFile("data/mixer_state.xml");
        xml.writeTo(file);
    }

    bool loadMixerState(MixerState& state)
    {
        auto file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                        .getParentDirectory().getChildFile("data/mixer_state.xml");
        if (!file.existsAsFile()) return false;

        std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
        if (xml == nullptr) return false;

        auto* mixerNode = xml->getChildByName("MIXER_STATE");
        if (mixerNode == nullptr) return false;

        state.crossfader = (float)mixerNode->getDoubleAttribute("crossfader", 0.5);

        auto loadDeck = [&](const juce::String& name, DeckState& ds) {
            auto* node = mixerNode->getChildByName(name);
            if (node == nullptr) return;
            
            ds.loadedFile = node->getStringAttribute("loadedFile");
            ds.gain = (float)node->getDoubleAttribute("gain", 1.0);
            ds.eqHigh = (float)node->getDoubleAttribute("eqHigh", 0.0);
            ds.eqMid = (float)node->getDoubleAttribute("eqMid", 0.0);
            ds.eqLow = (float)node->getDoubleAttribute("eqLow", 0.0);
            ds.filter = (float)node->getDoubleAttribute("filter", 0.0);
            ds.volume = (float)node->getDoubleAttribute("volume", 1.0);
            ds.pitch = (float)node->getDoubleAttribute("pitch", 0.0);
            ds.syncEnabled = node->getBoolAttribute("syncEnabled", false);
            ds.loopStart = node->getDoubleAttribute("loopStart", 0.0);
            ds.loopLength = node->getDoubleAttribute("loopLength", 0.0);
            ds.loopEnabled = node->getBoolAttribute("loopEnabled", false);
            
            if (auto* fxNode = node->getChildByName("FX_SLOTS")) {
                for (auto* slot = fxNode->getFirstChildElement(); slot != nullptr; slot = slot->getNextElement()) {
                    int idx = slot->getIntAttribute("idx", -1);
                    if (idx >= 0 && idx < (int)ds.fxSlots.size()) {
                        ds.fxSlots[idx].amount = (float)slot->getDoubleAttribute("amount", 0.5);
                        ds.fxSlots[idx].enabled = slot->getBoolAttribute("enabled", false);
                    }
                }
            }
        };

        loadDeck("DECK_A", state.deckA);
        loadDeck("DECK_B", state.deckB);

        return true;
    }

private:
};
