#include "AudioCore.h"
#include "TrackDatabase.h"
#include <cmath>

AudioCore::AudioCore()
{
    // Initialize ONNX Runtime
    try {
        ortEnv = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "TridjsDemucs");
        onnxInitialized = true;
    } catch (...) {
        onnxInitialized = false;
    }

    formatManager.registerBasicFormats();

    for (int i = 0; i < NUM_FX; ++i)
    {
        EffectSlot slot;
        fxProcessors.push_back(std::move(slot));
    }

    // Deck A Setup
    deckAChannel = std::make_unique<PlaybackChannel>();
    deckAChannel->transport = std::make_unique<juce::AudioTransportSource>();
    deckAChannel->resampler = std::make_unique<juce::ResamplingAudioSource>(deckAChannel->transport.get(), false, 2);

    // Deck B Setup
    deckBChannel = std::make_unique<PlaybackChannel>();
    deckBChannel->transport = std::make_unique<juce::AudioTransportSource>();
    deckBChannel->resampler = std::make_unique<juce::ResamplingAudioSource>(deckBChannel->transport.get(), false, 2);

    // HandsFree Deck Setup
    handsFreeChannel = std::make_unique<PlaybackChannel>();
    handsFreeChannel->transport = std::make_unique<juce::AudioTransportSource>();
    handsFreeChannel->resampler = std::make_unique<juce::ResamplingAudioSource>(handsFreeChannel->transport.get(), false, 2);

    // Pads Setup
    // (Pads are now self-contained PadEngine structures initialized automatically)

    for (int i = 0; i < NUM_STEMS; ++i)
        stemGains[i].setCurrentAndTargetValue(1.0f);

    // Init EQ Filters
    for (int i = 0; i < 2; ++i) {
        deckAState.lowFilter[i] = std::make_unique<juce::dsp::IIR::Filter<float>>();
        deckAState.midFilter[i] = std::make_unique<juce::dsp::IIR::Filter<float>>();
        deckAState.highFilter[i] = std::make_unique<juce::dsp::IIR::Filter<float>>();
        deckBState.lowFilter[i] = std::make_unique<juce::dsp::IIR::Filter<float>>();
        deckBState.midFilter[i] = std::make_unique<juce::dsp::IIR::Filter<float>>();
        deckBState.highFilter[i] = std::make_unique<juce::dsp::IIR::Filter<float>>();
        deckHState.lowFilter[i] = std::make_unique<juce::dsp::IIR::Filter<float>>();
        deckHState.midFilter[i] = std::make_unique<juce::dsp::IIR::Filter<float>>();
        deckHState.highFilter[i] = std::make_unique<juce::dsp::IIR::Filter<float>>();
    }
}

AudioCore::~AudioCore()
{
    mixer.removeAllInputs();
}

void AudioCore::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    mixer.prepareToPlay (samplesPerBlockExpected, sampleRate);
    deckAChannel->resampler->prepareToPlay(samplesPerBlockExpected, sampleRate);
    deckBChannel->resampler->prepareToPlay(samplesPerBlockExpected, sampleRate);
    handsFreeChannel->resampler->prepareToPlay(samplesPerBlockExpected, sampleRate);
    
    updateEQFilters(0);
    updateEQFilters(1);
    updateEQFilters(2);
    
    deckAChannel->processingBuffer.setSize(2, samplesPerBlockExpected);
    deckBChannel->processingBuffer.setSize(2, samplesPerBlockExpected);
    handsFreeChannel->processingBuffer.setSize(2, samplesPerBlockExpected);

    delayBuffer.setSize(2, (int)sampleRate);
    delayBuffer.clear();
    writePos = 0;
    currentSampleRate = sampleRate;

    for (int i = 0; i < NUM_STEMS; ++i)
        stemGains[i].reset(sampleRate, 0.1);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlockExpected;
    spec.numChannels = 2;
    
    deckAState.prepare(sampleRate);
    deckBState.prepare(sampleRate);
    deckHState.prepare(sampleRate);
    vocalVstChain.prepare(sampleRate, samplesPerBlockExpected);
    
    micInputBuffer.setSize(2, samplesPerBlockExpected);
    micInputBuffer.clear();
}

void AudioCore::setCrossfaderPosition (float pos) { crossfaderPos = pos; }

void AudioCore::setDeckGain(int deckIdx, float gain) {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    state.gain = gain;
}

void AudioCore::setDeckVolume(int deckIdx, float vol) {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    state.volume = vol;
}

void AudioCore::setDeckEQ(int deckIdx, int band, float level) {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    if (band == 0) state.eqLow = level;
    else if (band == 1) state.eqMid = level;
    else if (band == 2) state.eqHigh = level;
    updateEQFilters(deckIdx);
}

float AudioCore::getDeckGain(int deckIdx) const {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    return state.gain;
}

float AudioCore::getDeckVolume(int deckIdx) const {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    return state.volume;
}

float AudioCore::getDeckEQ(int deckIdx, int band) const {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    if (band == 0) return state.eqLow;
    if (band == 1) return state.eqMid;
    return state.eqHigh;
}

void AudioCore::updateEQFilters(int deckIdx)
{
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    
    float lowGain = std::pow(10.0f, state.eqLow / 20.0f);
    float midGain = std::pow(10.0f, state.eqMid / 20.0f);
    float hiGain = std::pow(10.0f, state.eqHigh / 20.0f);
    
    auto lowCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate, 250.0f, 0.707f, lowGain);
    auto midCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, 1000.0f, 0.707f, midGain);
    auto hiCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate, 4000.0f, 0.707f, hiGain);
    
    for (int i = 0; i < 2; ++i) {
        *state.lowFilter[i]->coefficients = *lowCoeffs;
        *state.midFilter[i]->coefficients = *midCoeffs;
        *state.highFilter[i]->coefficients = *hiCoeffs;
    }

    if (state.filter < -0.01f) {
        state.lpFilter.setCutoffFrequencyHz(juce::jmap(std::abs(state.filter), 0.0f, 1.0f, 20000.0f, 20.0f));
        state.hpFilter.setCutoffFrequencyHz(20.0f);
    } else if (state.filter > 0.01f) {
        state.hpFilter.setCutoffFrequencyHz(juce::jmap(state.filter, 0.0f, 1.0f, 20.0f, 15000.0f));
        state.lpFilter.setCutoffFrequencyHz(20000.0f);
    } else {
        state.lpFilter.setCutoffFrequencyHz(20000.0f);
        state.hpFilter.setCutoffFrequencyHz(20.0f);
    }
}

void AudioCore::setDeckFilter(int deckIdx, float value) {
    if (deckIdx == 0) deckAState.filter = value;
    else if (deckIdx == 1) deckBState.filter = value;
    else deckHState.filter = value;
    updateEQFilters(deckIdx);
}

void AudioCore::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* mainBuffer = bufferToFill.buffer;
    int start = bufferToFill.startSample;
    int num = bufferToFill.numSamples;

    // 0. Capture Input (Microphone)
    float inLevel = mainBuffer->getMagnitude(start, num);
    lastInputLevel.store(inLevel);
    
    // Use pre-allocated buffer to avoid allocation on audio thread
    micInputBuffer.setSize(mainBuffer->getNumChannels(), num, false, false, true);
    for (int i = 0; i < mainBuffer->getNumChannels(); ++i) {
        micInputBuffer.copyFrom(i, 0, *mainBuffer, i, start, num);
    }

    // Process Mic through VST if active
    vocalVstChain.processMicAudio(micInputBuffer);

    // 1. Process Decks Manually
    deckAChannel->processingBuffer.setSize(2, num, false, false, true);
    deckAChannel->processingBuffer.clear();
    juce::AudioSourceChannelInfo deckAInfo(&deckAChannel->processingBuffer, 0, num);
    deckAChannel->resampler->getNextAudioBlock(deckAInfo);
    
    deckBChannel->processingBuffer.setSize(2, num, false, false, true);
    deckBChannel->processingBuffer.clear();
    juce::AudioSourceChannelInfo deckBInfo(&deckBChannel->processingBuffer, 0, num);
    deckBChannel->resampler->getNextAudioBlock(deckBInfo);

    handsFreeChannel->processingBuffer.setSize(2, num, false, false, true);
    handsFreeChannel->processingBuffer.clear();
    juce::AudioSourceChannelInfo deckHInfo(&handsFreeChannel->processingBuffer, 0, num);
    handsFreeChannel->resampler->getNextAudioBlock(deckHInfo);

    // [NEW] Process Pads (Memory Recording and Playback)
    for (int i = 0; i < 9; ++i) {
        auto& pad = pads[i];
        const juce::ScopedLock sl(pad.lock);
        
        // Handle Recording
        if (pad.isRecording) {
            int padSamples = pad.buffer.getNumSamples();
            int copyNum = std::min(num, padSamples - pad.writePosition);
            if (copyNum > 0) {
                // Mix mono mic input into both stereo channels of pad buffer
                pad.buffer.copyFrom(0, pad.writePosition, micInputBuffer.getReadPointer(0), copyNum);
                pad.buffer.copyFrom(1, pad.writePosition, micInputBuffer.getReadPointer(0), copyNum);
                pad.writePosition += copyNum;
            }
            if (pad.writePosition >= padSamples) {
                pad.isRecording = false; // Auto-stop at max capacity
                pad.hasAudio = true;
            }
        }
        
        // Handle Playback into handsFreeChannel->processingBuffer (so it gets FX!)
        if (pad.isPlaying && pad.hasAudio && pad.writePosition > 0) {
            int padLen = pad.writePosition;
            int samplesLeft = num;
            int destPos = 0;
            
            while (samplesLeft > 0) {
                int copyNum = std::min(samplesLeft, padLen - pad.playPosition);
                
                for (int ch = 0; ch < std::min(2, pad.buffer.getNumChannels()); ++ch) {
                    handsFreeChannel->processingBuffer.addFrom(ch, destPos, pad.buffer, ch, pad.playPosition, copyNum, pad.volume.load());
                }
                
                pad.playPosition += copyNum;
                destPos += copyNum;
                samplesLeft -= copyNum;
                
                if (pad.playPosition >= padLen) {
                    if (pad.isLooping) {
                        pad.playPosition = 0;
                    } else {
                        pad.isPlaying = false;
                        break;
                    }
                }
            }
        }
    }

    auto applyMixer = [&](int idx, juce::AudioBuffer<float>& buf) {
        auto& state = (idx == 0) ? deckAState : (idx == 1 ? deckBState : deckHState);
        
        buf.applyGain(state.gain);
        processDeckDsp(buf, state, num);
        buf.applyGain(state.volume);
        
        float p = buf.getMagnitude(0, num);
        if (idx == 0) deckAPeak.store(p); 
        else if (idx == 1) deckBPeak.store(p);
        else deckHPeak.store(p);
    };

    applyMixer(0, deckAChannel->processingBuffer);
    applyMixer(1, deckBChannel->processingBuffer);
    applyMixer(2, handsFreeChannel->processingBuffer);

    // 2. Mix Decks into Master Buffer with DJ Crossfader Curve
    bufferToFill.clearActiveBufferRegion();
    
    // "Full-Center" curve: both decks at 1.0 in the middle (0.5)
    // This prevents the volume dip at center point.
    float crossA = std::min(1.0f, 2.0f * (1.0f - crossfaderPos));
    float crossB = std::min(1.0f, 2.0f * crossfaderPos);
    
    mainBuffer->addFrom(0, start, deckAChannel->processingBuffer, 0, 0, num, crossA);
    mainBuffer->addFrom(1, start, deckAChannel->processingBuffer, 1, 0, num, crossA);
    mainBuffer->addFrom(0, start, deckBChannel->processingBuffer, 0, 0, num, crossB);
    mainBuffer->addFrom(1, start, deckBChannel->processingBuffer, 1, 0, num, crossB);
    
    // 2.1 Mix HandsFree Deck (Direct to Master, not affected by crossfader)
    mainBuffer->addFrom(0, start, handsFreeChannel->processingBuffer, 0, 0, num);
    mainBuffer->addFrom(1, start, handsFreeChannel->processingBuffer, 1, 0, num);

    // 3. (Removed old pad mixer - pads are now mixed into HandsFree)

    // 3.5 Mix Mic Input (Independent but part of master chain)
    if (micEnabled.load()) {
        float vol = micVolume.load();
        for (int i = 0; i < mainBuffer->getNumChannels(); ++i) {
            mainBuffer->addFrom(i, start, micInputBuffer, i, 0, num, vol);
        }
    }

    // 4. Master Volume
    mainBuffer->applyGain(start, num, masterVolume);

    if (padRecorder.isRecording.load()) {
        const juce::ScopedLock sl (padRecorder.lock);
        if (padRecorder.writer != nullptr) {
            juce::AudioBuffer<float> tempRecordBuffer(2, num);
            tempRecordBuffer.clear();
            int numIn = mainBuffer->getNumChannels();
            for (int ch = 0; ch < numIn; ++ch) {
                tempRecordBuffer.addFrom(ch % 2, 0, *mainBuffer, ch, start, num);
            }
            padRecorder.writer->writeFromAudioSampleBuffer(tempRecordBuffer, 0, num);
        }
    }

    bool anyStemActive = false;
    for (int s = 0; s < NUM_STEMS; ++s) {
        if (stemGains[s].getNextValue() < 0.99f || stemGains[s].isSmoothing()) 
            anyStemActive = true;
    }

    if (stemsAreReady.load() && anyStemActive && isMainTrackPlaying())
    {
        auto posSec = getMainTrackPosition();
        auto startSample = (juce::int64)(posSec * 44100.0);

        for (int ch = 0; ch < mainBuffer->getNumChannels(); ++ch) {
            auto* out = mainBuffer->getWritePointer(ch, start);
            for (int i = 0; i < num; ++i) {
                juce::int64 idx = startSample + i;
                if (idx >= 0 && idx < vocalBuffer.getNumSamples()) {
                    float v = vocalBuffer.getSample(ch % vocalBuffer.getNumChannels(), (int)idx);
                    float d = drumsBuffer.getSample(ch % drumsBuffer.getNumChannels(), (int)idx);
                    float b = bassBuffer.getSample(ch % bassBuffer.getNumChannels(), (int)idx);
                    
                    out[i] = (v * stemGains[0].getCurrentValue()) + 
                             (d * stemGains[1].getCurrentValue()) + 
                             (b * stemGains[2].getCurrentValue());
                }
            }
        }
    }
    for (int s = 0; s < NUM_STEMS; ++s) stemGains[s].skip(num);
    
    currentPeakLevel.store(mainBuffer->getMagnitude(start, num));

    if (masterRecorder.isRecording.load()) {
        const juce::ScopedLock sl(masterRecorder.lock);
        if (masterRecorder.writer != nullptr) {
            masterRecorder.writer->writeFromAudioSampleBuffer(*mainBuffer, start, num);
        }
    }
}

void AudioCore::processDeckDsp(juce::AudioBuffer<float>& buffer, DeckMixerState& dState, int numSamples)
{
    // 1. Apply EQ
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        int c = ch % 2;
        if (dState.lowFilter[c] && dState.midFilter[c] && dState.highFilter[c]) {
            float* d = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i) {
                d[i] = dState.lowFilter[c]->processSample(d[i]);
                d[i] = dState.midFilter[c]->processSample(d[i]);
                d[i] = dState.highFilter[c]->processSample(d[i]);
            }
        }
    }

    // 2. Apply 6-Slot FX
    bool anyFx = false;
    for (auto& slot : dState.fxSlots) if (slot.enabled) anyFx = true;

    if (anyFx) {
        float lfoInc = juce::MathConstants<float>::twoPi * (0.35f / (float)currentSampleRate);
        int delaySize = dState.fxDelayBuffer.getNumSamples();
        int numCh = buffer.getNumChannels();

        for (int ch = 0; ch < numCh; ++ch) {
            float* data = buffer.getWritePointer(ch);
            float* delayData = dState.fxDelayBuffer.getWritePointer(ch % dState.fxDelayBuffer.getNumChannels());
            int localWritePos = dState.fxWritePos;
            float localLfo = dState.fxLfoPhase;

            for (int i = 0; i < numSamples; ++i) {
                float input = data[i];
                float output = input;
                localLfo += lfoInc;
                if (localLfo > juce::MathConstants<float>::twoPi) localLfo -= juce::MathConstants<float>::twoPi;

                float dInput = input;
                if (dState.fxSlots[0].enabled) { // Delay
                    int dLen = (int)(currentSampleRate * 0.4);
                    float ds = delayData[(localWritePos - dLen + delaySize) % delaySize];
                    dInput = juce::jlimit(-1.0f, 1.0f, input + ds * 0.45f * dState.fxSlots[0].amount);
                    output = juce::jlimit(-1.0f, 1.0f, output + ds * dState.fxSlots[0].amount);
                } else if (dState.fxSlots[1].enabled) { // Echo
                    int eLen = (int)(currentSampleRate * 0.25);
                    float ds = delayData[(localWritePos - eLen + delaySize) % delaySize];
                    dInput = juce::jlimit(-1.0f, 1.0f, input + ds * 0.7f * dState.fxSlots[1].amount);
                    output = juce::jlimit(-1.0f, 1.0f, output + ds * dState.fxSlots[1].amount * 0.6f);
                } else if (dState.fxSlots[5].enabled) { // Dub Echo
                    int dLen = (int)(currentSampleRate * 0.38);
                    float ds = delayData[(localWritePos - dLen + delaySize) % delaySize];
                    dInput = (float)std::tanh(input + ds * 0.78f * dState.fxSlots[5].amount);
                    output = juce::jlimit(-1.0f, 1.0f, output + ds * dState.fxSlots[5].amount);
                }
                
                delayData[localWritePos] = dInput;

                if (dState.fxSlots[2].enabled) { // Reverb
                    float r1 = delayData[(localWritePos - (int)(currentSampleRate*0.03) + delaySize) % delaySize];
                    float r2 = delayData[(localWritePos - (int)(currentSampleRate*0.05) + delaySize) % delaySize];
                    output = juce::jlimit(-1.0f, 1.0f, output + (r1 + r2) * 0.4f * dState.fxSlots[2].amount);
                }

                if (dState.fxSlots[3].enabled) { // Flange
                    float mod = (1.0f + std::sin(localLfo)) * 140.0f;
                    output = (output + delayData[(localWritePos - (int)(120 + mod) + delaySize) % delaySize] * dState.fxSlots[3].amount) * 0.7f;
                }

                if (dState.fxSlots[4].enabled) { // Space
                    int sLen = (int)(currentSampleRate * (ch == 0 ? 0.005 : 0.015));
                    output = output * (1.0f - dState.fxSlots[4].amount * 0.5f) + delayData[(localWritePos - sLen + delaySize) % delaySize] * dState.fxSlots[4].amount * 0.5f;
                }

                data[i] = output;
                localWritePos = (localWritePos + 1) % delaySize;
            }
            if (ch == numCh - 1) {
                dState.fxWritePos = localWritePos;
                dState.fxLfoPhase = localLfo;
            }
        }
    }

    // 3. Apply XY Touch FX (4-quadrant)
    if (dState.xyEnabled) {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);

        // LP and HP always applied — transparent when open (no kill at center)
        dState.lpFilter.process(context);
        dState.hpFilter.process(context);

        // Top-Left quadrant: Flanger
        if (dState.flangerMix > 0.01f) {
            dState.flanger.setMix(dState.flangerMix * 0.7f);
            dState.flanger.process(context);
        }

        // Top-Right quadrant: Echo
        if (dState.echoMix > 0.01f) {
            for (int c = 0; c < buffer.getNumChannels(); ++c) {
                float* samples = buffer.getWritePointer(c);
                for (int j = 0; j < numSamples; ++j) {
                    float delayed = dState.echo.popSample(c);
                    dState.echo.pushSample(c, samples[j] + delayed * 0.5f);
                    samples[j] += delayed * dState.echoMix * 0.6f;
                }
            }
        }
    }
}

void AudioCore::handleSyncLogic() {
    if (masterDeckIdx < 0) return;
    
    int slaveIdx = (masterDeckIdx == 0) ? 1 : 0;
    if (!syncEnabled[slaveIdx]) return;
    
    double masterBpm = getDeckBpm(masterDeckIdx);
    double slaveBaseBpm = (slaveIdx == 0) ? deckABpm : deckBBpm;
    
    if (masterBpm > 0 && slaveBaseBpm > 0) {
        // 1. Continuous BPM Match
        double targetPitch = (masterBpm / slaveBaseBpm - 1.0) / 0.06;
        setDeckPitch(slaveIdx, targetPitch);
        
        // 2. Phase Sync (Beatmatch)
        if (isDeckPlaying(masterDeckIdx) && isDeckPlaying(slaveIdx)) {
            double masterPos = getDeckPosition(masterDeckIdx);
            double slavePos = getDeckPosition(slaveIdx);
            double beatDuration = 60.0 / masterBpm;
            
            double masterPhase = std::fmod(masterPos, beatDuration);
            double slavePhase = std::fmod(slavePos, beatDuration);
            
            double phaseDiff = masterPhase - slavePhase;
            if (phaseDiff > beatDuration * 0.5) phaseDiff -= beatDuration;
            else if (phaseDiff < -beatDuration * 0.5) phaseDiff += beatDuration;
            
            // Only snap if drift is > 10ms to avoid constant jumping
            if (std::abs(phaseDiff) > 0.01) {
                if (slaveIdx == 0) seekDeckA(slavePos + phaseDiff);
                else seekDeckB(slavePos + phaseDiff);
            }
        }
    }
}

bool AudioCore::loadDeckA(const juce::File& file, double bpm) {
    if (!file.existsAsFile()) return false;
    deckABpm = 0.0;
    
    // 1. Eject current track and reset state
    deckAChannel->transport->stop();
    deckAChannel->transport->setSource(nullptr);
    deckAChannel->readerSource.reset();
    thumbnail.clear(); // Clear old waveform immediately
    deckACue = 0.0;
    setDeckLoopEnabled(0, false);
    deckAPitch = 0.0; // Reset pitch to neutral
    
    // 2. Load new track
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr) {
        auto newSource = std::make_unique<CrossfadingLoopSource>(reader, true);
        deckAChannel->transport->setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        deckAChannel->readerSource = std::move(newSource);
        deckAName = file.getFileNameWithoutExtension();
        
        if (bpm <= 0.0 && trackDb != nullptr) {
            TrackDatabase::Track t;
            if (trackDb->getTrackByPath(file.getFullPathName(), t)) {
                bpm = t.bpm;
            }
        }
        if (bpm <= 0.0) bpm = 120.0;
        deckABpm = bpm;
        
        mainTrackBpm = deckABpm;
        thumbnail.setSource(new juce::FileInputSource(file));
        // Mixer decks do not use stems
        return true;
    }
    return false;
}

bool AudioCore::loadDeckB(const juce::File& file, double bpm) {
    if (!file.existsAsFile()) return false;
    deckBBpm = 0.0;

    // 1. Eject current track and reset state
    deckBChannel->transport->stop();
    deckBChannel->transport->setSource(nullptr);
    deckBChannel->readerSource.reset();
    thumbnailB.clear(); // Clear old waveform immediately
    deckBCue = 0.0;
    setDeckLoopEnabled(1, false);
    deckBPitch = 0.0; // Reset pitch to neutral

    // 2. Load new track
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr) {
        auto newSource = std::make_unique<CrossfadingLoopSource>(reader, true);
        deckBChannel->transport->setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        deckBChannel->readerSource = std::move(newSource);
        deckBName = file.getFileNameWithoutExtension();
        
        if (bpm <= 0.0 && trackDb != nullptr) {
            TrackDatabase::Track t;
            if (trackDb->getTrackByPath(file.getFullPathName(), t)) {
                bpm = t.bpm;
            }
        }
        if (bpm <= 0.0) bpm = 120.0;
        deckBBpm = bpm;
        
        thumbnailB.setSource(new juce::FileInputSource(file));
        return true;
    }
    return false;
}

bool AudioCore::loadHandsFreeDeck(const juce::File& file, double bpm) {
    if (!file.existsAsFile()) return false;

    // 1. Reset state
    handsFreeChannel->transport->stop();
    handsFreeChannel->transport->setSource(nullptr);
    handsFreeChannel->readerSource.reset();
    thumbnailH.clear(); // Clear old waveform immediately
    deckHCue = 0.0;
    setDeckLoopEnabled(2, false);
    deckHPitch = 0.0;

    // 2. Load new track
    auto* reader = formatManager.createReaderFor(file);
    if (reader != nullptr) {
        auto newSource = std::make_unique<CrossfadingLoopSource>(reader, true);
        handsFreeChannel->transport->setSource(newSource.get(), 0, nullptr, reader->sampleRate);
        handsFreeChannel->readerSource = std::move(newSource);
        deckHName = file.getFileNameWithoutExtension();
        if (bpm > 0.0) deckHBpm = bpm;
        else deckHBpm = detectBpm(file);
        mainTrackBpm = deckHBpm;
        thumbnailH.setSource(new juce::FileInputSource(file));
        
        // Load pre-computed stems for DJ Hands Free
        stemsAreReady = false;
        if (trackDb != nullptr) {
            TrackDatabase::Track t;
            if (trackDb->getTrackByPath(file.getFullPathName(), t)) {
                TrackDatabase::Analysis a;
                if (trackDb->loadAnalysis(t.id, a) && 
                    juce::File(a.vocalStemPath).existsAsFile() &&
                    juce::File(a.instrumentalStemPath).existsAsFile() &&
                    juce::File(a.beatStemPath).existsAsFile()) 
                {
                    // Load the pre-computed stems into the buffers in a background thread
                    std::thread([this, a]() {
                        auto loadBuffer = [this](const juce::String& path, juce::AudioBuffer<float>& buffer) {
                            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(juce::File(path)));
                            if (reader) {
                                buffer.setSize(reader->numChannels, (int)reader->lengthInSamples);
                                reader->read(&buffer, 0, (int)reader->lengthInSamples, 0, true, true);
                            } else {
                                buffer.setSize(2, 0);
                            }
                        };
                        loadBuffer(a.vocalStemPath, vocalBuffer);
                        loadBuffer(a.beatStemPath, drumsBuffer);
                        loadBuffer(a.instrumentalStemPath, bassBuffer);
                        stemsAreReady = true;
                    }).detach();
                } else {
                    asyncExtractStems(file); // Fallback to realtime if not processed
                }
            } else {
                asyncExtractStems(file);
            }
        } else {
            asyncExtractStems(file);
        }
        
        return true;
    }
    return false;
}

void AudioCore::playHandsFreeDeck() { handsFreeChannel->transport->start(); }
void AudioCore::stopHandsFreeDeck() { handsFreeChannel->transport->stop(); }

void AudioCore::triggerDeckCue (int deckIdx)
{
    auto* transport = (deckIdx == 0) ? deckAChannel->transport.get() : 
                     (deckIdx == 1 ? deckBChannel->transport.get() : handsFreeChannel->transport.get());
    
    if (transport == nullptr) return;

    double pos = transport->getCurrentPosition();
    double len = transport->getLengthInSeconds();

    // 1. If at the end of the track, always reset to the very beginning
    if (pos >= len - 0.1) {
        transport->stop();
        transport->setPosition(0.0);
        return;
    }

    if (transport->isPlaying()) {
        // 2. If playing, mark the current point as the new CUE and stop
        double markPos = pos;
        if (deckIdx == 0) deckACue = markPos;
        else if (deckIdx == 1) deckBCue = markPos;
        else deckHCue = markPos;
        
        transport->stop();
        transport->setPosition(markPos);
    } else {
        // 3. If paused/stopped, jump to the last marked CUE point
        double cuePos = getDeckCuePoint(deckIdx);
        transport->setPosition(cuePos);
    }
}

void AudioCore::seekHandsFreeDeck(double pos) { handsFreeChannel->transport->setPosition(pos); }
void AudioCore::ejectHandsFreeDeck() { handsFreeChannel->transport->stop(); handsFreeChannel->transport->setSource(nullptr); handsFreeChannel->readerSource.reset(); }



void AudioCore::playDeckA() { 
    if (masterDeckIdx == -1) masterDeckIdx = 0;
    deckAChannel->transport->start(); 
}
void AudioCore::stopDeckA() { deckAChannel->transport->stop(); }
void AudioCore::seekDeckA(double pos) { deckAChannel->transport->setPosition(pos); }
void AudioCore::ejectDeckA() { 
    deckAChannel->transport->stop(); 
    deckAChannel->transport->setSource(nullptr); 
    deckAChannel->readerSource.reset(); 
    deckAName = ""; 
    thumbnail.clear(); 
    deckACue = 0.0;
}

void AudioCore::playDeckB() { 
    if (masterDeckIdx == -1) masterDeckIdx = 1;
    deckBChannel->transport->start(); 
}
void AudioCore::stopDeckB() { deckBChannel->transport->stop(); }
void AudioCore::seekDeckB(double pos) { deckBChannel->transport->setPosition(pos); }
void AudioCore::ejectDeckB() { 
    deckBChannel->transport->stop(); 
    deckBChannel->transport->setSource(nullptr); 
    deckBChannel->readerSource.reset(); 
    deckBName = ""; 
    thumbnailB.clear(); 
    deckBCue = 0.0;
}

double AudioCore::getDeckPosition (int deckIdx) const {
    if (deckIdx == 0) return deckAChannel->transport->getCurrentPosition();
    if (deckIdx == 1) return deckBChannel->transport->getCurrentPosition();
    return handsFreeChannel->transport->getCurrentPosition();
}

double AudioCore::getDeckLength (int deckIdx) const {
    if (deckIdx == 0) return deckAChannel->transport->getLengthInSeconds();
    if (deckIdx == 1) return deckBChannel->transport->getLengthInSeconds();
    return handsFreeChannel->transport->getLengthInSeconds();
}

bool AudioCore::isDeckPlaying (int deckIdx) const {
    if (deckIdx == 0) return deckAChannel->transport->isPlaying();
    if (deckIdx == 1) return deckBChannel->transport->isPlaying();
    return handsFreeChannel->transport->isPlaying();
}

double AudioCore::getMainTrackPosition() const { return getDeckPosition(2); }
double AudioCore::getMainTrackLength() const { return getDeckLength(2); }
bool AudioCore::isMainTrackPlaying() const { return isDeckPlaying(2); }

void AudioCore::setMainTrackLoopRange(double startTime, double duration) {
    if (handsFreeChannel && handsFreeChannel->readerSource) {
        if (startTime <= 0) startTime = handsFreeChannel->transport->getCurrentPosition();
        handsFreeChannel->readerSource->setLoopRange((juce::int64)(startTime * currentSampleRate), (juce::int64)(duration * currentSampleRate));
    }
}

void AudioCore::setMainTrackLoopEnabled(bool enabled) {
    if (handsFreeChannel && handsFreeChannel->readerSource)
        handsFreeChannel->readerSource->setLooping(enabled);
}

bool AudioCore::isMainTrackLoopEnabled() const {
    if (handsFreeChannel && handsFreeChannel->readerSource)
        return handsFreeChannel->readerSource->isLooping();
    return false;
}

void AudioCore::setDeckLoopRange(int deckIdx, double startTime, double duration) {
    PlaybackChannel* channel = nullptr;
    if (deckIdx == 0) channel = deckAChannel.get();
    else if (deckIdx == 1) channel = deckBChannel.get();
    else channel = handsFreeChannel.get();

    if (channel && channel->readerSource) {
        if (startTime <= 0) startTime = channel->transport->getCurrentPosition();
        double sr = getDeckSourceSampleRate(deckIdx);
        channel->readerSource->setLoopRange((juce::int64)(startTime * sr), (juce::int64)(duration * sr));
    }
}

void AudioCore::setDeckLoopEnabled(int deckIdx, bool enabled) {
    PlaybackChannel* channel = nullptr;
    if (deckIdx == 0) channel = deckAChannel.get();
    else if (deckIdx == 1) channel = deckBChannel.get();
    else channel = handsFreeChannel.get();

    if (channel && channel->readerSource)
        channel->readerSource->setLooping(enabled);
}

bool AudioCore::isDeckLoopEnabled(int deckIdx) const {
    PlaybackChannel* channel = nullptr;
    if (deckIdx == 0) channel = deckAChannel.get();
    else if (deckIdx == 1) channel = deckBChannel.get();
    else channel = handsFreeChannel.get();

    if (channel && channel->readerSource)
        return channel->readerSource->isLooping();
    return false;
}

double AudioCore::getDeckLoopStart(int deckIdx) const {
    PlaybackChannel* channel = nullptr;
    if (deckIdx == 0) channel = deckAChannel.get();
    else if (deckIdx == 1) channel = deckBChannel.get();
    else channel = handsFreeChannel.get();

    if (channel && channel->readerSource)
        return (double)channel->readerSource->getLoopStart() / getDeckSourceSampleRate(deckIdx);
    return 0.0;
}

double AudioCore::getDeckLoopLength(int deckIdx) const {
    PlaybackChannel* channel = nullptr;
    if (deckIdx == 0) channel = deckAChannel.get();
    else if (deckIdx == 1) channel = deckBChannel.get();
    else channel = handsFreeChannel.get();

    if (channel && channel->readerSource)
        return (double)channel->readerSource->getLoopLength() / getDeckSourceSampleRate(deckIdx);
    return 0.0;
}

double AudioCore::getDeckSourceSampleRate(int deckIdx) const {
    PlaybackChannel* channel = nullptr;
    if (deckIdx == 0) channel = deckAChannel.get();
    else if (deckIdx == 1) channel = deckBChannel.get();
    else channel = handsFreeChannel.get();

    if (channel && channel->readerSource)
        return channel->readerSource->getSourceSampleRate();
    return 44100.0;
}


void AudioCore::asyncExtractStems(const juce::File& file)
{
    stemsAreReady = false;
    stemProgress = 0.0f;

    std::thread([this, file]() {
        juce::AudioFormatManager manager;
        manager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(file));
        
        if (reader == nullptr) return;

        int numCh = reader->numChannels;
        int numSamples = (int)reader->lengthInSamples;

        vocalBuffer.setSize(numCh, numSamples);
        drumsBuffer.setSize(numCh, numSamples);
        bassBuffer.setSize(numCh, numSamples);

        // Temp buffer for processing
        juce::AudioBuffer<float> tempBuffer(numCh, numSamples);
        reader->read(&tempBuffer, 0, numSamples, 0, true, true);

        // Proof of concept extraction (High-Precision 4th Order Crossover)
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* src = tempBuffer.getReadPointer(ch);
            auto* vOut = vocalBuffer.getWritePointer(ch);
            auto* dOut = drumsBuffer.getWritePointer(ch);
            auto* bOut = bassBuffer.getWritePointer(ch);

            // Per-channel internal filter states
            float lp[4] = {0,0,0,0};
            float hp[4] = {0,0,0,0};

            for (int i = 0; i < numSamples; ++i)
            {
                float s = src[i];
                
                // 4th Order LP (Bass < 150Hz) - coefficient calibrated for 44.1k
                float lpIn = s;
                for(int p=0; p<4; ++p) {
                    lp[p] += 0.022f * (lpIn - lp[p]);
                    lpIn = lp[p];
                }
                float bass = lp[3];

                // 4th Order HP (Drums/High > 4000Hz)
                float hpIn = s;
                for(int p=0; p<4; ++p) {
                    hp[p] += 0.22f * (hpIn - hp[p]);
                    hpIn = hp[p];
                }
                float highs = s - hp[3];

                bOut[i] = bass;
                dOut[i] = highs;
                vOut[i] = s - bass - highs; // Pure Mid/Vocal residue
            }
        }

        stemsAreReady = true;
        stemProgress = 1.0f;
    }).detach();
}

void AudioCore::releaseResources()
{
    mixer.releaseResources();
}

void AudioCore::setGlobalPitchRatio(double ratio) {
    setDeckPitch(2, ratio);
}

void AudioCore::setDeckPitch(int deckIdx, double pitch) {
    if (deckIdx == 0) deckAPitch = pitch;
    else if (deckIdx == 1) deckBPitch = pitch;
    else if (deckIdx == 2) deckHPitch = pitch;

    double speed = 1.0 + (pitch * 0.06);
    
    PlaybackChannel* channel = nullptr;
    if (deckIdx == 0) channel = deckAChannel.get();
    else if (deckIdx == 1) channel = deckBChannel.get();
    else if (deckIdx == 2) channel = handsFreeChannel.get();

    if (channel && channel->resampler)
        channel->resampler->setResamplingRatio(speed); 

    // If it's the global pitch (deck 2), also update all pads
    // (Note: Raw memory pads currently play at 1.0x speed. Resampling can be added later if needed)
}

double AudioCore::getDeckPitch(int deckIdx) const {
    if (deckIdx == 0) return deckAPitch;
    if (deckIdx == 1) return deckBPitch;
    return deckHPitch;
}

// -------------------------------------------------------------
// CrossfadingLoopSource Implementation (5ms Anti-Click)
// -------------------------------------------------------------

AudioCore::CrossfadingLoopSource::CrossfadingLoopSource(juce::AudioFormatReader* reader, bool deleteReaderWhenDone) 
    : readerSource(std::make_unique<juce::AudioFormatReaderSource>(reader, deleteReaderWhenDone))
{
    sourceSampleRate = reader->sampleRate;
    readerSource->setLooping(false); // Vamos controlar o loop manualmente para aplicar o crossfade
}

AudioCore::CrossfadingLoopSource::~CrossfadingLoopSource() {}

void AudioCore::CrossfadingLoopSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    crossfadeSamples = static_cast<int>(sampleRate * 0.005); // 5 milissegundos
    readerSource->prepareToPlay(samplesPerBlockExpected, sampleRate);
    tailBuffer.setSize(2, samplesPerBlockExpected + crossfadeSamples);
    headBuffer.setSize(2, samplesPerBlockExpected + crossfadeSamples);
}

void AudioCore::CrossfadingLoopSource::releaseResources() { readerSource->releaseResources(); }
void AudioCore::CrossfadingLoopSource::setNextReadPosition(juce::int64 newPosition) { readerSource->setNextReadPosition(newPosition); }
juce::int64 AudioCore::CrossfadingLoopSource::getNextReadPosition() const { return readerSource->getNextReadPosition(); }
juce::int64 AudioCore::CrossfadingLoopSource::getTotalLength() const { return readerSource->getTotalLength(); }
bool AudioCore::CrossfadingLoopSource::isLooping() const { return looping; }
void AudioCore::CrossfadingLoopSource::setLooping(bool shouldLoop) { looping = shouldLoop; }
void AudioCore::CrossfadingLoopSource::setLoopRange(juce::int64 start, juce::int64 length) { 
    loopStart = start; 
    loopLength = length; 
}

void AudioCore::CrossfadingLoopSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
    auto totalLen = readerSource->getTotalLength();
    if (totalLen == 0) return;

    // Se não estiver em modo loop de range, usa o comportamento padrão
    juce::int64 start = (looping && loopLength > 0) ? loopStart : 0;
    juce::int64 end = (looping && loopLength > 0) ? (loopStart + loopLength) : totalLen;
    end = juce::jmin(end, totalLen);

    if (!looping || (end - start) <= crossfadeSamples * 2) {
        readerSource->getNextAudioBlock(bufferToFill);
        if (looping && readerSource->getNextReadPosition() >= end)
            readerSource->setNextReadPosition(start);
        return;
    }

    int samplesLeft = bufferToFill.numSamples;
    int dstStart = bufferToFill.startSample;
    
    while (samplesLeft > 0)
    {
        auto pos = readerSource->getNextReadPosition();
        
        if (pos >= end - crossfadeSamples)
        {
            int samplesToProcess = juce::jmin(samplesLeft, (int)(end - pos));
            if (samplesToProcess <= 0) { readerSource->setNextReadPosition(start); continue; }

            tailBuffer.setSize(bufferToFill.buffer->getNumChannels(), samplesToProcess, false, false, true);
            tailBuffer.clear();
            juce::AudioSourceChannelInfo tailInfo(&tailBuffer, 0, samplesToProcess);
            readerSource->getNextAudioBlock(tailInfo);
            
            headBuffer.setSize(bufferToFill.buffer->getNumChannels(), samplesToProcess, false, false, true);
            headBuffer.clear();
            juce::AudioSourceChannelInfo headInfo(&headBuffer, 0, samplesToProcess);
            
            juce::int64 offset = pos - (end - crossfadeSamples);
            readerSource->setNextReadPosition(start + offset);
            readerSource->getNextAudioBlock(headInfo);
            
            readerSource->setNextReadPosition(start + offset + samplesToProcess);

            // Note: Hook to signal cue point reach
            // audioCore.setDeckCuePoint(2, 0.0);

            for (int chan = 0; chan < bufferToFill.buffer->getNumChannels(); ++chan)
            {
                auto* dst = bufferToFill.buffer->getWritePointer(chan, dstStart);
                auto* tailSrc = tailBuffer.getReadPointer(chan);
                auto* headSrc = headBuffer.getReadPointer(chan);
                for (int i = 0; i < samplesToProcess; ++i) {
                    float factor = (float)i / (float)samplesToProcess; 
                    dst[i] = tailSrc[i] * (1.0f - factor) + headSrc[i] * factor;
                }
            }
            dstStart += samplesToProcess;
            samplesLeft -= samplesToProcess;
        }
        else
        {
            int samplesToProcess = juce::jmin(samplesLeft, (int)((end - crossfadeSamples) - pos));
            if (samplesToProcess <= 0) { readerSource->setNextReadPosition(end - crossfadeSamples); continue; }

            juce::AudioSourceChannelInfo normalInfo(bufferToFill.buffer, dstStart, samplesToProcess);
            readerSource->getNextAudioBlock(normalInfo);
            dstStart += samplesToProcess;
            samplesLeft -= samplesToProcess;
        }
    }
}
// -------------------------------------------------------------


void AudioCore::setFxEnabled(int deckIdx, int fxIndex, bool enabled) {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    if (fxIndex >= 0 && fxIndex < 6) state.fxSlots[fxIndex].enabled = enabled;
}

void AudioCore::setFxAmount(int deckIdx, int fxIndex, float amount) {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    if (fxIndex >= 0 && fxIndex < 6) state.fxSlots[fxIndex].amount = amount;
}

bool AudioCore::isFxEnabled(int deckIdx, int fxIndex) const {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    if (fxIndex >= 0 && fxIndex < 6) return state.fxSlots[fxIndex].enabled;
    return false;
}

float AudioCore::getFxAmount(int deckIdx, int fxIndex) const {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    if (fxIndex >= 0 && fxIndex < 6) return state.fxSlots[fxIndex].amount;
    return 0.5f;
}

void AudioCore::setXyMode(int deckIdx, XyMode m) {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    state.xyMode = m;
}

void AudioCore::setXyFilter(int deckIdx, float x, float y) {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    state.curX = x; state.curY = y;

    // ── 4-Quadrant DJ Filter ──────────────────────────────────────────
    //  Top-Left       │ Top-Right
    //  LP + Flanger   │ HP + Echo
    //  ───────────────┼───────────────
    //  Bottom-Left    │ Bottom-Right
    //  LP + Resonance │ HP + Resonance
    //
    // X: LP left (min 300Hz) ←→ HP right (max 8kHz), transparent at x=0.5
    // Y: bottom = resonance boost · top = flanger (left) or echo (right)
    // LP/HP always on → at center they are fully open = no kill anywhere
    // ─────────────────────────────────────────────────────────────────

    state.flangerMix = 0.0f;
    state.echoMix    = 0.0f;
    float resonance  = 0.0f;

    if (x <= 0.5f) {
        // ── Left half: LP filter ──────────────────────────────────────
        float t = x * 2.0f;  // 0.0 (far left) → 1.0 (center)
        // 800 Hz at x=0 (audible bass+mids) → 20 000 Hz at x=0.5 (transparent)
        float lpCutoff = 800.0f * std::pow(20000.0f / 800.0f, t);

        if (y <= 0.5f) {
            // Bottom-Left: LP + Resonance
            resonance = (0.5f - y) * 2.0f * 0.72f;
        } else {
            // Top-Left: LP + Flanger
            state.flangerMix = (y - 0.5f) * 2.0f * 0.85f;
        }

        state.lpFilter.setCutoffFrequencyHz(lpCutoff);
        state.lpFilter.setResonance(resonance);
        state.hpFilter.setCutoffFrequencyHz(20.0f);   // HP open
        state.hpFilter.setResonance(0.0f);
    } else {
        // ── Right half: HP filter ─────────────────────────────────────
        float t = (x - 0.5f) * 2.0f; // 0.0 (center) → 1.0 (far right)
        // 20 Hz at x=0.5 (transparent) → 6 000 Hz at x=1.0
        float hpCutoff = 20.0f * std::pow(6000.0f / 20.0f, t);

        if (y <= 0.5f) {
            // Bottom-Right: HP + Resonance
            resonance = (0.5f - y) * 2.0f * 0.72f;
        } else {
            // Top-Right: HP + Echo
            state.echoMix = (y - 0.5f) * 2.0f * 0.85f;
        }

        state.hpFilter.setCutoffFrequencyHz(hpCutoff);
        state.hpFilter.setResonance(resonance);
        state.lpFilter.setCutoffFrequencyHz(20000.0f); // LP open
        state.lpFilter.setResonance(0.0f);
    }

    // Both filters always active; transparent when open
    state.lpMix = 1.0f;
    state.hpMix = 1.0f;
}
void AudioCore::setXyFilterEnabled(int deckIdx, bool enabled) {
    auto& state = (deckIdx == 0) ? deckAState : (deckIdx == 1 ? deckBState : deckHState);
    state.xyEnabled = enabled;
}

bool AudioCore::loadAudioFile (const juce::File& file, int padIndex, bool shouldLoop)
{
    if (padIndex < 0 || padIndex > 8) return false;
    if (!file.existsAsFile()) return false;

    auto* reader = formatManager.createReaderFor (file);
    if (reader != nullptr)
    {
        auto& pad = pads[padIndex];
        const juce::ScopedLock sl(pad.lock);
        
        int numSamples = (int)reader->lengthInSamples;
        int numCh = reader->numChannels;
        
        if (numSamples > pad.buffer.getNumSamples()) {
            pad.buffer.setSize(std::max(2, numCh), numSamples, false, true, false);
        }
        
        pad.buffer.clear();
        reader->read(&pad.buffer, 0, numSamples, 0, true, true);
        
        pad.playPosition = 0;
        pad.writePosition = numSamples; 
        pad.isLooping = shouldLoop;
        pad.hasAudio = true;
        pad.isPlaying = false;
        pad.isRecording = false;
        
        delete reader;
        return true;
    }
    return false;
}

double AudioCore::detectBpm(const juce::File& file)
{
    return 128.0; 
}

void AudioCore::playPad (int padIndex)
{
    if (padIndex < 0 || padIndex > 8) return;
    auto& pad = pads[padIndex];
    if (pad.hasAudio) {
        pad.playPosition = 0;
        pad.isPlaying = true;
    }
}

bool AudioCore::isPadPlaying (int padIndex) const
{
    if (padIndex < 0 || padIndex > 8) return false;
    return pads[padIndex].isPlaying;
}

void AudioCore::setPadLoop (int padIndex, bool shouldLoop)
{
    if (padIndex < 0 || padIndex > 8) return;
    pads[padIndex].isLooping = shouldLoop;
}

void AudioCore::stopPad (int padIndex)
{
    if (padIndex < 0 || padIndex > 8) return;
    pads[padIndex].isPlaying = false;
}

void AudioCore::ejectPad (int padIndex)
{
    if (padIndex < 0 || padIndex > 8) return;
    auto& pad = pads[padIndex];
    const juce::ScopedLock sl(pad.lock);
    pad.isPlaying = false;
    pad.isRecording = false;
    pad.hasAudio = false;
    pad.playPosition = 0;
    pad.writePosition = 0;
    pad.buffer.clear();
}

void AudioCore::setMasterVolume (float volume)
{
    masterVolume = volume;
}

void AudioCore::setTrackVolume(float volume)
{
    trackVolume = volume;
    setDeckVolume(2, volume); // Track Volume in Header/Hands-Free mode targets Deck H
}

void AudioCore::setPadVolume (int padIndex, float gain)
{
    if (padIndex < 0 || padIndex > 8) return;
    pads[padIndex].volume = gain;
}

void AudioCore::startPadRecording (int padIndex)
{
    if (padIndex < 0 || padIndex > 8) return;
    auto& pad = pads[padIndex];
    const juce::ScopedLock sl(pad.lock);
    
    pad.buffer.clear();
    pad.playPosition = 0;
    pad.writePosition = 0;
    pad.hasAudio = false; 
    pad.isPlaying = false;
    pad.isRecording = true;
}

void AudioCore::stopPadRecording (int padIndex, std::function<void(juce::File)> onFinished)
{
    if (padIndex < 0 || padIndex > 8) return;
    auto& pad = pads[padIndex];
    pad.isRecording = false;
    
    if (pad.writePosition > 0) {
        pad.hasAudio = true;
        
        // 1. Deep copy the buffer to avoid data races if the user clears the pad
        auto tempBuffer = std::make_shared<juce::AudioBuffer<float>>();
        int numSamples = pad.writePosition;
        {
            const juce::ScopedLock sl(pad.lock);
            tempBuffer->makeCopyOf(pad.buffer, true);
        }
        
        double sr = (currentSampleRate > 0) ? currentSampleRate : 44100.0;
        
        // 2. Save in background thread
        std::thread([tempBuffer, numSamples, padIndex, sr, onFinished]() {
            auto samplersDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                                .getChildFile("tridjs_lifeStudio")
                                .getChildFile("samplers");
            samplersDir.createDirectory();
            
            auto timeStr = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
            auto file = samplersDir.getChildFile("Pad_" + juce::String(padIndex + 1) + "_" + timeStr + ".wav");
            
            juce::WavAudioFormat wavFormat;
            if (auto outStream = std::make_unique<juce::FileOutputStream>(file)) {
                if (outStream->openedOk()) {
                    if (auto writer = std::unique_ptr<juce::AudioFormatWriter>(
                            wavFormat.createWriterFor(outStream.release(), sr, 2, 16, {}, 0))) 
                    {
                        writer->writeFromAudioSampleBuffer(*tempBuffer, 0, numSamples);
                        
                        // Fire callback on main thread if requested
                        if (onFinished) {
                            juce::MessageManager::callAsync([onFinished, file]() {
                                onFinished(file);
                            });
                        }
                        return; // Success
                    }
                }
            }
            // Fallback on failure
            if (onFinished) {
                juce::MessageManager::callAsync([onFinished]() { onFinished(juce::File()); });
            }
        }).detach();
        
    } else {
        if (onFinished) onFinished(juce::File());
    }
}

void AudioCore::setStemMuted(int stemIndex, bool muted)
{
    if (stemIndex >= 0 && stemIndex < NUM_STEMS) {
        stemMuted[stemIndex] = muted;
        stemGains[stemIndex].setTargetValue(muted ? 0.0f : 1.0f);
    }
}

bool AudioCore::isStemMuted(int stemIndex) const
{
    if (stemIndex >= 0 && stemIndex < NUM_STEMS)
        return stemMuted[stemIndex];
    return false;
}
void AudioCore::startMasterRecording()
{
    const juce::ScopedLock sl(masterRecorder.lock);
    if (masterRecorder.isRecording.load()) return;

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    masterRecorder.file = tempDir.getChildFile("temp_master_rec.wav");
    
    if (masterRecorder.file.existsAsFile())
        masterRecorder.file.deleteFile();

    auto* wavFormat = formatManager.findFormatForFileExtension("wav");
    
    if (wavFormat != nullptr) {
        auto* writer = wavFormat->createWriterFor(
            new juce::FileOutputStream(masterRecorder.file),
            currentSampleRate, 2, 24, {}, 0);

        if (writer != nullptr) {
            masterRecorder.writer.reset(writer);
            masterRecorder.isRecording = true;
        }
    }
}

void AudioCore::stopMasterRecording(std::function<void(juce::File)> onFinished)
{
    juce::File result;
    {
        const juce::ScopedLock sl(masterRecorder.lock);
        masterRecorder.isRecording = false;
        masterRecorder.writer.reset();
        result = masterRecorder.file;
    }
    
    if (onFinished)
        onFinished(result);
}
