#pragma once

#include <JuceHeader.h>
#include <memory>
#include <map>

// Forward declare to avoid pulling all of the sandbox headers into every TU
class ControllerSandboxWindow;

#include "TridjsLookAndFeel.h"
#include "HandFreeComponent.h"

#include "HeaderComponent.h"
#include "PadsGridComponent.h"
#include "FxRackComponent.h"
#include "AudioCore.h"
#include "SideBrowserComponent.h"
#include "FooterComponent.h"
#include "PersistenceManager.h"
#include "InputManager.h"
#include "RgbManager.h"
#include "MixerComponent.h"
#include "TrackDatabase.h"
#include "AnalysisManager.h"
#include "TrackBrowserComponent.h"
#include "DriveManager.h"
#include "Controllers/Core/ControllerRouter.h"
#include "Controllers/Core/MixxxMappingParser.h"
#include "Controllers/Professional/Mixxx/MixxxJSEngine.h"
#include "Controllers/ControllerManagerWindow.h"

class MainComponent  : public juce::AudioAppComponent,
                       public juce::DragAndDropContainer,
                       public juce::Timer,
                       public juce::MenuBarModel,
                       public juce::ApplicationCommandTarget,
                       public juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    void timerCallback() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // MenuBarModel implementation
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;
    
    // ApplicationCommandTarget implementation
    juce::ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands (juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;
    bool perform (const InvocationInfo& info) override;
    
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    enum CommandIDs
    {
        navFx = 0x1000,
        navFxTouch,
        navRgb,
        navLearn,
        navSerial,
        navConfig
    };

    void saveAllSettings();
    void loadAllSettings();
    void resetPitch();
    void updatePitchFromExtern(float v);
    void incrementPitch(float delta);
    
    void setLayoutMode(int mode);
    void showAboutWindow();
    void showLicenseWindow();
    void showDonateWindow();
    void navegarParaAba(int tabIndex);
    void setSaveOnQuit(bool shouldSave) { saveOnQuit = shouldSave; }
    void openControllerSandbox();
    void showControllerManager();
    void loadControllerMapping(const juce::File& xmlFile);

private:
    bool saveOnQuit = true;
    void loadMidiMappingFromFile (const juce::File& file);
    void applyMidiAction(int rowIdx, float value);
    void handleMasterRecordingFinalization(const juce::File& tempFile);

    TridjsLookAndFeel customTheme;
    AudioCore audioEngine;
    PersistenceManager persistence;
    
    // Data & Analysis (Shared)
    TrackDatabase trackDb;
    AnalysisManager analysisManager { trackDb };
    std::unique_ptr<TrackBrowserComponent> browser;

    // UI Layouts
    enum class LayoutMode { HandFree, Mixer };
    LayoutMode currentMode = LayoutMode::HandFree;

    std::unique_ptr<juce::MenuBarComponent> menuBar;
    
    // Components
    // I'll use the external HandFreeComponent
    std::unique_ptr<class HandFreeComponent> handFreeComp;
    
    std::unique_ptr<MixerComponent> mixerComp;

    // Input Management
    InputManager inputManager;
    RgbManager rgbManager;
    DriveManager driveManager;
    std::map<int, juce::String> midiMappings; // RowIndex -> "CC X" or "Note X"


    // Recording
    juce::TimeSliceThread backgroundThread { "Audio Recorder Thread" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    std::unique_ptr<juce::FileChooser> fileChooser;
    double currentSampleRate = 44100.0;
    
    juce::ApplicationCommandManager commandManager;
    std::unique_ptr<ControllerSandboxWindow> sandboxWindow;

    // Controller Integration
    ControllerRouter controllerRouter;
    MixxxMappingParser mixxxParser;
    std::unique_ptr<MixxxJSEngine> jsEngine;
    std::map<int, std::map<int, int>> msbState; 
    void processControllerEvents();
    void loadFlx10Mapping();
    void autoDetectController();
    bool flx10MappingLoaded = false;

    std::unique_ptr<ControllerManagerWindow> controllerManagerWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
