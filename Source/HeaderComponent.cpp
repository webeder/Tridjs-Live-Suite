#include "HeaderComponent.h"
#include "LanguageManager.h"
#include "AudioCore.h"

HeaderComponent::HeaderComponent (AudioCore& engine) 
    : audioCore(engine),
      waveformDisplay(engine),
      vstWidget(engine.getVocalVstChain(), engine.getVstManager())
{
    addAndMakeVisible(waveformDisplay);
    waveformDisplay.onSeekToPosition = [this](double posSeconds) {
        if (onSeek) onSeek(posSeconds);
    };

    playBtn.onClick = [this] {
        if (audioCore.isMainTrackPlaying()) {
            audioCore.stopMainTrack();
        } else {
            audioCore.playMainTrack();
        }
    };
    addAndMakeVisible(playBtn);
    
    cueBtn.onClick = [this] {
        if (audioCore.isMainTrackPlaying()) {
            audioCore.stopMainTrack();
            audioCore.seekMainTrack(audioCore.getDeckCuePoint(2));
        } else {
            audioCore.triggerDeckCue(2);
        }
        repaint();
    };
    cueBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::yellow);
    cueBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    cueBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible(cueBtn);

    // VU Meter
    addAndMakeVisible(vuMeter);

    // Master Knob (0 to 100)
    masterKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    masterKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterKnob.setRange(0.0, 100.0, 1.0);
    masterKnob.setValue(100.0);
    masterKnob.setColour(juce::Slider::thumbColourId, juce::Colours::cyan);
    masterKnob.onValueChange = [this] {
        float vol = (float)(masterKnob.getValue() / 100.0);
        masterValue.setText(juce::String((int)masterKnob.getValue()), juce::dontSendNotification);
        if (onMasterVolumeChanged) onMasterVolumeChanged(vol);
    };
    addAndMakeVisible(masterKnob);
    
    masterLabel.setFont(juce::Font("Roboto", 10.0f, juce::Font::plain));
    masterLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey.withAlpha(0.8f));
    addAndMakeVisible(masterLabel);
    masterValue.setJustificationType(juce::Justification::centred);
    masterValue.setColour(juce::Label::textColourId, juce::Colours::cyan);
    masterValue.setFont(juce::Font("Roboto", 13.0f, juce::Font::bold));
    addAndMakeVisible(masterValue);

    // Volume Track Knob (0 to 100)
    volumeKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    volumeKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeKnob.setRange(0.0, 100.0, 1.0);
    volumeKnob.setValue(100.0);
    volumeKnob.setColour(juce::Slider::thumbColourId, juce::Colours::cyan);
    volumeKnob.onValueChange = [this] {
        float vol = (float)(volumeKnob.getValue() / 100.0);
        volumeValue.setText(juce::String((int)volumeKnob.getValue()), juce::dontSendNotification);
        if (onTrackVolumeChanged) onTrackVolumeChanged(vol);
    };
    addAndMakeVisible(volumeKnob);

    volumeLabel.setFont(juce::Font("Roboto", 10.0f, juce::Font::plain));
    volumeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey.withAlpha(0.8f));
    addAndMakeVisible(volumeLabel);
    volumeValue.setJustificationType(juce::Justification::centred);
    volumeValue.setColour(juce::Label::textColourId, juce::Colours::cyan);
    volumeValue.setFont(juce::Font("Roboto", 13.0f, juce::Font::bold));
    addAndMakeVisible(volumeValue);

    // Mic Controls
    micButton.setButtonText(juce::String::fromUTF8("\xf0\x9f\x8e\xa4")); // 🎤
    micButton.setClickingTogglesState(true);
    micButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222222));
    micButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
    micButton.onClick = [this] {
        audioCore.setMicEnabled(micButton.getToggleState());
    };
    addAndMakeVisible(micButton);

    micKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    micKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    micKnob.setRange(0.0, 100.0, 1.0);
    micKnob.setValue(100.0);
    micKnob.setColour(juce::Slider::thumbColourId, juce::Colours::orange);
    micKnob.onValueChange = [this] {
        float vol = (float)(micKnob.getValue() / 100.0);
        micValue.setText(juce::String((int)micKnob.getValue()), juce::dontSendNotification);
        audioCore.setMicVolume(vol);
    };
    addAndMakeVisible(micKnob);

    micLabel.setFont(juce::Font("Roboto", 10.0f, juce::Font::plain));
    micLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey.withAlpha(0.8f));
    micLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(micLabel);

    micValue.setJustificationType(juce::Justification::centred);
    micValue.setColour(juce::Label::textColourId, juce::Colours::orange);
    micValue.setFont(juce::Font("Roboto", 13.0f, juce::Font::bold));
    addAndMakeVisible(micValue);

    // LCD
    lcdClock.setFont(juce::Font(22.0f, juce::Font::bold));
    lcdClock.setColour(juce::Label::textColourId, juce::Colour((juce::uint32)0xffffa500));
    addAndMakeVisible(lcdClock);

    quantToggle.setClickingTogglesState(true);
    quantToggle.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff222222));
    quantToggle.setColour(juce::TextButton::buttonOnColourId, juce::Colours::cyan);
    addAndMakeVisible(quantToggle);

    // Record block
    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    recordButton.setColour (juce::TextButton::textColourOffId, juce::Colour(0xff00F0FF));
    recordButton.setColour (juce::TextButton::textColourOnId, juce::Colours::red);
    recordButton.setClickingTogglesState(true);
    recordButton.onClick = [this] {
        isRecording = recordButton.getToggleState();
        if (isRecording) recordStartTime = juce::Time::getMillisecondCounter();
        else recordDuration.setText("00:00:00", juce::dontSendNotification);
        
        if (onRecordToggled)
            onRecordToggled(isRecording);
    };
    addAndMakeVisible(recordButton);

    recordDuration.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    recordDuration.setColour(juce::Label::outlineColourId, juce::Colours::white.withAlpha(0.2f));
    recordDuration.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(recordDuration);

    ejectButton.setTooltip(TJS_L("MIXER_EJECT"));
    ejectButton.onClick = [this] { if (onEject) onEject(); };
    addAndMakeVisible(ejectButton);

    // Loop Controls
    addAndMakeVisible(autoLoopBtn);
    autoLoopBtn.setClickingTogglesState(true);
    autoLoopBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
    autoLoopBtn.onClick = [this] { applyCurrentLoop(); };

    addAndMakeVisible(loopInBtn);
    loopInBtn.setVisible(false);
    loopInBtn.onClick = [this] { 
        loopInPoint = audioCore.getDeckPosition(2); 
    };

    addAndMakeVisible(loopOutBtn);
    loopOutBtn.setVisible(false);
    loopOutBtn.onClick = [this] {
        double currentPos = audioCore.getDeckPosition(2);
        if (currentPos > loopInPoint + 0.1) {
            double duration = currentPos - loopInPoint;
            audioCore.setDeckLoopRange(2, loopInPoint, duration);
            audioCore.setDeckLoopEnabled(2, true);
            autoLoopBtn.setToggleState(true, juce::dontSendNotification);
            waveformDisplay.setLoopVisual(loopInPoint, duration, true);
        }
    };

    addAndMakeVisible(loopMinusBtn);
    loopMinusBtn.onClick = [this] {
        currentLoopBeats = std::max(1, currentLoopBeats / 2);
        applyCurrentLoop();
    };

    addAndMakeVisible(loopPlusBtn);
    loopPlusBtn.onClick = [this] {
        currentLoopBeats = std::min(32, currentLoopBeats * 2);
        applyCurrentLoop();
    };

    addAndMakeVisible(loopLabel);
    loopLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    loopLabel.setFont(juce::Font(10.0f, juce::Font::bold));

    addAndMakeVisible(loopSizeLabel);
    loopSizeLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    loopSizeLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    loopSizeLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(vstWidget);

    LanguageManager::getInstance().addChangeListener(this);
    updateLanguage();
    
    startTimer(30);
}

HeaderComponent::~HeaderComponent()
{
    LanguageManager::getInstance().removeChangeListener(this);
    stopTimer();
}

void HeaderComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    updateLanguage();
}

void HeaderComponent::updateLanguage()
{
    bool playing = audioCore.isMainTrackPlaying();
    playBtn.setButtonText(playing ? TJS_L("MIXER_STOP") : TJS_L("MIXER_PLAY"));
    cueBtn.setButtonText(TJS_L("MIXER_CUE"));
    autoLoopBtn.setButtonText(TJS_L("LOOP_AUTO"));
    loopInBtn.setButtonText(TJS_L("LOOP_IN"));
    loopOutBtn.setButtonText(TJS_L("LOOP_OUT"));
    loopLabel.setText(TJS_L("LOOP_TITLE"), juce::dontSendNotification);
    masterLabel.setText(TJS_L("MIXER_MASTER"), juce::dontSendNotification);
    volumeLabel.setText(TJS_L("MIXER_VOL_TRACK"), juce::dontSendNotification);
    micLabel.setText(TJS_L("MIC_LABEL"), juce::dontSendNotification);
    quantToggle.setButtonText(TJS_L("HF_QUANT"));
    recordButton.setButtonText(TJS_L("HF_REC"));
    
    waveformDisplay.repaint();
    repaint();
}

void HeaderComponent::timerCallback()
{
    if (isRecording)
    {
        auto elapsedMs = juce::Time::getMillisecondCounter() - recordStartTime;
        int totalSecs = (int)(elapsedMs / 1000);
        int mins = (totalSecs / 60) % 60;
        int hours = totalSecs / 3600;
        int secs = totalSecs % 60;
        
        recordDuration.setText(juce::String::formatted("%02d:%02d:%02d", hours, mins, secs), juce::dontSendNotification);
        // Time stays white as requested
        recordDuration.setColour(juce::Label::textColourId, juce::Colours::white);
    }

    waveformDisplay.repaint();
    repaint(); // For record dot flashing
}

void HeaderComponent::updateTransportInfo (double positionSeconds, double trackLengthSeconds, bool playing)
{
    waveformDisplay.trackLength = trackLengthSeconds;
    waveformDisplay.isPlaying = playing;
    waveformDisplay.currentPos = positionSeconds;
    
    waveformDisplay.updateOverlays(currentBpm, positionSeconds, playing);
    
    playBtn.setColour(juce::TextButton::buttonColourId, 
        playing ? juce::Colour((juce::uint32)0xff00ff55) : juce::Colour((juce::uint32)0xff00aa44));
    playBtn.setButtonText(playing ? TJS_L("MIXER_STOP") : TJS_L("MIXER_PLAY"));
    repaint();
}

void HeaderComponent::updatePeakLevel(float peak)
{
    vuMeter.setLevel(peak);
}

// ---------------------------------------------------------------
// VU Meter
// ---------------------------------------------------------------

void HeaderComponent::VuMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    
    // Dark background
    g.setColour(juce::Colour((juce::uint32)0xff0a0a0a));
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Border
    g.setColour(juce::Colour((juce::uint32)0xff444444));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    
    if (level <= 0.001f) return;
    
    float clampedLevel = juce::jlimit(0.0f, 1.5f, level);
    
    // Horizontal segments: left to right, green -> yellow -> red
    int totalSegments = 24;
    float segW = bounds.getWidth() / (float)totalSegments;
    int litSegments = (int)((clampedLevel / 1.5f) * totalSegments);
    
    for (int i = 0; i < litSegments && i < totalSegments; ++i)
    {
        float ratio = (float)i / (float)totalSegments;
        juce::Colour segColor;
        
        if (ratio < 0.5f)
            segColor = juce::Colour((juce::uint32)0xff00ff55);  // Bright green
        else if (ratio < 0.75f)
            segColor = juce::Colour((juce::uint32)0xffffee00);  // Bright yellow
        else
            segColor = juce::Colour((juce::uint32)0xffff2222);  // Bright red
        
        float x = bounds.getX() + i * segW;
        g.setColour(segColor);
        g.fillRoundedRectangle(x + 1, bounds.getY() + 2, segW - 2, bounds.getHeight() - 4, 2.0f);
    }
}

// ---------------------------------------------------------------
// WaveformDisplay
// ---------------------------------------------------------------

HeaderComponent::WaveformDisplay::WaveformDisplay (AudioCore& engine) 
    : audioCore(engine),
      thumbnail(engine.getThumbnail(2)) 
{
    formatManager.registerBasicFormats();
    // Register to be notified when the thumbnail is updated (new track loaded)
    thumbnail.addChangeListener(this);
    addAndMakeVisible(zoomInBtn);
    addAndMakeVisible(zoomOutBtn);
    zoomInBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x33ffffff));
    zoomOutBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x33ffffff));
    zoomInBtn.onClick = [this] { setZoomLevel(zoomFactor + 0.5); };
    addAndMakeVisible(zoomInBtn);
    bpmOverlay.setJustificationType(juce::Justification::centred);
    bpmOverlay.setFont(juce::Font("Roboto", 24.0f, juce::Font::bold));
    bpmOverlay.setColour(juce::Label::textColourId, juce::Colours::cyan);
    bpmOverlay.setInterceptsMouseClicks(false, false);
    bpmOverlay.setBorderSize(juce::BorderSize<int>(0));
    addAndMakeVisible(bpmOverlay);

    timeOverlay.setJustificationType(juce::Justification::centred);
    timeOverlay.setFont(juce::Font("Roboto", 18.0f, juce::Font::bold));
    timeOverlay.setColour(juce::Label::textColourId, juce::Colours::white);
    timeOverlay.setInterceptsMouseClicks(false, false);
    timeOverlay.setBorderSize(juce::BorderSize<int>(0));
    addAndMakeVisible(timeOverlay);
}

HeaderComponent::WaveformDisplay::~WaveformDisplay()
{
    thumbnail.removeChangeListener(this);
}
void HeaderComponent::WaveformDisplay::setLoopVisual(double start, double duration, bool active) {
    loopStart = start;
    loopDuration = duration;
    loopActive = active;
    repaint();
}

void HeaderComponent::WaveformDisplay::resized()
{
    auto area = getLocalBounds();
    
    // Esquerda: A maior parte para o Waveform (94%)
    auto leftCol = area.removeFromLeft((int)(getWidth() * 0.94f));
    // Track name e DECK serão desenhados manualmente no paint()

    // Direita: Botões e Tempo/BPM (Apenas 6%)
    auto rightCol = area;
    
    // Centralizando os botões no topo
    auto btns = rightCol.removeFromTop(25);
    int btnWidth = 18;
    int totalBtnsWidth = btnWidth * 2;
    int xOffset = (btns.getWidth() - totalBtnsWidth) / 2;
    auto btnsArea = btns.reduced(xOffset, 0); // Isso centraliza o par de botões
    
    zoomOutBtn.setBounds(btnsArea.removeFromRight(btnWidth).reduced(1)); 
    zoomInBtn.setBounds(btnsArea.removeFromRight(btnWidth).reduced(1));
    
    // Alinhamento centralizado no painel ultra estreito
    bpmOverlay.setBounds(rightCol.removeFromTop(30));
    timeOverlay.setBounds(rightCol.removeFromTop(25));
}

void HeaderComponent::WaveformDisplay::setZoomIn() { setZoomLevel(zoomFactor + 0.5); }
void HeaderComponent::WaveformDisplay::setZoomOut() { setZoomLevel(zoomFactor - 0.5); }

void HeaderComponent::WaveformDisplay::updateOverlays(double bpm, double time, bool playing)
{
    juce::Colour bpmCol = playing ? juce::Colours::cyan : juce::Colours::grey.withAlpha(0.5f);
    juce::Colour timeCol = playing ? juce::Colours::white : juce::Colours::grey.withAlpha(0.3f);
    
    bpmOverlay.setColour(juce::Label::textColourId, bpmCol);
    timeOverlay.setColour(juce::Label::textColourId, timeCol);
    
    bpmOverlay.setText(juce::String(bpm, 2), juce::dontSendNotification);
    
    int totalSecs = (int)std::abs(time);
    int mins = totalSecs / 60;
    int secs = totalSecs % 60;
    int ms = (int)(std::abs(time - (int)time) * 100);
    juce::String timeStr = (time < 0 ? "-" : "") + juce::String::formatted("%02d:%02d.%02d", mins, secs, ms);
    timeOverlay.setText(timeStr, juce::dontSendNotification);
}


void HeaderComponent::WaveformDisplay::setZoomLevel(double newZoom) {
    zoomFactor = juce::jlimit(1.0, 100.0, newZoom);
    repaint();
}

void HeaderComponent::WaveformDisplay::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) {
    if (wheel.deltaY != 0) {
        setZoomLevel(zoomFactor + (wheel.deltaY * 5.0)); // Ajuste de sensibilidade
    }
}

void HeaderComponent::WaveformDisplay::generateRgbWaveform (const juce::File& file)
{
    if (isAnalyzing) return;
    isAnalyzing = true;

    // Use a background thread to avoid freezing the UI
    std::thread([this, file]() {
        juce::AudioFormatManager manager;
        manager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (manager.createReaderFor (file));
        
        if (reader == nullptr) {
            isAnalyzing = false;
            return;
        }

        std::vector<SpectralPoint> tempSpectralData;
        double sampleRate = reader->sampleRate;
        juce::int64 numSamples = reader->lengthInSamples;
        
        int pointsPerSec = 50;
        int samplesPerPoint = (int)(sampleRate / pointsPerSec);
        int totalPoints = (int)(numSamples / samplesPerPoint);
        tempSpectralData.reserve(totalPoints);

        juce::IIRFilter lowFilter, midLowFilter, midHighFilter, highFilter;
        lowFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(sampleRate, 250.0));
        midLowFilter.setCoefficients(juce::IIRCoefficients::makeHighPass(sampleRate, 250.0));
        midHighFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(sampleRate, 4000.0));
        highFilter.setCoefficients(juce::IIRCoefficients::makeHighPass(sampleRate, 4000.0));

        juce::AudioBuffer<float> buffer (reader->numChannels, samplesPerPoint);
        
        for (int i = 0; i < totalPoints; ++i)
        {
            reader->read(&buffer, 0, samplesPerPoint, (juce::int64)i * samplesPerPoint, true, true);
            float maxL = 0, maxM = 0, maxH = 0;
            
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                auto* d = buffer.getReadPointer(ch);
                for (int s = 0; s < samplesPerPoint; ++s) {
                    float val = d[s];
                    maxL = std::max(maxL, std::abs(lowFilter.processSingleSampleRaw(val)));
                    maxM = std::max(maxM, std::abs(midHighFilter.processSingleSampleRaw(midLowFilter.processSingleSampleRaw(val))));
                    maxH = std::max(maxH, std::abs(highFilter.processSingleSampleRaw(val)));
                }
            }
            tempSpectralData.push_back({maxL, maxM, maxH});

            // Periodically check if we should abort? (Optional)
        }

        juce::MessageManager::callAsync([this, data = std::move(tempSpectralData), sr = sampleRate, np = numSamples]() mutable {
            spectralData = std::move(data);
            // Update trackLength from analysis if not already set by transport
            if (trackLength <= 0.0 && sr > 0)
                trackLength = (double)np / sr;
            isAnalyzing = false;
            repaint();
        });
    }).detach();
}

void HeaderComponent::WaveformDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    const float width = (float)getWidth();
    const float height = (float)getHeight();

    g.fillAll(juce::Colour(0xff0f0f0f)); 

    if (isAnalyzing)
    {
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(14.0f);
        g.drawText(TJS_L("HF_ANALYZING"), bounds, juce::Justification::centred);
    }

    if (loadedTrackName.isNotEmpty() && !spectralData.empty())
    {
        const float yCenter = height * 0.5f;

        // Derive effective track length from spectral data if transport hasn't updated yet
        const int pointsPerSec = 50;
        double effectiveLength = trackLength;
        if (effectiveLength <= 0.0 && !spectralData.empty())
            effectiveLength = (double)spectralData.size() / (double)pointsPerSec;

        double visibleDuration = (effectiveLength > 0) ? (effectiveLength / zoomFactor) : 10.0;
        double startTime = currentPos - (visibleDuration * 0.5);
        double endTime = currentPos + (visibleDuration * 0.5);

        if (zoomFactor <= 1.01) {
            startTime = 0;
            endTime = effectiveLength;
            visibleDuration = effectiveLength;
        }

        const double duration = endTime - startTime;

        
        const float vGainL = 2.8f; 
        const float vGainM = 4.1f; 
        const float vGainH = 3.9f;
        
        const float zoomRatio = juce::jlimit(0.0f, 1.0f, (float)((zoomFactor - 1.0) / 10.0));
        const float hiAlphaMult = 0.4f + (zoomRatio * 0.6f); 

        const int step = (zoomFactor > 15.0) ? 2 : 1; 
        const float halfH = height * 0.45f;

        const int waveformLimit = (int)(width * 0.94f);
        for (int x = 0; x < waveformLimit; x += step)
        {
            double timeAtX = startTime + ((double)x / width) * duration;
            if (timeAtX < 0 || timeAtX > effectiveLength) continue;


            int spectralIdx = (int)(timeAtX * pointsPerSec);
            
            if (spectralIdx >= 0 && spectralIdx < (int)spectralData.size())
            {
                auto const& p = spectralData[(size_t)spectralIdx];
                
                float r = p.low * vGainL;
                float g_comp = p.mid * vGainM;
                float b = p.high * vGainH;
                
                float maxComp = std::max({r, g_comp, b});
                if (maxComp < 0.05f) continue;
                
                r = juce::jlimit(0.0f, 1.0f, r);
                g_comp = juce::jlimit(0.0f, 1.0f, g_comp);
                b = juce::jlimit(0.0f, 1.0f, b);

                float totalAmp = juce::jlimit(0.0f, 1.0f, (r + g_comp + b) * 0.5f);
                float hh = totalAmp * halfH;

                juce::Colour c = juce::Colour::fromFloatRGBA(r, g_comp, b * hiAlphaMult, 1.0f);
                
                g.setColour(c);
                g.drawVerticalLine(x, yCenter - hh, yCenter + hh);
            }
        }

        // --- Overlays Background (Right Side Panel) ---
        float panelWidth = width * 0.06f;
        auto bgRect = juce::Rectangle<float>(width - panelWidth, 0.0f, panelWidth, height);

        g.setColour(juce::Colours::black);
        g.fillRect(bgRect); 
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawVerticalLine((int)(width - panelWidth), 0.0f, height);

        // --- CUE e Playhead ---
        const float clipLimit = width * 0.94f;

        double cuePoint = audioCore.getDeckCuePoint(2);
        if (cuePoint >= startTime && cuePoint <= endTime) {
            auto cueX = ((cuePoint - startTime) / duration) * width;
            if (cueX < clipLimit) {
                g.setColour(juce::Colours::orange);
                g.fillRect((float)cueX - 1.0f, 0.0f, 3.0f, height);
            }
        }

        auto playX = ((currentPos - startTime) / duration) * width;

        if (loopActive && loopDuration > 0.0) {
            double lStart = std::max(startTime, loopStart);
            double lEnd = std::min(endTime, loopStart + loopDuration);
            if (lEnd > lStart) {
                float lx1 = (float)(((lStart - startTime) / duration) * width);
                float lx2 = (float)(((lEnd - startTime) / duration) * width);
                
                lx2 = std::min(lx2, clipLimit);
                if (lx2 > lx1 && lx1 < clipLimit) {
                    g.setColour(juce::Colours::yellow.withAlpha(0.4f));
                    g.fillRect(lx1, 0.0f, lx2 - lx1, height);
                    g.setColour(juce::Colours::yellow.withAlpha(0.8f));
                    g.drawRect(lx1, 0.0f, lx2 - lx1, height, 2.0f);
                }
            }
        }

        if (playX < clipLimit) {
            g.setColour(juce::Colours::white);
            g.fillRect((float)playX - 1.0f, 0.0f, 2.0f, height);
        }
    }
    else if (isDraggingOver) {
        g.fillAll(juce::Colours::cyan.withAlpha(0.2f));
    }

    // --- DECK identifier (Red square with 'A') ---
    g.setColour(juce::Colours::red);
    g.fillRect(10, 10, 26, 26);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font("Roboto", 18.0f, juce::Font::bold));
    g.drawText("A", 10, 10, 26, 26, juce::Justification::centred);

    if (loadedTrackName.isEmpty()) {
        // Draw DECK Label next to the 'A' square
        g.setFont(juce::Font("Roboto", 22.0f, juce::Font::bold));
        g.setColour(juce::Colours::black);
        g.drawText("DECK", 43, 12, 100, 25, juce::Justification::left);
        g.setColour(juce::Colours::cyan.withAlpha(0.8f));
        g.drawText("DECK", 42, 11, 100, 25, juce::Justification::left);
    }
    else {
        // --- Track Name (Visible only when loaded, at the BOTTOM) ---
        juce::String status = isPlaying ? TJS_L("HF_PLAYING") : TJS_L("HF_READY");
        juce::String displayName = status + loadedTrackName;
        g.setFont(juce::Font("Roboto", 18.0f, juce::Font::bold));
        
        int tx = 15;
        int ty = (int)height - 30; 
        
        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.9f));
        g.drawText(displayName, tx + 1, ty + 1, (int)width - 150, 25, juce::Justification::left);
        g.drawText(displayName, tx + 2, ty + 2, (int)width - 150, 25, juce::Justification::left);
        // Main Text
        g.setColour(juce::Colours::white);
        g.drawText(displayName, tx, ty, (int)width - 150, 25, juce::Justification::left);
    }
}

void HeaderComponent::WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (loadedTrackName.isEmpty() || spectralData.empty())
        return;

    // Se estiver clicando nos botões de zoom, não faz seek
    if (zoomInBtn.getBounds().contains(e.getPosition()) || zoomOutBtn.getBounds().contains(e.getPosition()))
        return;

    const int pointsPerSec = 50;
    double effectiveLength = trackLength;
    if (effectiveLength <= 0.0 && !spectralData.empty())
        effectiveLength = (double)spectralData.size() / (double)pointsPerSec;

    double visibleDuration = (effectiveLength > 0) ? (effectiveLength / zoomFactor) : 0.0;
    double startTime = currentPos - (visibleDuration * 0.5);
    
    if (zoomFactor <= 1.01) startTime = 0;

    double clickRatio = (double)e.x / (double)getWidth();
    double seekPos = startTime + (clickRatio * visibleDuration);
    
    seekPos = juce::jlimit(0.0, effectiveLength, seekPos);
    
    if (onSeekToPosition)
        onSeekToPosition(seekPos);

    // Se estiver parado, clicar também marca o ponto de CUE
    if (!isPlaying) {
        audioCore.setDeckCuePoint(2, seekPos);
        repaint();
    }
}

// ---------------------------------------------------------------
// Layout
// ---------------------------------------------------------------

void HeaderComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour ((juce::uint32)0xff252525));
    
    // Desenhar o Dot do Record (Horizontal)
    auto recBounds = recordButton.getBounds();
    float dotSize = 12.0f;
    float dotX = (float)recBounds.getX() - 14.0f;
    float dotY = (float)recBounds.getCentreY() - (dotSize / 2.0f);
    
    if (isRecording) {
        bool flash = (juce::Time::getMillisecondCounter() % 1000 < 500);
        g.setColour(flash ? juce::Colours::red.withAlpha(0.4f) : juce::Colours::red.withAlpha(0.2f));
        g.fillEllipse(dotX - 4, dotY - 4, dotSize + 8, dotSize + 8);
        g.setColour(flash ? juce::Colours::red : juce::Colours::red.withAlpha(0.6f));
        g.fillEllipse(dotX, dotY, dotSize, dotSize);
    } else {
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.fillEllipse(dotX, dotY, dotSize, dotSize);
    }
}

void HeaderComponent::resized()
{
    auto area = getLocalBounds().reduced(5);
    

    auto topArea = area.removeFromTop(85);
    
    // Waveform takes most of the remaining top
    waveformDisplay.setBounds(topArea);

    // Bottom area (controls row)
    auto bottomArea = area;
    
    // Master knob + value + label
    auto mastArea = bottomArea.removeFromLeft(75);
    masterValue.setBounds(mastArea.removeFromTop(20));
    masterKnob.setBounds(mastArea.removeFromTop(mastArea.getHeight() - 20));
    masterLabel.setBounds(mastArea);
    
    // Volume Track knob + value + label
    auto volArea = bottomArea.removeFromLeft(75);
    volumeValue.setBounds(volArea.removeFromTop(20));
    volumeKnob.setBounds(volArea.removeFromTop(volArea.getHeight() - 20));
    volumeLabel.setBounds(volArea);

    // Transport buttons: PLAY/STOP | CUE
    auto transportArea = bottomArea.removeFromLeft(110);
    auto btnH = transportArea.getHeight();
    playBtn.setBounds(transportArea.removeFromLeft(55).reduced(2).withHeight(btnH));
    cueBtn.setBounds(transportArea.removeFromLeft(55).reduced(2).withHeight(btnH));

    // VU Meter
    vuMeter.setBounds(bottomArea.removeFromLeft(120).reduced(2, 8));

    // Right side: Eject and Quantize
    auto rightBtns = bottomArea.removeFromRight(120);
    ejectButton.setBounds(rightBtns.removeFromLeft(60).withSizeKeepingCentre(55, 30));
    quantToggle.setBounds(rightBtns.removeFromLeft(60).withSizeKeepingCentre(55, 30));

    // Loop Controls (Retro Layout)
    auto loopArea = bottomArea.removeFromLeft(190); 
    autoLoopBtn.setBounds(loopArea.removeFromLeft(80).reduced(4, 10));
    
    auto adjArea = loopArea.reduced(2, 2);
    auto topRow = adjArea.removeFromTop(adjArea.getHeight() / 2);
    loopLabel.setBounds(topRow.removeFromLeft(45));
    loopSizeLabel.setBounds(topRow);
    
    auto botRow = adjArea;
    loopMinusBtn.setBounds(botRow.removeFromLeft(45).reduced(8, 2));
    loopPlusBtn.setBounds(botRow.removeFromLeft(45).reduced(8, 2));

    // Record block (Retro Layout)
    bottomArea.removeFromLeft(15);
    auto recBlock = bottomArea.removeFromLeft(160).reduced(5, 5);
    recordButton.setBounds(recBlock.removeFromLeft(50));
    recordDuration.setBounds(recBlock.reduced(0, 2));

    // Mic Section
    auto micArea = bottomArea.removeFromLeft(80).reduced(5, 5);
    micButton.setBounds(micArea.removeFromLeft(35).withSizeKeepingCentre(30, 30));
    micValue.setBounds(micArea.removeFromTop(20));
    micKnob.setBounds(micArea.removeFromTop(micArea.getHeight() - 20));
    micLabel.setBounds(micArea);

    // VST Widget
    vstWidget.setBounds(bottomArea.removeFromLeft(90).reduced(2, 5));
}

// ---------------------------------------------------------------
// OS File Drag And Drop
// ---------------------------------------------------------------

bool HeaderComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& file : files) {
        if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".mp3"))
            return true;
    }
    return false;
}

void HeaderComponent::filesDropped(const juce::StringArray& files, int, int)
{
    waveformDisplay.isDraggingOver = false;
    for (auto& file : files) {
        if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".mp3")) {
            juce::File f(file);
            waveformDisplay.loadedTrackName = f.getFileNameWithoutExtension();
            waveformDisplay.repaint();
            if (onFileDropped) onFileDropped(f);
            break;
        }
    }
}

void HeaderComponent::fileDragEnter(const juce::StringArray&, int, int)
{
    waveformDisplay.isDraggingOver = true;
    waveformDisplay.repaint();
}

void HeaderComponent::fileDragExit(const juce::StringArray&)
{
    waveformDisplay.isDraggingOver = false;
    waveformDisplay.repaint();
}

// ---------------------------------------------------------------
// Internal FileTree Drag And Drop
// ---------------------------------------------------------------

bool HeaderComponent::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::String desc = details.description.toString();
    // Handle both: single absolute path, or pipe-separated paths from TrackBrowserComponent
    if (desc.contains("|")) {
        auto firstPath = desc.upToFirstOccurrenceOf("|", false, false).trim();
        return juce::File::isAbsolutePath(firstPath);
    }
    return desc == "AudioFileSelected" || juce::File::isAbsolutePath(desc);
}

void HeaderComponent::itemDropped(const juce::DragAndDropTarget::SourceDetails& details)
{
    waveformDisplay.isDraggingOver = false;
    juce::File fileToLoad;

    if (auto* tree = dynamic_cast<juce::FileTreeComponent*>(details.sourceComponent.get()))
    {
        fileToLoad = tree->getSelectedFile();
    }
    else
    {
        juce::String desc = details.description.toString();
        // Handle pipe-separated paths from TrackBrowserComponent
        juce::String firstPath = desc.contains("|")
            ? desc.upToFirstOccurrenceOf("|", false, false).trim()
            : desc.trim();
        if (juce::File::isAbsolutePath(firstPath))
            fileToLoad = juce::File(firstPath);
    }

    if (fileToLoad.existsAsFile())
    {
        waveformDisplay.loadedTrackName = fileToLoad.getFileNameWithoutExtension();
        waveformDisplay.isPlaying = false;
        audioCore.setDeckCuePoint(2, 0.0);
        // generateRgbWaveform é chamado dentro de onFileDropped (loadTrackWithMetadata)
        if (onFileDropped) onFileDropped(fileToLoad);
    }
    
    waveformDisplay.repaint();
}

void HeaderComponent::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
    if (details.description.toString() == "AudioFileSelected") {
        waveformDisplay.isDraggingOver = true;
        waveformDisplay.repaint();
    }
}

void HeaderComponent::itemDragExit(const juce::DragAndDropTarget::SourceDetails& details)
{
    waveformDisplay.isDraggingOver = false;
    waveformDisplay.repaint();
}

void HeaderComponent::updateMasterVolumeFromExtern(float v)
{
    masterKnob.setValue(v * 100.0, juce::dontSendNotification);
    masterValue.setText(juce::String((int)(v * 100.0)), juce::dontSendNotification);
}

void HeaderComponent::updateTrackVolumeFromExtern(float v)
{
    volumeKnob.setValue(v * 100.0, juce::dontSendNotification);
    volumeValue.setText(juce::String((int)(v * 100.0)), juce::dontSendNotification);
}

void HeaderComponent::applyCurrentLoop()
{
    loopSizeLabel.setText(juce::String(currentLoopBeats) + " BEATS", juce::dontSendNotification);
    
    if (autoLoopBtn.getToggleState()) {
        double duration = (60.0 / currentBpm) * currentLoopBeats;
        
        // Se já estiver em loop, mantém o início. Se não, pega a posição atual.
        double start = audioCore.isDeckLoopEnabled(2) ? audioCore.getDeckLoopStart(2) : audioCore.getDeckPosition(2);
        
        audioCore.setDeckLoopRange(2, start, duration);
        audioCore.setDeckLoopEnabled(2, true);
        
        waveformDisplay.setLoopVisual(start, duration, true);
    } else {
        audioCore.setDeckLoopEnabled(2, false);
        waveformDisplay.setLoopVisual(0, 0, false);
    }
}

void HeaderComponent::updateBpmDisplay(double bpm) { 
    currentBpm = bpm;
}
