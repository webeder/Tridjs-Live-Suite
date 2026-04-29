#pragma once

#include <JuceHeader.h>
#include <functional>
#include <algorithm>

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

    std::function<void(double, double)> onLoopSet; // start, duration
    std::function<void(bool)> onLoopEnabled;

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
        void resized() override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        
        void generateRgbWaveform (const juce::File& file);
        void setZoomIn();
        void setZoomOut();
        void setZoomLevel (double newZoom);
        
        void setLoopVisual (double start, double duration, bool active);
        
        juce::String loadedTrackName;
        bool isDraggingOver = false;
        bool isPlaying = false;
        double trackLength = 0.0;
        double cuePoint = 0.0;
        double currentPos = 0.0;
        double zoomFactor = 1.0; 
        
        bool loopActive = false;
        double loopStart = 0.0;
        double loopDuration = 0.0;
        
        // Novos labels para sobrepor o grid do waveform
        juce::Label bpmOverlay { {}, "120.00" };
        juce::Label timeOverlay { {}, "00:00.00" };
        juce::Label deckLabel { {}, "DECK_01" };
        juce::Label trackNameLabel { {}, "NO TRACK" };
        void updateOverlays(double bpm, double time, bool playing);

        std::function<void(double)> onSeekToPosition;

    private:
        struct SpectralPoint { float low, mid, high; };
        std::vector<SpectralPoint> spectralData;
        std::vector<SpectralPoint> nextSpectralData; // For background loading
        std::atomic<bool> isAnalyzing { false };
        
        juce::AudioThumbnail& thumbnail;
        juce::Image waveformImage;
        juce::AudioFormatManager formatManager;
        
        juce::TextButton zoomInBtn { "+" };
        juce::TextButton zoomOutBtn { "-" };
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

    // Loop
    juce::TextButton autoLoopBtn { "AUTO LOOP" };
    juce::TextButton loopInBtn { "IN" }, loopOutBtn { "OUT" };
    juce::Label loopLabel { {}, "LOOP" }, loopSizeLabel { {}, "4 BEATS" };
    int currentLoopBeats = 4;
    double currentBpm = 120.0;
    void applyCurrentLoop();

    // Knobs
    juce::Slider masterKnob;
    juce::Label masterLabel { {}, "MASTER" };
    juce::Label masterValue { {}, "100" };
    juce::Slider volumeKnob;
    juce::Label volumeLabel { {}, "VOL TRACK" };
    juce::Label volumeValue { {}, "100" };

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
