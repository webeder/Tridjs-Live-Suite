#pragma once

#include <JuceHeader.h>
#include <map>

/**
 * Represents a single control mapping from a Mixxx XML file.
 */
struct MixxxControl
{
    juce::String group;
    juce::String key;
    juce::String description;
    
    bool isButton = false;
    bool is14BitMSB = false;
    bool is14BitLSB = false;
    bool isScriptBinding = false;
    
    // Internal state for 14-bit calculation
    int lastMSB = -1;
};

/**
 * Parses Mixxx .midi.xml files and provides lookups for MIDI messages.
 */
class MixxxMappingParser
{
public:
    MixxxMappingParser() = default;

    /** Loads a Mixxx .midi.xml file. Returns true on success. */
    bool loadXml(const juce::File& file)
    {
        juce::XmlDocument doc(file);
        auto root = doc.getDocumentElement();
        
        if (root == nullptr || !root->hasTagName("MixxxControllerPreset"))
            return false;

        std::map<int, MixxxControl> tempMappings;
        
        auto controller = root->getChildByName("controller");
        if (controller == nullptr) return false;
        
        auto controls = controller->getChildByName("controls");
        if (controls == nullptr) return false;

        for (auto* controlNode : controls->getChildIterator())
        {
            if (controlNode->hasTagName("control"))
            {
                MixxxControl ctrl;
                auto getChildValue = [&](const char* tag) {
                    if (auto* e = controlNode->getChildByName(tag))
                        return e->getAllSubText();
                    return juce::String();
                };

                ctrl.group = getChildValue("group").trim();
                ctrl.key = getChildValue("key").trim();
                ctrl.description = getChildValue("description").trim();

                int status = parseHex(getChildValue("status"));
                int midino = parseHex(getChildValue("midino"));

                if (auto* options = controlNode->getChildByName("options"))
                {
                    ctrl.isButton = options->getChildByName("button") != nullptr;
                    ctrl.is14BitMSB = options->getChildByName("fourteen-bit-msb") != nullptr;
                    ctrl.is14BitLSB = options->getChildByName("fourteen-bit-lsb") != nullptr;
                    ctrl.isScriptBinding = options->getChildByName("script-binding") != nullptr;
                }

                int mappingKey = (status << 8) | midino;
                tempMappings[mappingKey] = ctrl;
            }
        }

        if (tempMappings.empty())
            return false;

        // Atomic swap inside lock
        {
            const juce::ScopedLock lock (mappingLock);
            mappings.swap(tempMappings);
        }

        return true;
    }

    /** Looks up a control by status and midino. */
    MixxxControl getControl(int status, int midino) const
    {
        const juce::ScopedLock lock (mappingLock);
        int key = (status << 8) | midino;
        auto it = mappings.find(key);
        if (it != mappings.end())
            return it->second;
        return {}; 
    }

    size_t getMappingCount() const 
    { 
        const juce::ScopedLock lock (mappingLock);
        return mappings.size(); 
    }

private:
    std::map<int, MixxxControl> mappings;
    juce::CriticalSection mappingLock;

    int parseHex(juce::String hex)
    {
        if (hex.startsWithIgnoreCase("0x"))
            return hex.substring(2).getHexValue32();
        return hex.getHexValue32();
    }
};
