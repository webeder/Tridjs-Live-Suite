#include "SerialManager.h"
#include <cstring>
#include <SetupAPI.h>
#include <devguid.h>
#include <regstr.h>

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
    
    HDEVINFO hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo != INVALID_HANDLE_VALUE)
    {
        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); ++i)
        {
            char friendlyName[256] = { 0 };
            char hardwareId[512] = { 0 };
            char portName[64] = { 0 };
            
            // 1. Obter Friendly Name (geralmente contém COMx e a Descrição)
            if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)friendlyName, sizeof(friendlyName), NULL))
            {
                juce::String fName(friendlyName);
                juce::String comPort;
                
                // Extrair COMx do FriendlyName ex: "USB Serial Port (COM4)"
                int openParen = fName.lastIndexOf("(");
                int closeParen = fName.lastIndexOf(")");
                if (openParen != -1 && closeParen != -1 && closeParen > openParen)
                {
                    comPort = fName.substring(openParen + 1, closeParen).trim();
                }
                
                if (comPort.startsWithIgnoreCase("COM"))
                {
                    juce::String description = fName.upToFirstOccurrenceOf(" (", false, false).trim();
                    juce::String vidPidInfo;
                    
                    // 2. Obter Hardware ID para extrair VID/PID
                    if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_HARDWAREID, NULL, (PBYTE)hardwareId, sizeof(hardwareId), NULL))
                    {
                        juce::String hwId(hardwareId);
                        int vidPos = hwId.indexOfIgnoreCase("VID_");
                        int pidPos = hwId.indexOfIgnoreCase("PID_");
                        
                        if (vidPos != -1 && pidPos != -1)
                        {
                            juce::String vid = hwId.substring(vidPos + 4, vidPos + 8).toUpperCase();
                            juce::String pid = hwId.substring(pidPos + 4, pidPos + 8).toUpperCase();
                            vidPidInfo = " (VID:" + vid + " PID:" + pid + ")";
                        }
                    }
                    
                    ports.add(comPort + " - " + description + vidPidInfo);
                }
            }
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }

    // Fallback se o SetupAPI falhar por algum motivo
    if (ports.isEmpty())
    {
        char buffer[65535];
        if (QueryDosDeviceA(NULL, buffer, sizeof(buffer)))
        {
            char* p = buffer;
            while (*p)
            {
                juce::String name(p);
                if (name.startsWithIgnoreCase("COM")) ports.add(name);
                p += strlen(p) + 1;
            }
        }
    }
    
    ports.sort(true);
    return ports;
}

bool SerialManager::open(const juce::String& fullPortString, int baudRate)
{
    close();

    // Extrair apenas o nome da porta (ex: COM4) se vier o formato longo
    juce::String portName = fullPortString;
    if (fullPortString.contains(" - "))
        portName = fullPortString.upToFirstOccurrenceOf(" - ", false, false).trim();

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
                if (onDataReceived) onDataReceived(buffer, (int)bytesRead);
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
void SerialManager::sendRawData(const char* data, int size)
{
    if (hSerial == INVALID_HANDLE_VALUE) return;
    juce::ScopedLock sl(txLock);
    binaryTxQueue.add(juce::MemoryBlock(data, size));
}

void SerialManager::processTxQueue()
{
    juce::StringArray queueCopy;
    juce::Array<juce::MemoryBlock> binaryCopy;
    {
        juce::ScopedLock sl(txLock);
        if (txQueue.isEmpty() && binaryTxQueue.isEmpty()) return;
        queueCopy = txQueue;
        txQueue.clear();
        binaryCopy = binaryTxQueue;
        binaryTxQueue.clear();
    }

    // Processar String Queue (Protocolo Legado)
    for (const auto& msg : queueCopy)
    {
        juce::String toSend = msg;
        if (!toSend.endsWith("\n")) toSend += "\n";
        
        DWORD bytesWritten;
        WriteFile(hSerial, toSend.toRawUTF8(), (DWORD)toSend.length(), &bytesWritten, NULL);
        
        if (onRawDataSent) 
        {
            juce::String debugMsg = toSend.trim();
            juce::MessageManager::callAsync([this, debugMsg] {
                if (onRawDataSent) onRawDataSent(debugMsg);
            });
        }
    }

    // Processar Binary Queue (MIDI e Novos Comandos)
    for (const auto& block : binaryCopy)
    {
        DWORD bytesWritten;
        WriteFile(hSerial, block.getData(), (DWORD)block.getSize(), &bytesWritten, NULL);
        
        if (onRawDataSent) 
        {
            juce::String hex;
            const uint8_t* d = (const uint8_t*)block.getData();
            for (int i=0; i < (int)block.getSize(); ++i) hex += juce::String::toHexString((int)d[i]) + " ";
            juce::String debugMsg = "TX MIDI: " + hex.trim().toUpperCase();
            
            juce::MessageManager::callAsync([this, debugMsg] {
                if (onRawDataSent) onRawDataSent(debugMsg);
            });
        }
    }
}
