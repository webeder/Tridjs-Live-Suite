#pragma once

#include <JuceHeader.h>
#include "ControllerEvent.h"

struct ControllerProfile
{
    juce::String name;
    int vendorId = 0;
    int productId = 0;
    juce::String mappingPath;
    juce::String connectionType; // "MIDI", "HID", "Serial", "BLE"
    int numDecks = 2;
    
    // Static capabilities based on the JSON
    bool supportsRGB = false;
    bool supportsJogDisplay = false;
    bool supportsHID = false;
    bool supportsSysEx = false;
    
    // Dynamic capabilities list
    juce::Array<ControllerCapability> capabilities;

    // Helper to check capability
    bool hasCapability(ControllerCapability cap) const
    {
        return capabilities.contains(cap);
    }
};
