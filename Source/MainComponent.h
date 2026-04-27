#pragma once

#include <JuceHeader.h>
#include <memory>
#include <map>

#include "TridjsLookAndFeel.h"

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
                       public juce::Timer
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

    void saveAllSettings();
    void loadAllSettings();

private:
    void loadMidiMappingFromFile (const juce::File& file);
    void applyMidiAction(int rowIdx, float value);

    TridjsLookAndFeel customTheme;
    AudioCore audioEngine;
    PersistenceManager persistence;
    
    HeaderComponent header;
    StemsComponent stems;
    PadsGridComponent gridPads;
    FxRackComponent fxRack;
    FooterComponent footer;
    SideBrowserComponent sideBrowser;

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
