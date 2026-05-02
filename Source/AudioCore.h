#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <onnxruntime_cxx_api.h>

class AudioCore
{
public:
    AudioCore();
    ~AudioCore();

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();

    // Carrega um arquivo de áudio de forma thread-safe num canal interno do mixer
    bool loadAudioFile (const juce::File& file, int padIndex, bool shouldLoop = false);
    
    // Controles do Deck Principal
    bool loadMainTrack (const juce::File& file, double bpm = 0.0);
    void playMainTrack();
    void stopMainTrack();
    void seekMainTrack (double positionSeconds);
    void ejectMainTrack();
    void setMainTrackLoopRange (double startTime, double duration);
    void setMainTrackLoopEnabled (bool enabled);
    bool isMainTrackLoopEnabled() const;
    double getMainTrackBpm() const { return mainTrackBpm * (1.0 + (pitchValue * 0.06)); }
    double getMainTrackPosition() const;
    double getMainTrackLength() const;
    bool isMainTrackPlaying() const;

    // Controles de Playback dos Pads
    void playPad (int padIndex);
    void stopPad (int padIndex);
    void ejectPad (int padIndex);
    bool isPadPlaying (int padIndex) const;
    void setPadLoop (int padIndex, bool shouldLoop);
    void setPadVolume (int padIndex, float gain);
    
    // Recording sampler from MIC input
    void startPadRecording (int padIndex);
    void stopPadRecording (int padIndex, std::function<void(juce::File)> onFinished);

    // Master Output Recording
    void startMasterRecording();
    void stopMasterRecording (std::function<void(juce::File)> onFinished);

    // FX Controls (6 slots: 0=Delay, 1=Echo, 2=Reverb, 3=Flange, 4=Space, 5=DubEcho)
    void setFxEnabled (int fxIndex, bool enabled);
    void setFxAmount (int fxIndex, float amount);
    bool isFxEnabled (int fxIndex) const;

    // Stems Controls (frequency-band mute/unmute)
    void setStemMuted (int stemIndex, bool muted); // 0=vocal, 1=drums, 2=bass
    bool isStemMuted (int stemIndex) const;

    void setGlobalPitchRatio (double ratio);
    
    // Volume Controls (0.0 to 1.0)
    void setMasterVolume (float vol);
    void setTrackVolume (float vol);
    enum class XyMode { Ladder, UltraMulti };
    void setXyMode (XyMode mode);
    void setXyFilter (float x, float y);
    void setXyFilterEnabled (bool enabled);
    
    float getMasterVolume() const { return masterVolume; }
    float getTrackVolume() const { return trackVolume; }
    
    // Peak level for VU meter (0.0 to 1.0+)
    float getCurrentPeakLevel() const { return currentPeakLevel.load(); }
    float getInputLevel() const { return lastInputLevel.load(); }
    
    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    juce::AudioThumbnail& getThumbnail() { return thumbnail; }

private:
    juce::AudioFormatManager formatManager;
    juce::MixerAudioSource mixer;
    
    // Waveform Rendering Support
    juce::AudioThumbnailCache thumbnailCache { 5 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };

    double mainTrackBpm = 120.0;
    double pitchValue = 0.0; // -1.0 to 1.0 (maps to -8% to +8%)
    float masterVolume = 1.0f;
    float trackVolume = 1.0f;
    std::atomic<float> currentPeakLevel { 0.0f };
    std::atomic<float> lastInputLevel { 0.0f };

    // ONNX Runtime for Demucs
    static constexpr int NUM_FX = 6;
    std::unique_ptr<Ort::Env> ortEnv;
    std::unique_ptr<Ort::Session> ortSession;
    bool onnxInitialized = false;
    
    void asyncExtractStems (const juce::File& file);

    // Stem Audio Storage (Memory persistent)
    juce::AudioBuffer<float> vocalBuffer;
    juce::AudioBuffer<float> drumsBuffer;
    juce::AudioBuffer<float> bassBuffer;
    std::atomic<bool> stemsAreReady { false };
    std::atomic<float> stemProgress { 0.0f };

    // Real DSP Effect Processors
    struct EffectSlot {
        bool enabled = false;
        float amount = 0.0f;
    };
    std::vector<EffectSlot> fxProcessors;
    
    // FX Internal State
    juce::AudioBuffer<float> delayBuffer;
    int writePos = 0;
    float reverbState[2] = { 0.0f, 0.0f };
    float lfoPhase = 0.0f;
    double currentSampleRate = 44100.0;

    // XY Pad FX (Mola Logic)
    XyMode xyMode = XyMode::UltraMulti;
    juce::dsp::LadderFilter<float> xyLpFilter;
    juce::dsp::LadderFilter<float> xyHpFilter;
    juce::dsp::Chorus<float> xyFlanger;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> xyEcho;
    
    float xyFlangerMix = 0.0f;
    float xyEchoMix = 0.0f;
    float xyLpMix = 0.0f;
    float xyHpMix = 0.0f;
    
    std::atomic<bool> xyEnabled { false };

    // Stems State (3 bands: 0=vocal, 1=drums, 2=bass)
    static constexpr int NUM_STEMS = 3;
    bool stemMuted[NUM_STEMS] = {};
    juce::LinearSmoothedValue<float> stemGains[NUM_STEMS];
    float lpState[2] = { 0.0f, 0.0f };
    float hpState[2] = { 0.0f, 0.0f };

    // CrossfadingLoopSource MUST be defined before PlaybackChannel since
    // PlaybackChannel holds a unique_ptr to it.
    class CrossfadingLoopSource : public juce::PositionableAudioSource
    {
    public:
        CrossfadingLoopSource(juce::AudioFormatReader* reader, bool deleteReaderWhenDone);
        ~CrossfadingLoopSource() override;

        void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
        void releaseResources() override;
        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
        void setNextReadPosition(juce::int64 newPosition) override;
        juce::int64 getNextReadPosition() const override;
        juce::int64 getTotalLength() const override;
        bool isLooping() const override;
        void setLooping(bool shouldLoop) override;
        void setLoopRange(juce::int64 start, juce::int64 length);

    private:
        std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
        double currentSampleRate = 44100.0;
        bool looping = false;
        juce::int64 loopStart = 0;
        juce::int64 loopLength = 0;
        int crossfadeSamples = 220; // 5ms in samples
    };

    // PlaybackChannel: now defined AFTER CrossfadingLoopSource
    struct PlaybackChannel {
        std::unique_ptr<CrossfadingLoopSource> readerSource;
        std::unique_ptr<juce::ResamplingAudioSource> resampler;
        std::unique_ptr<juce::AudioTransportSource> transport;
    };

    std::unique_ptr<PlaybackChannel> mainTrackChannel;
    std::vector<std::unique_ptr<PlaybackChannel>> padChannels;

    // Helper to detect BPM (Basic peak detection)
    double detectBpm (const juce::File& file);

    struct RecorderState {
        juce::CriticalSection lock;
        std::unique_ptr<juce::AudioFormatWriter> writer;
        juce::File file;
        std::atomic<bool> isRecording { false };
    };
    
    RecorderState padRecorder;
    RecorderState masterRecorder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioCore)
};
