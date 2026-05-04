#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <onnxruntime_cxx_api.h>
#include "PluginScannerManager.h"
#include "VocalVstChain.h"

class TrackDatabase;

class AudioCore
{
public:
    AudioCore();
    ~AudioCore();

    void setDatabase (TrackDatabase* db) { trackDb = db; }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();

    // Controles do Deck A (Original Main Track)
    bool loadDeckA (const juce::File& file, double bpm = 0.0);
    void playDeckA();
    void stopDeckA();
    void seekDeckA (double positionSeconds);
    void ejectDeckA();
    
    // Controles do Deck B (New)
    bool loadDeckB (const juce::File& file, double bpm = 0.0);
    void playDeckB();
    void stopDeckB();
    void seekDeckB (double positionSeconds);
    void ejectDeckB();

    // Controles do Deck HandsFree (Automation)
    bool loadHandsFreeDeck (const juce::File& file, double bpm = 0.0);
    void playHandsFreeDeck();
    void stopHandsFreeDeck();
    void seekHandsFreeDeck (double positionSeconds);
    void ejectHandsFreeDeck();
    
    // Legacy support (redirects to HandsFree Deck)
    bool loadMainTrack (const juce::File& file, double bpm = 0.0) { return loadHandsFreeDeck(file, bpm); }
    void playMainTrack() { playHandsFreeDeck(); }
    void stopMainTrack() { stopHandsFreeDeck(); }
    void seekMainTrack (double pos) { seekHandsFreeDeck(pos); }
    void ejectMainTrack() { ejectHandsFreeDeck(); }
    void setMainTrackLoopRange (double startTime, double duration);
    void setMainTrackLoopEnabled (bool enabled);
    bool isMainTrackLoopEnabled() const;
    
    // Generic Deck Loop Controls
    void setDeckLoopRange (int deckIdx, double startTime, double duration);
    void setDeckLoopEnabled (int deckIdx, bool enabled);
    bool isDeckLoopEnabled (int deckIdx) const;
    double getDeckLoopStart (int deckIdx) const;
    double getDeckLoopLength (int deckIdx) const;
    double getMainTrackBpm() const { return mainTrackBpm * (1.0 + (deckHPitch * 0.06)); }
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
    bool loadAudioFile (const juce::File& file, int padIndex, bool shouldLoop);
    
    // Recording sampler from MIC input
    void startPadRecording (int padIndex);
    void stopPadRecording (int padIndex, std::function<void(juce::File)> onFinished);

    // Master Output Recording
    void startMasterRecording();
    void stopMasterRecording (std::function<void(juce::File)> onFinished);

    // FX Controls (6 slots: 0=Delay, 1=Echo, 2=Reverb, 3=Flange, 4=Space, 5=DubEcho)
    void setFxEnabled (int deckIdx, int fxIndex, bool enabled);
    void setFxAmount (int deckIdx, int fxIndex, float amount);
    bool isFxEnabled (int deckIdx, int fxIndex) const;
    float getFxAmount (int deckIdx, int fxIndex) const;

    // Stems Controls (frequency-band mute/unmute)
    void setStemMuted (int stemIndex, bool muted); // 0=vocal, 1=drums, 2=bass
    bool isStemMuted (int stemIndex) const;

    void setGlobalPitchRatio (double ratio);
    void setDeckPitch (int deckIdx, double pitch);
    double getDeckPitch (int deckIdx) const;
    
    // Volume Controls (0.0 to 1.0)
    void setMasterVolume (float vol);
    void setTrackVolume (float vol); // Deprecated but kept for compatibility
    enum class XyMode { Ladder, UltraMulti };
    void setXyFilter (int deckIdx, float x, float y);
    void setXyFilterEnabled (int deckIdx, bool enabled);
    void setXyMode (int deckIdx, XyMode mode);
    
    // Mic Controls
    void setMicEnabled (bool enabled) { micEnabled.store(enabled); }
    bool isMicEnabled() const { return micEnabled.load(); }
    void setMicVolume (float vol) { micVolume.store(vol); }
    float getMicVolume() const { return micVolume.load(); }
    
    float getMasterVolume() const { return masterVolume; }
    float getTrackVolume() const { return trackVolume; }
    void setCrossfaderPosition (float pos);
    
    // Per-Deck Gain/EQ
    void setDeckGain (int deckIdx, float gain);
    void setDeckVolume (int deckIdx, float vol);
    void setDeckEQ (int deckIdx, int band, float level); // 0=low, 1=mid, 2=high
    void setDeckFilter (int deckIdx, float value); // -1.0 (LP) to 1.0 (HP)
    
    float getDeckGain (int deckIdx) const;
    float getDeckVolume (int deckIdx) const;
    float getDeckEQ (int deckIdx, int band) const;
    
    // Peak level for VU meter (0.0 to 1.0+)
    float getCurrentPeakLevel() const { return currentPeakLevel.load(); }
    float getDeckPeakLevel(int deckIdx) const { 
        if (deckIdx == 0) return deckAPeak.load();
        if (deckIdx == 1) return deckBPeak.load();
        return deckHPeak.load();
    }
    float getInputLevel() const { return lastInputLevel.load(); }
    
    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    juce::AudioThumbnail& getThumbnail (int deckIdx = 0) { 
        if (deckIdx == 0) return thumbnail;
        if (deckIdx == 1) return thumbnailB;
        return thumbnailH;
    }
    
    juce::String getDeckName (int deckIdx) const { 
        if (deckIdx == 0) return deckAName;
        if (deckIdx == 1) return deckBName;
        return deckHName;
    }
    double getDeckBpm (int deckIdx) const { 
        double base = 120.0;
        double p = 0.0;
        if (deckIdx == 0) { base = deckABpm; p = deckAPitch; }
        else if (deckIdx == 1) { base = deckBBpm; p = deckBPitch; }
        else { base = deckHBpm; p = deckHPitch; }
        return base * (1.0 + (p * 0.06)); 
    }
    double getDeckPosition (int deckIdx) const;
    double getDeckLength (int deckIdx) const;
    bool isDeckPlaying (int deckIdx) const;
    
    int getMasterDeck() const { return masterDeckIdx; }
    void setMasterDeck(int idx) { masterDeckIdx = idx; }
    
    bool isSyncEnabled(int deckIdx) const { return syncEnabled[deckIdx]; }
    void setSyncEnabled(int deckIdx, bool enabled) { syncEnabled[deckIdx] = enabled; }
    
    // CUE Point Management
    void setDeckCuePoint (int deckIdx, double pos) {
        if (deckIdx == 0) deckACue = pos;
        else if (deckIdx == 1) deckBCue = pos;
        else deckHCue = pos;
    }
    double getDeckCuePoint (int deckIdx) const {
        if (deckIdx == 0) return deckACue;
        if (deckIdx == 1) return deckBCue;
        return deckHCue;
    }
    void triggerDeckCue (int deckIdx);
    
    VocalVstChain& getVocalVstChain() { return vocalVstChain; }
    PluginScannerManager& getVstManager() { return vstManager; }
    
    // New specific getters
    double getHandsFreePosition() const { return getDeckPosition(2); }
    double getHandsFreeLength() const { return getDeckLength(2); }
    bool isHandsFreePlaying() const { return isDeckPlaying(2); }

private:
    juce::AudioFormatManager formatManager;
    juce::MixerAudioSource mixer;
    
    double mainTrackBpm = 120.0;
    double deckABpm = 120.0, deckBBpm = 120.0, deckHBpm = 120.0;
    double deckACue = 0.0, deckBCue = 0.0, deckHCue = 0.0;
    juce::String deckAName, deckBName, deckHName;
    int masterDeckIdx = -1;
    bool syncEnabled[2] = { false, false };
    TrackDatabase* trackDb = nullptr;
    
    void handleSyncLogic();
    
    // Waveform Rendering Support
    juce::AudioThumbnailCache thumbnailCache { 10 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };
    juce::AudioThumbnail thumbnailB { 512, formatManager, thumbnailCache };
    juce::AudioThumbnail thumbnailH { 512, formatManager, thumbnailCache };

    double deckAPitch = 0.0, deckBPitch = 0.0, deckHPitch = 0.0; // -1.0 to 1.0 (maps to -6% to +6%)
    float masterVolume = 1.0f;
    float trackVolume = 1.0f;
    float crossfaderPos = 0.5f;

    struct DeckMixerState {
        float gain = 1.0f;
        float eqLow = 0.0f; // -24dB to +6dB?
        float eqMid = 0.0f;
        float eqHigh = 0.0f;
        float volume = 1.0f;
        float filter = 0.0f; // -1.0 to 1.0
        
        // Internal EQ Filters (Basic 3-band)
        std::unique_ptr<juce::dsp::IIR::Filter<float>> lowFilter[2];
        std::unique_ptr<juce::dsp::IIR::Filter<float>> midFilter[2];
        std::unique_ptr<juce::dsp::IIR::Filter<float>> highFilter[2];

        // Per-Deck FX State
        struct EffectSlot {
            bool enabled = false;
            float amount = 0.5f;
        };
        std::vector<EffectSlot> fxSlots;

        // Per-Deck XY State
        XyMode xyMode = XyMode::UltraMulti;
        float curX = 0.5f, curY = 0.5f;
        bool xyEnabled = false;
        
        // Internal DSP for this deck's XY
        juce::dsp::LadderFilter<float> lpFilter;
        juce::dsp::LadderFilter<float> hpFilter;
        juce::dsp::Chorus<float> flanger;
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> echo { 44100 };
        
        float flangerMix = 0.0f, echoMix = 0.0f, lpMix = 0.0f, hpMix = 0.0f;

        // Internal DSP for 6-Slot FX
        juce::AudioBuffer<float> fxDelayBuffer;
        int fxWritePos = 0;
        float fxLfoPhase = 0.0f;

        DeckMixerState() {
            fxSlots.resize(6);
            fxDelayBuffer.setSize(2, 44100 * 2); // 2 seconds delay line
            fxDelayBuffer.clear();
        }
        
        void prepare(double sr) {
            juce::dsp::ProcessSpec spec { sr, 512, 2 };
            lpFilter.prepare(spec);
            hpFilter.prepare(spec);
            flanger.prepare(spec);
            echo.prepare(spec);
            
            fxDelayBuffer.setSize(2, (int)(sr * 2.0));
            fxDelayBuffer.clear();
        }
    };
    
    DeckMixerState deckAState, deckBState, deckHState;
    std::atomic<float> deckAPeak { 0.0f }, deckBPeak { 0.0f }, deckHPeak { 0.0f };

    void processDeckDsp (juce::AudioBuffer<float>& buffer, DeckMixerState& state, int numSamples);

    std::atomic<float> currentPeakLevel { 0.0f };
    std::atomic<float> lastInputLevel { 0.0f };
    std::atomic<bool> micEnabled { false };
    std::atomic<float> micVolume { 1.0f };
    PluginScannerManager vstManager;
    VocalVstChain vocalVstChain { vstManager.getFormatManager() };

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

    // Real DSP Effect Processors (Global/HandFree - kept for compatibility)
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

    // Stems State (3 bands: 0=vocal, 1=drums, 2=bass)
    static constexpr int NUM_STEMS = 3;
    bool stemMuted[NUM_STEMS] = {};
    juce::LinearSmoothedValue<float> stemGains[NUM_STEMS];

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
        juce::int64 getLoopStart() const { return loopStart; }
        juce::int64 getLoopLength() const { return loopLength; }

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
        
        // Adicionando mixer local para este canal para processar EQ antes do mixer master
        juce::AudioBuffer<float> processingBuffer;
    };

    std::unique_ptr<PlaybackChannel> deckAChannel;
    std::unique_ptr<PlaybackChannel> deckBChannel;
    std::unique_ptr<PlaybackChannel> handsFreeChannel;
    std::vector<std::unique_ptr<PlaybackChannel>> padChannels;
    
    juce::AudioBuffer<float> micInputBuffer;

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

    void updateEQFilters(int deckIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioCore)
};
