#include <JuceHeader.h>
#include "DriveManager.h"


#ifdef JUCE_WIN32
#define NOMINMAX
#include <Windows.h>
#include <Dbt.h>

/** Janela oculta para capturar mensagens do Windows */
class DeviceChangeWindow : public juce::Component {
public:
    DeviceChangeWindow(DriveManager& m) : manager(m) {
        setOpaque(false);
        addToDesktop(0); 
    }

    ~DeviceChangeWindow() {
        removeFromDesktop();
    }

    LRESULT windowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override {
        if (msg == WM_DEVICECHANGE) {
            if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
                manager.handleDeviceChange(static_cast<juce::uint64>(wParam));
            }
        }
        return juce::Component::windowProc(hWnd, msg, wParam, lParam);
    }

private:
    DriveManager& manager;
};
#endif

struct DriveManager::Pimpl {
#ifdef JUCE_WIN32
    std::unique_ptr<DeviceChangeWindow> msgWindow;
#endif
};

DriveManager::DriveManager() : pimpl(std::make_unique<Pimpl>()) {
/*
#ifdef JUCE_WIN32
    pimpl->msgWindow = std::make_unique<DeviceChangeWindow>(*this);
#endif
*/
    updateDriveList();
}

DriveManager::~DriveManager() {
    stopTimer();
    pimpl = nullptr;
}

std::vector<DriveManager::DriveInfo> DriveManager::getFixedDrives() const {
    juce::ScopedLock sl(driveLock);
    return fixedDrives;
}

std::vector<DriveManager::DriveInfo> DriveManager::getRemovableDrives() const {
    juce::ScopedLock sl(driveLock);
    return removableDrives;
}

void DriveManager::refresh() {
    // updateDriveList(); // DISABLED
}

void DriveManager::handleDeviceChange(juce::uint64 /*eventType*/) {
    // DISABLED
}

void DriveManager::timerCallback() {
    stopTimer();
}

void DriveManager::updateDriveList() {
    isScanning = false;
    sendChangeMessage();
}

