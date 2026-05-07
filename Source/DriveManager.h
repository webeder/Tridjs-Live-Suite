#include <JuceHeader.h>
#pragma once

#include <vector>

/**
 * Gerenciador de Dispositivos de Armazenamento.
 * Classifica unidades em Fixas e Removíveis e monitora mudanças em tempo real.
 */
class DriveManager : public juce::ChangeBroadcaster,
                     private juce::Timer {

public:
    struct DriveInfo {
        juce::File root;
        juce::String label;
        bool isRemovable;
    };

    DriveManager();
    ~DriveManager();

    /** Retorna apenas discos rígidos/SSDs internos */
    std::vector<DriveInfo> getFixedDrives() const;

    /** Retorna apenas pendrives e HDs externos */
    std::vector<DriveInfo> getRemovableDrives() const;

    /** Atualiza a lista de drives manualmente */
    void refresh();

    /** Chamado internamente para processar mudanças de hardware */
    void handleDeviceChange(juce::uint64 eventType);

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    void updateDriveList();
    void timerCallback() override;

    bool isScanning = false;


    mutable juce::CriticalSection driveLock;
    std::vector<DriveInfo> fixedDrives;
    std::vector<DriveInfo> removableDrives;

    JUCE_DECLARE_WEAK_REFERENCEABLE(DriveManager)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DriveManager)
};

