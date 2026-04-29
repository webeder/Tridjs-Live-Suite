#pragma once

#include <JuceHeader.h>
#define NOMINMAX
#include <windows.h>
#include <thread>
#include <atomic>
#include <functional>

class SerialManager : public juce::Thread
{
public:
    SerialManager();
    ~SerialManager() override;

    static juce::StringArray listAvailablePorts();

    bool open(const juce::String& portName, int baudRate = 115200);
    void close();
    bool isOpened() const { return hSerial != INVALID_HANDLE_VALUE; }
    juce::String getCurrentPort() const { return currentPort; }

    void run() override;

    std::function<void(const juce::String&)> onLineReceived;
    std::function<void(const juce::String&)> onStatusMessage;
    std::function<void(const juce::String&)> onRawDataSent;
    std::function<void(const char*, int)> onDataReceived;

    void sendString(const juce::String& text);
    void sendRawData(const char* data, int size);

private:
    HANDLE hSerial = INVALID_HANDLE_VALUE;
    std::atomic<bool> shouldExit{ false };
    juce::String currentPort;
    int currentBaud = 115200;

    void processBuffer(const char* data, int size);
    juce::String accumulationBuffer;

    juce::CriticalSection txLock;
    juce::StringArray txQueue;
    juce::Array<juce::MemoryBlock> binaryTxQueue;
    void processTxQueue();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SerialManager)
};
