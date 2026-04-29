#pragma once

#include <JuceHeader.h>
#include <memory>
#include <map>

#include "TridjsLookAndFeel.h"
#include "HandFreeComponent.h"

#include "HeaderComponent.h"
#include "StemsComponent.h"
#include "PadsGridComponent.h"
#include "FxRackComponent.h"
#include "AudioCore.h"
#include "SideBrowserComponent.h"
#include "FooterComponent.h"
#include "PersistenceManager.h"
#include "InputManager.h"
#include "RgbManager.h"

class MainComponent  : public juce::AudioAppComponent,
                       public juce::DragAndDropContainer,
                       public juce::Timer,
                       public juce::MenuBarModel
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

private:
    void loadMidiMappingFromFile (const juce::File& file);
    void applyMidiAction(int rowIdx, float value);

    TridjsLookAndFeel customTheme;
    AudioCore audioEngine;
    PersistenceManager persistence;
    
    // UI Layouts
    enum class LayoutMode { HandFree, Mixer };
    LayoutMode currentMode = LayoutMode::HandFree;

    std::unique_ptr<juce::MenuBarComponent> menuBar;
    
    // Components
    // I'll use the external HandFreeComponent
    std::unique_ptr<class HandFreeComponent> handFreeComp;
    
    struct MixerPlaceholder : public juce::Component {
        MixerPlaceholder() {
            addAndMakeVisible(label);
            label.setText("DJ MIXER - EM BREVE", juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(juce::Font(24.0f, juce::Font::bold));
            label.setColour(juce::Label::textColourId, juce::Colours::grey);
        }
        void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black.withAlpha(0.8f)); }
        void resized() override { label.setBounds(getLocalBounds()); }
        juce::Label label;
    };
    MixerPlaceholder mixerPlaceholder;

    // Input Management
    InputManager inputManager;
    RgbManager rgbManager;
    std::map<int, juce::String> midiMappings; // RowIndex -> "CC X" or "Note X"


    // Recording
    juce::TimeSliceThread backgroundThread { "Audio Recorder Thread" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    std::unique_ptr<juce::FileChooser> fileChooser;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
