#include "InputManager.h"

InputManager::InputManager()
{
    serialManager.onLineReceived = [this](const juce::String& line) {
        if (currentMode == InputMode::SERIAL || isLearning)
        {
            if (onRawDataReceived) onRawDataReceived(line);
            handleSerialLine(line);
        }
    };

    serialManager.onStatusMessage = [this](const juce::String& status) {
        if (onStatusMessage) onStatusMessage(status);
    };
}

InputManager::~InputManager()
{
    if (midiInput) midiInput->stop();
    midiInput.reset();
}

void InputManager::setInputMode(InputMode mode)
{
    currentMode = mode;
    if (onStatusMessage) 
        onStatusMessage("Modo de entrada: " + juce::String(mode == InputMode::MIDI ? "MIDI" : "SERIAL"));
}

void InputManager::setMidiInput(const juce::MidiDeviceInfo& device)
{
    if (midiInput) midiInput->stop();
    midiInput.reset();

    midiInput = juce::MidiInput::openDevice(device.identifier, this);
    if (midiInput) midiInput->start();
}

juce::String InputManager::getCurrentMidiDeviceName() const
{
    return midiInput ? midiInput->getName() : "None";
}

bool InputManager::openSerialPort(const juce::String& portName, int baudRate)
{
    return serialManager.open(portName, baudRate);
}

void InputManager::closeSerialPort()
{
    serialManager.close();
}

void InputManager::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    if (currentMode != InputMode::MIDI && !isLearning) return;

    if (message.isNoteOn())
    {
        if (onNoteOn) onNoteOn(message.getNoteNumber(), message.getVelocity() / 127.0f);
    }
    else if (message.isNoteOff())
    {
        if (onNoteOff) onNoteOff(message.getNoteNumber());
    }
    else if (message.isController())
    {
        if (onCC) onCC(message.getControllerNumber(), message.getControllerValue() / 127.0f);
    }
}

void InputManager::handleSerialLine(const juce::String& line)
{
    // Protocolo:
    // NOTE_ON:nota:velocity
    // NOTE_OFF:nota
    // CC:controlador:valor

    juce::StringArray tokens;
    tokens.addTokens(line, ":", "");

    if (tokens.size() < 2) return;

    juce::String cmd = tokens[0].trim();

    if (cmd == "NOTE_ON" && tokens.size() >= 3)
    {
        int note = tokens[1].getIntValue();
        int vel = tokens[2].getIntValue();
        if (onNoteOn) onNoteOn(note, vel / 127.0f);
    }
    else if (cmd == "NOTE_OFF")
    {
        int note = tokens[1].getIntValue();
        if (onNoteOff) onNoteOff(note);
    }
    else if (cmd == "CC" && tokens.size() >= 3)
    {
        int ctrl = tokens[1].getIntValue();
        int val = tokens[2].getIntValue();
        if (onCC) onCC(ctrl, val / 127.0f);
    }
}
