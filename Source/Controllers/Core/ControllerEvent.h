#pragma once

#include <JuceHeader.h>

enum class ControllerCapability
{
    RGBPads,
    JogDisplay,
    TouchJog,
    HID,
    SysEx,
    Sampler,
    Stems,
    MotorizedJog,
    Screens,
    GestureInput
};

enum class ControllerEventType
{
    Play,
    Cue,
    Sync,
    Volume,
    Crossfader,
    EQHigh,
    EQMid,
    EQLow,
    Filter,
    Pitch,
    JogScratch,
    JogBend,
    HotCue,
    Unknown
};

struct ControllerInputEvent
{
    ControllerEventType type = ControllerEventType::Unknown;
    int deckIndex = 0;   // 0 for Deck A, 1 for Deck B, etc.
    float value = 0.0f;  // Normalized 0.0 to 1.0 or specific delta
    int parameter = 0;   // E.g., HotCue index
};

struct ControllerOutputEvent
{
    ControllerEventType type = ControllerEventType::Unknown;
    int deckIndex = 0;
    float value = 0.0f;
    int parameter = 0;
};
