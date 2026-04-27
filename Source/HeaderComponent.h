#pragma once

#include <JuceHeader.h>
#include <functional>

class HeaderComponent : public juce::Component,
                        public juce::DragAndDropTarget,
                        public juce::FileDragAndDropTarget,
                        public juce::Timer
{
public:
    HeaderComponent (juce::AudioThumbnail& thumb);
    ~HeaderComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // Callbacks to MainComponent
    std::function<void(bool)> onRecordToggled;
    std::function<void(const juce::File&)> onFileDropped;
    std::function<void()> onPlay;
    std::function<void()> onStop;
    std::function<void()> onEject;
    std::function<void(double)> onSeek;
    std::function<void(float)> onMasterVolumeChanged;
    std::function<void(float)> onTrackVolumeChanged;
    std::function<void(float)> onPitchChanged;
    
    // Transport info from MainComponent
    void updateTransportInfo (double positionSeconds, double trackLengthSeconds, bool playing);
    void updatePeakLevel (float peak);
    void updateMasterVolumeFromExtern (float v);
    void updateTrackVolumeFromExtern (float v);
    void updateBpmDisplay (double bpm);

    // DragAndDrop overrides
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;

    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit(const juce::DragAndDropTarget::SourceDetails& details) override;

private:
    // Waveform with click-to-seek
    class WaveformDisplay : public juce::Component
    {
    public:
        WaveformDisplay (juce::AudioThumbnail& thumb);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;

        juce::String loadedTrackName;
        bool isDraggingOver = false;
        bool isPlaying = false;
        double trackLength = 0.0;
        double cuePoint = 0.0;
        double currentPos = 0.0;
        std::function<void(double)> onSeekToPosition;

    private:
        juce::AudioThumbnail& thumbnail;
    };
    WaveformDisplay waveformDisplay;

    // VU Meter (painted manually)
    class VuMeter : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override;
        void setLevel (float newLevel) { level = newLevel; repaint(); }
    private:
        float level = 0.0f;
    };
    VuMeter vuMeter;

    // Transport Buttons
    juce::TextButton playBtn { "PLAY" };
    juce::TextButton stopBtn { "STOP" };
    juce::TextButton cueBtn  { "CUE" };

    // Knobs
    juce::Slider masterKnob;
    juce::Label masterLabel { {}, "MASTER" };
    juce::Label masterValue { {}, "100" };
    juce::Slider volumeKnob;
    juce::Label volumeLabel { {}, "VOL TRACK" };
    juce::Label volumeValue { {}, "100" };

    juce::Slider pitchSlider;
    juce::Label pitchLabel { {}, "PITCH" };
    juce::Label pitchValue { {}, "1.00x" };
    juce::Label bpmDisplay { {}, "BPM: ---" };

    // Indicators
    juce::Label lcdClock { {}, "00:00:00" };
    juce::TextButton quantToggle { "QUANT" };
    juce::TextButton gearBtn { "SET" };

    // Record
    juce::TextButton recordButton { "REC" };
    juce::TextButton ejectButton { "EJT" };
    juce::Label recordDuration { {}, "00:00" };
    bool isRecording = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderComponent)
};
