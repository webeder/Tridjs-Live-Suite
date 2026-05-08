#pragma once

#include <JuceHeader.h>
#include "Core/ControllerEvent.h"

// ============================================================
// MockAudioEngine
// A completely isolated mock that receives ControllerInputEvents
// and logs them to a callback. Does NOT touch AudioCore.
// ============================================================
class MockAudioEngine
{
public:
    using LogCallback = std::function<void(const juce::String&)>;

    void setLogCallback(LogCallback cb) { logCallback = cb; }

    void processEvent(const ControllerInputEvent& event)
    {
        juce::String msg;

        switch (event.type)
        {
            case ControllerEventType::Play:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> PLAY  (value=" + juce::String(event.value) + ")";
                deckPlayState[event.deckIndex] = event.value > 0.5f;
                break;

            case ControllerEventType::Cue:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> CUE";
                break;

            case ControllerEventType::Sync:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> SYNC  (value=" + juce::String(event.value) + ")";
                break;

            case ControllerEventType::Volume:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> VOLUME  = " + juce::String(event.value, 3);
                deckVolume[event.deckIndex] = event.value;
                break;

            case ControllerEventType::Crossfader:
                msg = "[MOCK] CROSSFADER = " + juce::String(event.value, 3);
                break;

            case ControllerEventType::EQHigh:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> EQ HIGH = " + juce::String(event.value, 3);
                break;

            case ControllerEventType::EQMid:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> EQ MID = " + juce::String(event.value, 3);
                break;

            case ControllerEventType::EQLow:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> EQ LOW = " + juce::String(event.value, 3);
                break;

            case ControllerEventType::Filter:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> FILTER = " + juce::String(event.value, 3);
                break;

            case ControllerEventType::Pitch:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> PITCH = " + juce::String(event.value, 3);
                break;

            case ControllerEventType::JogScratch:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> JOG SCRATCH delta=" + juce::String(event.value, 3);
                break;

            case ControllerEventType::JogBend:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> JOG BEND delta=" + juce::String(event.value, 3);
                break;

            case ControllerEventType::HotCue:
                msg = "[MOCK] Deck " + deckName(event.deckIndex) + " -> HOT CUE #" + juce::String(event.parameter);
                break;

            default:
                msg = "[MOCK] UNKNOWN EVENT type=" + juce::String((int)event.type);
                break;
        }

        if (logCallback)
            logCallback(msg);
    }

private:
    LogCallback logCallback;
    bool deckPlayState[4] = { false, false, false, false };
    float deckVolume[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    static juce::String deckName(int idx)
    {
        switch (idx)
        {
            case 0: return "A";
            case 1: return "B";
            case 2: return "C";
            case 3: return "D";
            default: return juce::String(idx);
        }
    }
};
