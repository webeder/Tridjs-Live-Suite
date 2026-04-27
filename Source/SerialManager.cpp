#include "SerialManager.h"
#include <cstring>

SerialManager::SerialManager() : Thread("SerialReaderThread")
{
}

SerialManager::~SerialManager()
{
    close();
}

juce::StringArray SerialManager::listAvailablePorts()
{
    juce::StringArray ports;
    char buffer[65535];
    if (QueryDosDeviceA(NULL, buffer, sizeof(buffer)))
    {
        char* p = buffer;
        while (*p)
        {
            juce::String name(p);
            if (name.startsWithIgnoreCase("COM"))
            {
                ports.add(name);
            }
            p += strlen(p) + 1;
        }
    }
    ports.sort(true);
    return ports;
}

bool SerialManager::open(const juce::String& portName, int baudRate)
{
    close();

    currentPort = portName;
    currentBaud = baudRate;

    juce::String fullPath = "\\\\.\\" + portName;
    hSerial = CreateFileA(fullPath.toRawUTF8(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hSerial == INVALID_HANDLE_VALUE)
    {
        if (onStatusMessage) onStatusMessage("Erro ao abrir " + portName);
        return false;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams))
    {
        if (onStatusMessage) onStatusMessage("Erro ao obter estado da serial");
        close();
        return false;
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams))
    {
        if (onStatusMessage) onStatusMessage("Erro ao configurar serial");
        close();
        return false;
    }

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hSerial, &timeouts))
    {
        if (onStatusMessage) onStatusMessage("Erro ao configurar timeouts");
        close();
        return false;
    }

    shouldExit = false;
    startThread();

    if (onStatusMessage) onStatusMessage("Conectado em " + portName);
    return true;
}

void SerialManager::close()
{
    shouldExit = true;
    stopThread(1000);

    if (hSerial != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
        if (onStatusMessage) onStatusMessage("Serial desconectada");
    }
}

void SerialManager::run()
{
    char buffer[256];
    DWORD bytesRead;

    while (!threadShouldExit() && !shouldExit)
    {
        processTxQueue();

        if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL))
        {
            if (bytesRead > 0)
            {
                processBuffer(buffer, (int)bytesRead);
            }
        }
        else
        {
            // Erro de leitura - provável desconexão
            shouldExit = true;
            juce::MessageManager::callAsync([this] {
                if (onStatusMessage) onStatusMessage("Erro de leitura: porta desconectada?");
                // Não chama close() aqui para evitar deadlock, 
                // apenas limpa o handle se necessário
            });
            break;
        }
        juce::Thread::sleep(10);
    }
}

void SerialManager::processBuffer(const char* data, int size)
{
    for (int i = 0; i < size; ++i)
    {
        char c = data[i];
        if (c == '\n' || c == '\r')
        {
            if (accumulationBuffer.isNotEmpty())
            {
                juce::String line = accumulationBuffer.trim();
                if (line.isNotEmpty() && onLineReceived)
                {
                    onLineReceived(line);
                }
                accumulationBuffer = "";
            }
        }
        else
        {
            accumulationBuffer += c;
        }
    }
}

void SerialManager::sendString(const juce::String& text)
{
    if (hSerial == INVALID_HANDLE_VALUE) return;
    
    juce::ScopedLock sl(txLock);
    // Evitar flood de comandos idênticos repetidos
    if (txQueue.size() > 0 && txQueue.getReference(txQueue.size() - 1) == text) return;
    
    txQueue.add(text);
}

void SerialManager::processTxQueue()
{
    juce::StringArray queueCopy;
    {
        juce::ScopedLock sl(txLock);
        if (txQueue.isEmpty()) return;
        queueCopy = txQueue;
        txQueue.clear();
    }

    for (const auto& msg : queueCopy)
    {
        juce::String toSend = msg;
        if (!toSend.endsWith("\n")) toSend += "\n";
        
        DWORD bytesWritten;
        WriteFile(hSerial, toSend.toRawUTF8(), (DWORD)toSend.length(), &bytesWritten, NULL);
        FlushFileBuffers(hSerial);
        
        if (onRawDataSent) 
        {
            juce::String debugMsg = toSend.trim();
            juce::MessageManager::callAsync([this, debugMsg] {
                if (onRawDataSent) onRawDataSent(debugMsg);
            });
        }
    }
}
