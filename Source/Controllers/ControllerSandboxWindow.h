#pragma once

#include <JuceHeader.h>
#include <thread>
#include "Core/ControllerRouter.h"
#include "Core/ControllerEvent.h"
#include "Core/MixxxMappingParser.h"
#include "MockAudioEngine.h"

// ============================================================
// ControllerSandboxWindow
// A self-contained debug window that validates the full pipeline:
//   FLX10 → MIDI In → Mapping → JS Engine → ControllerRouter → MockAudioEngine
//
// Does NOT touch AudioCore, MainComponent, or MixerComponent.
// ============================================================
class ControllerSandboxWindow : public juce::DocumentWindow,
                                private juce::Timer,
                                private juce::MidiInputCallback
{
public:
    ControllerSandboxWindow()
        : DocumentWindow("🎛️  Controller Sandbox — Tridjs",
                         juce::Colour(0xff0d0d0d),
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setResizeLimits(900, 600, 3000, 2000);
        setSize(1100, 700);
        setVisible(true);

        // Build UI
        content = std::make_unique<SandboxContent>(*this);
        setContentOwned(content.get(), false);

        // Wire MockAudioEngine logging to our UI
        mockEngine.setLogCallback([this](const juce::String& msg)
        {
            juce::Component::SafePointer<SandboxContent> safeContent (content.get());
            juce::MessageManager::callAsync([safeContent, msg]()
            {
                if (safeContent != nullptr)
                    safeContent->appendEngineLog(msg);
            });
        });

        startTimer(20); // 50Hz polling of the EventBus
    }

    ~ControllerSandboxWindow()
    {
        stopTimer();
        closeMidiInput();
    }

    void closeButtonPressed() override { setVisible(false); }

    // ============================================================
    // Inner content component
    // ============================================================
    class SandboxContent : public juce::Component,
                           private juce::ComboBox::Listener,
                           private juce::Button::Listener
    {
    public:
        SandboxContent(ControllerSandboxWindow& owner) : window(owner)
        {
            // --- Style ---
            setOpaque(true);

            // --- MIDI Device Selector ---
            addAndMakeVisible(midiDeviceLabel);
            midiDeviceLabel.setText("MIDI Input:", juce::dontSendNotification);
            midiDeviceLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

            addAndMakeVisible(midiDeviceCombo);
            midiDeviceCombo.addListener(this);
            refreshMidiDevices();

            addAndMakeVisible(refreshBtn);
            refreshBtn.setButtonText("Refresh");
            refreshBtn.addListener(this);

            addAndMakeVisible(loadMappingBtn);
            loadMappingBtn.setButtonText("📂 Load Mixxx XML");
            loadMappingBtn.addListener(this);
            loadMappingBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a4a4a));

            // --- Log Panels ---
            setupLogPanel(midiInLog, midiInLabel, "MIDI IN", juce::Colour(0xff003300));
            setupLogPanel(midiOutLog, midiOutLabel, "MIDI OUT", juce::Colour(0xff000033));
            setupLogPanel(eventBusLog, eventBusLabel, "EVENT BUS", juce::Colour(0xff1a0033));
            setupLogPanel(engineLog, engineLabel, "MOCK ENGINE", juce::Colour(0xff330000));

            addAndMakeVisible(clearBtn);
            clearBtn.setButtonText("Clear All Logs");
            clearBtn.addListener(this);

            // --- Inject Test Event ---
            addAndMakeVisible(testPlayBtn);
            testPlayBtn.setButtonText("▶ Test PLAY (Deck A)");
            testPlayBtn.addListener(this);
            testPlayBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a6b1a));

            addAndMakeVisible(testVolumeBtn);
            testVolumeBtn.setButtonText("🎚 Test VOLUME 0.75");
            testVolumeBtn.addListener(this);
            testVolumeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a3a6b));

            addAndMakeVisible(testEqBtn);
            testEqBtn.setButtonText("🎛 Test EQ HIGH 0.9");
            testEqBtn.addListener(this);
            testEqBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff6b3a1a));

            addAndMakeVisible(testCrossBtn);
            testCrossBtn.setButtonText("↔ Test CROSSFADER 0.5");
            testCrossBtn.addListener(this);
            testCrossBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a1a6b));
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff0d0d0d));

            // Header bar
            g.setColour(juce::Colour(0xff1a1a2e));
            g.fillRect(0, 0, getWidth(), 50);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font("", 18.0f, juce::Font::bold));
            g.drawText("🎛️  Controller Sandbox — Tridjs Live Suite", 14, 0, getWidth() - 20, 50, juce::Justification::centredLeft);

            g.setColour(juce::Colour(0xff444466));
            g.drawText("feature/controller-sandbox  |  Isolated validation mode — AudioCore is NOT connected", 14, 32, getWidth() - 20, 16, juce::Justification::centredLeft);
            g.setFont(juce::Font("", 11.0f, juce::Font::italic));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(8);
            area.removeFromTop(52); // header

            // MIDI selector row
            auto topRow = area.removeFromTop(30);
            midiDeviceLabel.setBounds(topRow.removeFromLeft(80));
            refreshBtn.setBounds(topRow.removeFromRight(80).reduced(2));
            loadMappingBtn.setBounds(topRow.removeFromRight(150).reduced(2));
            midiDeviceCombo.setBounds(topRow.reduced(4));

            area.removeFromTop(6);

            // Test buttons row
            auto btnRow = area.removeFromTop(36);
            int bw = btnRow.getWidth() / 4;
            testPlayBtn.setBounds(btnRow.removeFromLeft(bw).reduced(3));
            testVolumeBtn.setBounds(btnRow.removeFromLeft(bw).reduced(3));
            testEqBtn.setBounds(btnRow.removeFromLeft(bw).reduced(3));
            testCrossBtn.setBounds(btnRow.reduced(3));

            area.removeFromTop(6);
            clearBtn.setBounds(area.removeFromRight(120).removeFromTop(28));

            area.removeFromTop(4);

            // 4 log panels in a 2x2 grid
            auto top = area.removeFromTop(area.getHeight() / 2);
            auto bot = area;

            layoutPanel(midiInLabel, midiInLog, top.removeFromLeft(top.getWidth() / 2).reduced(3));
            layoutPanel(midiOutLabel, midiOutLog, top.reduced(3));
            layoutPanel(eventBusLabel, eventBusLog, bot.removeFromLeft(bot.getWidth() / 2).reduced(3));
            layoutPanel(engineLabel, engineLog, bot.reduced(3));
        }

        void appendMidiIn(const juce::String& msg) { appendTo(midiInLog, msg); }
        void appendMidiOut(const juce::String& msg) { appendTo(midiOutLog, msg); }
        void appendEventBus(const juce::String& msg) { appendTo(eventBusLog, msg); }
        void appendEngineLog(const juce::String& msg) { appendTo(engineLog, msg); }

    private:
        ControllerSandboxWindow& window;

        // Header controls
        juce::Label midiDeviceLabel;
        juce::ComboBox midiDeviceCombo;
        juce::TextButton refreshBtn, clearBtn, loadMappingBtn;
        juce::TextButton testPlayBtn, testVolumeBtn, testEqBtn, testCrossBtn;

        // Log panels (label + text area)
        juce::Label midiInLabel, midiOutLabel, eventBusLabel, engineLabel;
        juce::TextEditor midiInLog, midiOutLog, eventBusLog, engineLog;

        void setupLogPanel(juce::TextEditor& editor, juce::Label& label,
                           const juce::String& title, juce::Colour bgColour)
        {
            label.setText(title, juce::dontSendNotification);
            label.setColour(juce::Label::textColourId, juce::Colours::white);
            label.setFont(juce::Font("", 11.0f, juce::Font::bold));
            addAndMakeVisible(label);

            editor.setMultiLine(true);
            editor.setReadOnly(true);
            editor.setScrollbarsShown(true);
            editor.setColour(juce::TextEditor::backgroundColourId, bgColour);
            editor.setColour(juce::TextEditor::textColourId, juce::Colours::lightgreen);
            editor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
            addAndMakeVisible(editor);
        }

        void layoutPanel(juce::Label& label, juce::TextEditor& editor, juce::Rectangle<int> area)
        {
            label.setBounds(area.removeFromTop(16));
            editor.setBounds(area);
        }

        void appendTo(juce::TextEditor& editor, const juce::String& msg)
        {
            auto ts = juce::Time::getCurrentTime().toString(false, true, true, true);
            
            // Limit text length to prevent UI hang
            if (editor.getTotalNumChars() > 5000)
                editor.setText(editor.getText().getLastCharacters(2500), false);

            editor.moveCaretToEnd();
            editor.insertTextAtCaret("[" + ts + "]  " + msg + "\n");
        }

        void refreshMidiDevices()
        {
            midiDeviceCombo.clear();
            midiDeviceCombo.addItem("-- Select MIDI Input --", 1);
            auto devices = juce::MidiInput::getAvailableDevices();
            for (int i = 0; i < devices.size(); ++i)
                midiDeviceCombo.addItem(devices[i].name, i + 2);
            midiDeviceCombo.setSelectedId(1);
        }

        void comboBoxChanged(juce::ComboBox* cb) override
        {
            if (cb == &midiDeviceCombo)
            {
                int idx = midiDeviceCombo.getSelectedItemIndex() - 1;
                if (idx >= 0)
                {
                    auto devices = juce::MidiInput::getAvailableDevices();
                    if (idx < devices.size())
                        window.openMidiInput(devices[idx].identifier);
                }
            }
        }

        void buttonClicked(juce::Button* btn) override
        {
            if (btn == &refreshBtn)
            {
                refreshMidiDevices();
            }
            else if (btn == &loadMappingBtn)
            {
                window.chooseAndLoadMapping();
            }
            else if (btn == &clearBtn)
            {
                midiInLog.clear(); midiOutLog.clear();
                eventBusLog.clear(); engineLog.clear();
            }
            else if (btn == &testPlayBtn)
            {
                ControllerInputEvent ev;
                ev.type = ControllerEventType::Play;
                ev.deckIndex = 0;
                ev.value = 1.0f;
                window.injectEvent(ev);
            }
            else if (btn == &testVolumeBtn)
            {
                ControllerInputEvent ev;
                ev.type = ControllerEventType::Volume;
                ev.deckIndex = 0;
                ev.value = 0.75f;
                window.injectEvent(ev);
            }
            else if (btn == &testEqBtn)
            {
                ControllerInputEvent ev;
                ev.type = ControllerEventType::EQHigh;
                ev.deckIndex = 0;
                ev.value = 0.9f;
                window.injectEvent(ev);
            }
            else if (btn == &testCrossBtn)
            {
                ControllerInputEvent ev;
                ev.type = ControllerEventType::Crossfader;
                ev.deckIndex = 0;
                ev.value = 0.5f;
                window.injectEvent(ev);
            }
        }
    };

    // ============================================================
    // MIDI Input handling
    // ============================================================
    void openMidiInput(const juce::String& deviceId)
    {
        closeMidiInput();
        midiInput = juce::MidiInput::openDevice(deviceId, this);
        if (midiInput)
        {
            midiInput->start();
            content->appendMidiIn("--- MIDI Input opened: " + midiInput->getName() + " ---");
        }
    }

    void closeMidiInput()
    {
        if (midiInput)
        {
            midiInput->stop();
            midiInput.reset();
        }
    }

    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg) override
    {
        static int messageCounter = 0;
        messageCounter++;

        // Only log 1 out of 4 CC messages to prevent flood, but log all Note messages
        bool shouldLog = msg.isNoteOn() || msg.isNoteOff() || (messageCounter % 4 == 0);

        if (shouldLog)
        {
            juce::String hex;
            for (int i = 0; i < msg.getRawDataSize(); ++i)
                hex += juce::String::toHexString(msg.getRawData()[i]).toUpperCase().paddedLeft('0', 2) + " ";

            // Use a SafePointer to avoid crashing if the window is closed
            juce::Component::SafePointer<SandboxContent> safeContent (content.get());

            juce::MessageManager::callAsync([safeContent, hex, msg]()
            {
                if (safeContent != nullptr)
                    safeContent->appendMidiIn("MIDI: " + hex.trim() + "  (" + msg.getDescription() + ")");
            });
        }

        // Convert raw MIDI to a ControllerInputEvent and push to the EventBus
        ControllerInputEvent ev = rawMidiToEvent(msg);
        if (ev.type != ControllerEventType::Unknown)
        {
            router.pushEvent(ev);
            juce::String evStr = controllerEventToString(ev);
            
            juce::Component::SafePointer<SandboxContent> safeContent (content.get());
            juce::MessageManager::callAsync([safeContent, evStr]()
            {
                if (safeContent != nullptr)
                    safeContent->appendEventBus("PUSH -> " + evStr);
            });
        }
    }

    // ============================================================
    // Inject a synthetic event (for UI test buttons)
    // ============================================================
    void injectEvent(const ControllerInputEvent& ev)
    {
        router.pushEvent(ev);
        content->appendEventBus("INJECT -> " + controllerEventToString(ev));
    }

    // ============================================================
    // Timer: drain the EventBus and feed MockAudioEngine
    // ============================================================
    void timerCallback() override
    {
        ControllerInputEvent ev;
        int count = 0;
        const int maxLogsPerTick = 10;

        while (router.popEvent(ev))
        {
            if (count < maxLogsPerTick)
                content->appendEventBus("POP   -> " + controllerEventToString(ev));
            else if (count == maxLogsPerTick)
                content->appendEventBus("... (throttled extra events this tick)");

            mockEngine.processEvent(ev);
            count++;
        }
    }

private:
    std::unique_ptr<SandboxContent> content;
    std::unique_ptr<juce::MidiInput> midiInput;
    ControllerRouter router;
    MockAudioEngine mockEngine;
    MixxxMappingParser mappingParser;

    void chooseAndLoadMapping()
    {
        chooser = std::make_unique<juce::FileChooser>("Select Mixxx MIDI XML Mapping...",
                                                     juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                     "*.midi.xml");
        
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        
        chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                content->appendMidiIn("--- LOADING: " + file.getFileName() + " ---");
                
                // Parse in background to avoid hanging the UI thread
                std::thread([this, file]() {
                    if (mappingParser.loadXml(file))
                    {
                        auto count = mappingParser.getMappingCount();
                        juce::MessageManager::callAsync([this, count]() {
                            content->appendMidiIn("--- SUCCESS: Loaded " + juce::String(count) + " mappings ---");
                        });
                    }
                    else
                    {
                        juce::MessageManager::callAsync([this]() {
                            content->appendMidiIn("--- ERROR: Failed to parse XML! ---");
                        });
                    }
                }).detach();
            }
        });
    }

    std::unique_ptr<juce::FileChooser> chooser;
    std::map<int, std::map<int, int>> msbState; 
    juce::uint32 lastJsLogTime = 0; // For throttling script logs

    // ------------------------------------------------------------------
    // Raw MIDI → ControllerInputEvent (generic mapping for sandbox)
    // In production this will be done by the Mixxx JS Engine
    // ------------------------------------------------------------------
    ControllerInputEvent rawMidiToEvent(const juce::MidiMessage& msg)
    {
        ControllerInputEvent ev;
        int status = msg.getRawData()[0];
        int midino = msg.getRawData()[1];
        int val7bit = msg.getRawData()[2];

        // 1. Try to find a mapping in the Mixxx XML
        MixxxControl ctrl = mappingParser.getControl(status, midino);
        
        if (ctrl.key.isNotEmpty())
        {
            // Parse deck index from group like "[Channel1]"
            int chNum = 1;
            if (ctrl.group.startsWithIgnoreCase("[Channel"))
                chNum = ctrl.group.substring(8, ctrl.group.length() - 1).getIntValue();
            
            ev.deckIndex = juce::jlimit(0, 3, chNum - 1);

            // Handle 14-bit High Resolution MIDI
            if (ctrl.is14BitMSB)
            {
                msbState[status][midino] = val7bit;
                return ev; // Wait for LSB
            }
            else if (ctrl.is14BitLSB)
            {
                int msbMidino = midino - 0x20;
                if (msbMidino >= 0 && msbState.count(status) && msbState[status].count(msbMidino))
                {
                    int msb = msbState[status][msbMidino];
                    float val14bit = ((msb << 7) | val7bit) / 16383.0f;
                    ev.value = val14bit;
                }
                else {
                    ev.value = val7bit / 127.0f; // Fallback to 7-bit
                }
            }
            else
            {
                ev.value = val7bit / 127.0f;
            }

            // Map Mixxx keys to our internal EventTypes
            if (ctrl.key == "play")            ev.type = ControllerEventType::Play;
            else if (ctrl.key == "cue_default") ev.type = ControllerEventType::Cue;
            else if (ctrl.key == "volume")      ev.type = ControllerEventType::Volume;
            else if (ctrl.key == "pregain")     ev.type = ControllerEventType::Gain;
            else if (ctrl.key == "rate")        ev.type = ControllerEventType::Pitch;
            else if (ctrl.key.containsIgnoreCase("filter") || ctrl.key == "super1") ev.type = ControllerEventType::Filter;
            else if (ctrl.key.containsIgnoreCase("parameter1")) ev.type = ControllerEventType::EQLow;
            else if (ctrl.key.containsIgnoreCase("parameter2")) ev.type = ControllerEventType::EQMid;
            else if (ctrl.key.containsIgnoreCase("parameter3")) ev.type = ControllerEventType::EQHigh;
            else if (ctrl.key == "crossfader")  ev.type = ControllerEventType::Crossfader;
            else if (ctrl.key == "sync_enabled") ev.type = ControllerEventType::Sync;
            else if (ctrl.key == "loop_in")     ev.type = ControllerEventType::LoopIn;
            else if (ctrl.key == "loop_out")    ev.type = ControllerEventType::LoopOut;
            else if (ctrl.key == "reloop_toggle") ev.type = ControllerEventType::Reloop;
            else if (ctrl.key == "slip_enabled") ev.type = ControllerEventType::Slip;
            else if (ctrl.isScriptBinding)
            {
                // Auto-detect jog wheel scripts to show them in the sandbox logs
                if (ctrl.key.containsIgnoreCase("jog"))
                {
                    ev.type = ControllerEventType::JogScratch;
                    ev.value = (val7bit - 64.0f) / 10.0f; // Standard relative relative encoding
                }
                else
                {
                    ev.type = ControllerEventType::Unknown;
                    
                    // Throttle script logs to avoid hanging the UI thread
                    auto now = juce::Time::getMillisecondCounter();
                    if (now - lastJsLogTime > 100)
                    {
                        lastJsLogTime = now;
                        juce::String key = ctrl.key;
                        juce::Component::SafePointer<SandboxContent> safeContent (content.get());
                        juce::MessageManager::callAsync([safeContent, key]() {
                            if (safeContent != nullptr) safeContent->appendMidiIn("JS BINDING: " + key);
                        });
                    }
                }
            }

            return ev;
        }

        // 2. Fallback to hardcoded basic mapping if no XML is loaded
        int ch = msg.getChannel();
        if (ch >= 1 && ch <= 4) ev.deckIndex = ch - 1;
        else ev.deckIndex = (ch - 1) % 4;

        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            if (note == 11) { ev.type = ControllerEventType::Play;   ev.value = 1.0f; }
            else if (note == 12) { ev.type = ControllerEventType::Cue; ev.value = 1.0f; }
            else if (note == 13) { ev.type = ControllerEventType::Sync; ev.value = 1.0f; }
        }
        else if (msg.isNoteOff())
        {
            int note = msg.getNoteNumber();
            if (note == 11) { ev.type = ControllerEventType::Play;   ev.value = 0.0f; }
        }
        else if (msg.isController())
        {
            int cc = msg.getControllerNumber();
            float rawVal = (float)msg.getControllerValue();
            float val = rawVal / 127.0f;

            if (cc == 19)       { ev.type = ControllerEventType::Volume;     ev.value = val; }
            else if (cc == 31)  
            { 
                // Discriminator: CC 31 is Crossfader on Ch 7, but Jog on Decks 1-4
                if (msg.getChannel() == 7)
                    ev.type = ControllerEventType::Crossfader;
                else
                    ev.type = ControllerEventType::JogScratch;

                ev.value = (msg.getChannel() == 7) ? val : (rawVal - 64.0f) / 10.0f;
            }
            else if (cc == 23)  { ev.type = ControllerEventType::EQLow;      ev.value = val; }
            else if (cc == 24)  { ev.type = ControllerEventType::EQMid;      ev.value = val; }
            else if (cc == 25)  { ev.type = ControllerEventType::EQHigh;     ev.value = val; }
            else if (cc == 26)  { ev.type = ControllerEventType::Filter;     ev.value = val; }
            else if (cc == 0)   { ev.type = ControllerEventType::Pitch;      ev.value = val; }
            
            else if (cc == 33 || cc == 34) 
            {
                ev.type = (cc == 33) ? ControllerEventType::JogScratch : ControllerEventType::JogBend;
                ev.value = (rawVal - 64.0f) / 10.0f; 
            }
        }

        return ev;
    }

    static juce::String controllerEventToString(const ControllerInputEvent& ev)
    {
        juce::String typeName;
        switch (ev.type)
        {
            case ControllerEventType::Play:       typeName = "PLAY"; break;
            case ControllerEventType::Cue:        typeName = "CUE"; break;
            case ControllerEventType::Sync:       typeName = "SYNC"; break;
            case ControllerEventType::Volume:     typeName = "VOLUME"; break;
            case ControllerEventType::Crossfader: typeName = "CROSSFADER"; break;
            case ControllerEventType::EQHigh:     typeName = "EQ_HIGH"; break;
            case ControllerEventType::EQMid:      typeName = "EQ_MID"; break;
            case ControllerEventType::EQLow:      typeName = "EQ_LOW"; break;
            case ControllerEventType::Filter:     typeName = "FILTER"; break;
            case ControllerEventType::Pitch:      typeName = "PITCH"; break;
            case ControllerEventType::Gain:       typeName = "GAIN/TRIM"; break;
            case ControllerEventType::LoopIn:     typeName = "LOOP_IN"; break;
            case ControllerEventType::LoopOut:    typeName = "LOOP_OUT"; break;
            case ControllerEventType::Reloop:     typeName = "RELOOP"; break;
            case ControllerEventType::Slip:       typeName = "SLIP"; break;
            case ControllerEventType::JogScratch: typeName = "JOG_SCRATCH"; break;
            case ControllerEventType::JogBend:    typeName = "JOG_BEND"; break;
            case ControllerEventType::HotCue:     typeName = "HOT_CUE"; break;
            default:                              typeName = "UNKNOWN"; break;
        }
        
        juce::String deckChar = juce::String::charToString((char)('A' + ev.deckIndex));
        return "{ type=" + typeName + " deck=" + deckChar + " (idx=" + juce::String(ev.deckIndex) + ") val=" + juce::String(ev.value, 3) + " }";
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControllerSandboxWindow)
};
