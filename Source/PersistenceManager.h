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

    void saveSettings(float masterVol, float trackVol, int inputMode = 0, const juce::String& serialPort = "", int baud = 115200, bool someOption = false)
    {
        juce::XmlElement xml("project");
        xml.setAttribute("version", "1.0");
        
        auto* settings = xml.createNewChildElement("SETTINGS");
        settings->setAttribute("masterVolume", (double)masterVol);
        settings->setAttribute("trackVolume", (double)trackVol);
        settings->setAttribute("inputMode", inputMode);
        settings->setAttribute("serialPort", serialPort);
        settings->setAttribute("serialBaud", baud);
        settings->setAttribute("someOption", someOption);
        
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

private:
};
