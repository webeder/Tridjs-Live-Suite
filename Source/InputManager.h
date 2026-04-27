#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include "SerialManager.h"

class InputManager : public juce::MidiInputCallback
{
public:
    enum class InputMode
    {
        MIDI,
        SERIAL
    };

    InputManager();
    ~InputManager() override;

    void setInputMode(InputMode mode);
    InputMode getInputMode() const { return currentMode; }
    
    void setLearning(bool learning) { isLearning = learning; }

    // MIDI Methods
    void setMidiInput(const juce::MidiDeviceInfo& device);
    juce::String getCurrentMidiDeviceName() const;

    // Serial Methods
    bool openSerialPort(const juce::String& portName, int baudRate = 115200);
    void closeSerialPort();
    bool isSerialOpened() const { return serialManager.isOpened(); }
    juce::StringArray getAvailableSerialPorts() { return SerialManager::listAvailablePorts(); }
    SerialManager& getSerialManager() { return serialManager; }

    // Callbacks
    std::function<void(int note, float velocity)> onNoteOn;
    std::function<void(int note)> onNoteOff;
    std::function<void(int controller, float value)> onCC;
    std::function<void(const juce::String& status)> onStatusMessage;
    std::function<void(const juce::String& rawData)> onRawDataReceived;

    // From MidiInputCallback
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

private:
    void handleSerialLine(const juce::String& line);

    InputMode currentMode = InputMode::MIDI;
    bool isLearning = false;
    std::unique_ptr<juce::MidiInput> midiInput;
    SerialManager serialManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputManager)
};
