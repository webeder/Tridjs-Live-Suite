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

    serialManager.onDataReceived = [this](const char* data, int size) {
        if (currentMode == InputMode::SERIAL || isLearning)
        {
            handleSerialData(data, size);
        }
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
    if (source != nullptr && currentMode != InputMode::MIDI && !isLearning) return;
    if (source == nullptr && currentMode != InputMode::SERIAL && !isLearning) return;

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

void InputManager::handleSerialData(const char* data, int size)
{
    for (int i = 0; i < size; ++i)
    {
        uint8_t b = static_cast<uint8_t>(data[i]);

        if (b & 0x80) // Status byte
        {
            midiBuffer[0] = b;
            midiBufferPtr = 1;
            
            // Determinar quantos bytes esperar
            uint8_t type = b & 0xF0;
            if (b >= 0xF8) expectedBytes = 1; // Real-time
            else if (type == 0x80 || type == 0x90 || type == 0xA0 || type == 0xB0 || type == 0xE0)
                expectedBytes = 3;
            else if (type == 0xC0 || type == 0xD0 || b == 0xF1 || b == 0xF3)
                expectedBytes = 2;
            else if (b == 0xF2)
                expectedBytes = 3;
            else
                expectedBytes = 0;

            if (expectedBytes == 1)
            {
                juce::MidiMessage msg(b);
                handleIncomingMidiMessage(nullptr, msg);
                midiBufferPtr = 0; // Don't allow running status for real-time
            }
        }
        else if (midiBufferPtr > 0 && expectedBytes > 0) // Data byte
        {
            if (midiBufferPtr < 3)
                midiBuffer[midiBufferPtr++] = b;
            
            if (midiBufferPtr == expectedBytes)
            {
                juce::MidiMessage msg(midiBuffer[0], midiBuffer[1], expectedBytes > 2 ? midiBuffer[2] : 0);
                
                // Formatar para exibição no Log (Hex)
                juce::String hex;
                for (int j=0; j<expectedBytes; ++j) hex += juce::String::toHexString((int)midiBuffer[j]) + " ";
                if (onRawDataReceived) onRawDataReceived("MIDI: " + hex.trim().toUpperCase());

                // Processar como MIDI
                handleIncomingMidiMessage(nullptr, msg);
                
                // Reset para Running Status
                midiBufferPtr = 1; 
            }
        }
    }
}
