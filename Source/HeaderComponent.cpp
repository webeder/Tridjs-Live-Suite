#include "HeaderComponent.h"

HeaderComponent::HeaderComponent (juce::AudioThumbnail& thumb) : waveformDisplay(thumb)
{
    addAndMakeVisible(waveformDisplay);
    waveformDisplay.onSeekToPosition = [this](double posSeconds) {
        if (onSeek) onSeek(posSeconds);
    };

    // Transport Buttons
    playBtn.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff1a6b1a));
    playBtn.onClick = [this] {
        // If CUE is set and we're stopped, play from CUE point
        if (!waveformDisplay.isPlaying && waveformDisplay.cuePoint > 0.0) {
            if (onSeek) onSeek(waveformDisplay.cuePoint);
        }
        if (onPlay) onPlay();
    };
    addAndMakeVisible(playBtn);

    stopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff666666));
    stopBtn.onClick = [this] { if (onStop) onStop(); };
    addAndMakeVisible(stopBtn);

    ejectButton.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff553333));
    ejectButton.onClick = [this] {
        waveformDisplay.loadedTrackName = "";
        waveformDisplay.currentPos = 0.0;
        waveformDisplay.cuePoint = 0.0;
        waveformDisplay.isPlaying = false;
        waveformDisplay.repaint();
        if (onEject) onEject();
    };
    addAndMakeVisible(ejectButton);

    cueBtn.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff995500));
    cueBtn.onClick = [this] {
        if (waveformDisplay.isPlaying) {
            // While playing: save current pos as cue
            waveformDisplay.cuePoint = waveformDisplay.currentPos;
        } else {
            // While stopped: seek to cue point
            if (onSeek) onSeek(waveformDisplay.cuePoint);
        }
        repaint();
    };
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
    
    masterLabel.setJustificationType(juce::Justification::centred);
    masterLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    masterLabel.setFont(juce::Font(10.0f));
    addAndMakeVisible(masterLabel);
    masterValue.setJustificationType(juce::Justification::centred);
    masterValue.setColour(juce::Label::textColourId, juce::Colours::cyan);
    masterValue.setFont(juce::Font(11.0f, juce::Font::bold));
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

    volumeLabel.setJustificationType(juce::Justification::centred);
    volumeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    volumeLabel.setFont(juce::Font(10.0f));
    addAndMakeVisible(volumeLabel);
    volumeValue.setJustificationType(juce::Justification::centred);
    volumeValue.setColour(juce::Label::textColourId, juce::Colours::cyan);
    volumeValue.setFont(juce::Font(11.0f, juce::Font::bold));
    addAndMakeVisible(volumeValue);

    // LCD
    lcdClock.setFont(juce::Font(22.0f, juce::Font::bold));
    lcdClock.setColour(juce::Label::textColourId, juce::Colour((juce::uint32)0xffffa500));
    addAndMakeVisible(lcdClock);

    quantToggle.setClickingTogglesState(true);
    quantToggle.setColour(juce::TextButton::buttonColourId, juce::Colour((juce::uint32)0xff222222));
    quantToggle.setColour(juce::TextButton::buttonOnColourId, juce::Colours::cyan);
    addAndMakeVisible(quantToggle);

    gearBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(gearBtn);

    // Pitch Slider (-1.0 to 1.0 mapped to -8% to +8%)
    pitchSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    pitchSlider.setRange(-1.0, 1.0, 0.001);
    pitchSlider.setValue(0.0);
    pitchSlider.setColour(juce::Slider::thumbColourId, juce::Colours::orange);
    pitchSlider.onValueChange = [this] {
        float val = (float)pitchSlider.getValue();
        float percent = val * 8.0f;
        pitchValue.setText((percent >= 0 ? "+" : "") + juce::String(percent, 1) + "%", juce::dontSendNotification);
        if (onPitchChanged) onPitchChanged(val);
    };
    addAndMakeVisible(pitchSlider);

    pitchLabel.setJustificationType(juce::Justification::centred);
    pitchLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    pitchLabel.setFont(juce::Font(10.0f));
    addAndMakeVisible(pitchLabel);

    pitchValue.setJustificationType(juce::Justification::centred);
    pitchValue.setColour(juce::Label::textColourId, juce::Colours::orange);
    pitchValue.setFont(juce::Font(14.0f, juce::Font::bold));
    pitchValue.setText("0.0%", juce::dontSendNotification);
    addAndMakeVisible(pitchValue);

    bpmDisplay.setFont(juce::Font(16.0f, juce::Font::bold));
    bpmDisplay.setColour(juce::Label::textColourId, juce::Colours::cyan);
    bpmDisplay.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmDisplay);

    // Record block
    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour ((juce::uint32)0x33aa2222));
    recordButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red);
    recordButton.setClickingTogglesState(true);
    recordButton.onClick = [this] {
        isRecording = recordButton.getToggleState();
        if (onRecordToggled)
            onRecordToggled(isRecording);
    };
    addAndMakeVisible (recordButton);

    addAndMakeVisible(recordDuration);

    // Loop Setup
    loopBtn.setClickingTogglesState(true);
    loopBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff444444));
    loopBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
    loopBtn.onClick = [this] {
        bool active = loopBtn.getToggleState();
        if (active) {
            int beats = loopSizeCombo.getSelectedId();
            if (beats <= 0) beats = 4;
            double duration = (60.0 / currentBpm) * beats;
            double start = waveformDisplay.currentPos;
            if (onLoopSet) onLoopSet(start, duration);
            waveformDisplay.setLoopVisual(start, duration, true);
        } else {
            waveformDisplay.setLoopVisual(0, 0, false);
        }
        if (onLoopEnabled) onLoopEnabled(active);
    };
    addAndMakeVisible(loopBtn);

    loopSizeCombo.addItem("1/2", 0); // Need to handle fractions differently? User asked for 1, 2, 4, 8, 16, 32
    loopSizeCombo.addItem("1 Beat", 1);
    loopSizeCombo.addItem("2 Beats", 2);
    loopSizeCombo.addItem("4 Beats", 4);
    loopSizeCombo.addItem("8 Beats", 8);
    loopSizeCombo.addItem("16 Beats", 16);
    loopSizeCombo.addItem("32 Beats", 32);
    loopSizeCombo.setSelectedId(4);
    loopSizeCombo.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(loopSizeCombo);

    startTimer(30);
}

HeaderComponent::~HeaderComponent()
{
    stopTimer();
}

void HeaderComponent::timerCallback()
{
    waveformDisplay.repaint();
}

void HeaderComponent::updateTransportInfo(double positionSeconds, double trackLengthSeconds, bool playing)
{
    waveformDisplay.currentPos = positionSeconds;
    waveformDisplay.trackLength = trackLengthSeconds;
    waveformDisplay.isPlaying = playing;
    
    int totalSecs = (int)positionSeconds;
    int hrs = totalSecs / 3600;
    int mins = (totalSecs % 3600) / 60;
    int secs = totalSecs % 60;
    
    lcdClock.setText(juce::String::formatted("%02d:%02d:%02d", hrs, mins, secs), juce::dontSendNotification);
    
    playBtn.setColour(juce::TextButton::buttonColourId, 
        playing ? juce::Colour((juce::uint32)0xff00cc00) : juce::Colour((juce::uint32)0xff1a6b1a));
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

HeaderComponent::WaveformDisplay::WaveformDisplay (juce::AudioThumbnail& thumb) : thumbnail (thumb) {
    formatManager.registerBasicFormats();
    addAndMakeVisible(zoomInBtn);
    addAndMakeVisible(zoomOutBtn);
    zoomInBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x33ffffff));
    zoomOutBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x33ffffff));
    zoomInBtn.onClick = [this] { setZoomLevel(zoomFactor + 0.5); };
    zoomOutBtn.onClick = [this] { setZoomLevel(zoomFactor - 0.5); };
}
void HeaderComponent::WaveformDisplay::setLoopVisual(double start, double duration, bool active) {
    loopStart = start;
    loopDuration = duration;
    loopActive = active;
    repaint();
}

void HeaderComponent::WaveformDisplay::resized() {
    auto r = getLocalBounds().removeFromTop(25).removeFromRight(50);
    zoomOutBtn.setBounds(r.removeFromLeft(22).reduced(1));
    zoomInBtn.setBounds(r.removeFromLeft(22).reduced(1));
}

void HeaderComponent::WaveformDisplay::setZoomIn() { setZoomLevel(zoomFactor + 0.5); }
void HeaderComponent::WaveformDisplay::setZoomOut() { setZoomLevel(zoomFactor - 0.5); }

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
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return;

    spectralData.clear();
    double sampleRate = reader->sampleRate;
    int64 numSamples = reader->lengthInSamples;
    
    // Análise de alta resolução: 50 pontos por segundo
    int pointsPerSec = 50;
    int samplesPerPoint = (int)(sampleRate / pointsPerSec);
    int totalPoints = (int)(numSamples / samplesPerPoint);
    spectralData.reserve(totalPoints);

    juce::IIRFilter lowFilter, midLowFilter, midHighFilter, highFilter;
    lowFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(sampleRate, 250.0));
    midLowFilter.setCoefficients(juce::IIRCoefficients::makeHighPass(sampleRate, 250.0));
    midHighFilter.setCoefficients(juce::IIRCoefficients::makeLowPass(sampleRate, 4000.0));
    highFilter.setCoefficients(juce::IIRCoefficients::makeHighPass(sampleRate, 4000.0));

    juce::AudioBuffer<float> buffer (reader->numChannels, samplesPerPoint);
    
    for (int i = 0; i < totalPoints; ++i)
    {
        reader->read(&buffer, 0, samplesPerPoint, (int64)i * samplesPerPoint, true, true);
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
        spectralData.push_back({maxL, maxM, maxH});
    }
    repaint();
}

void HeaderComponent::WaveformDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xff0f0f0f)); // Fundo Rekordbox

    if (loadedTrackName.isNotEmpty() && spectralData.size() > 0)
    {
        float width = (float)getWidth();
        float height = (float)getHeight();
        float yCenter = height * 0.5f;

        // Centralização no Playhead: Waveform corre por baixo
        double visibleDuration = (trackLength > 0) ? (trackLength / zoomFactor) : 10.0;
        double startTime = currentPos - (visibleDuration * 0.5);
        double endTime = currentPos + (visibleDuration * 0.5);

        // Se zoom for 1.0, opcionalmente mostrar a track toda ou manter o comportamento de centro
        if (zoomFactor <= 1.01) {
            startTime = 0;
            endTime = trackLength;
            visibleDuration = trackLength;
        }

        double duration = endTime - startTime;
        int pointsPerSec = 50;
        
        // Ganhos Espectrais (Conforme Requisito)
        float vGainL = 1.0f;
        float vGainM = 1.3f;
        float vGainH = 1.1f;
        
        // Estética das Ondas: Ajustar opacidade baseada no Zoom
        float zoomRatio = juce::jlimit(0.0f, 1.0f, (float)((zoomFactor - 1.0) / 10.0));
        float hiAlphaMult = 0.4f + (zoomRatio * 0.6f); 

        int step = (zoomFactor > 10.0) ? 2 : 1; 

        for (int x = 0; x < getWidth(); x += step)
        {
            double timeAtX = startTime + ((double)x / width) * duration;
            if (timeAtX < 0 || timeAtX > trackLength) continue;

            int spectralIdx = (int)(timeAtX * pointsPerSec);
            
            if (spectralIdx >= 0 && spectralIdx < (int)spectralData.size())
            {
                auto& p = spectralData[spectralIdx];
                
                // Aplicação de Ganhos e Threshold (Gatilho de Transparência 5%)
                float l = p.low * vGainL * 2.8f; 
                float m = p.mid * vGainM * 3.2f;
                float h = p.high * vGainH * 3.6f;

                float maxAmp = std::max({l, m, h});
                if (maxAmp < 0.05f) continue; // Gatilho de Transparência: Revela o fundo escuro

                l = juce::jlimit(0.0f, 1.0f, l);
                m = juce::jlimit(0.0f, 1.0f, m);
                h = juce::jlimit(0.0f, 1.0f, h);

                float totalAmp = juce::jlimit(0.0f, 1.0f, (l + m + h) * 0.6f);
                float hh = (totalAmp * height) * 0.45f;

                // 1. Efeito de Glow (Brilho suave)
                g.setColour(juce::Colour::fromFloatRGBA(l, m, h, 0.15f));
                g.drawVerticalLine(x, yCenter - hh * 1.2f, yCenter + hh * 1.2f);

                // 2. Cor Resultante (Composta) - Não é vermelho sólido
                // Se houver muito médio/agudo, a cor vira Amarelo/Ciano/Branco
                juce::Colour compositeColor = juce::Colour::fromFloatRGBA(l, m, h * hiAlphaMult, 1.0f);
                
                // 3. Desenho da Linha Fina (1.0px a 1.5px)
                g.setColour(juce::Colours::black); // Contorno para definição
                g.drawVerticalLine(x, yCenter - hh - 0.5f, yCenter + hh + 0.5f);
                
                g.setColour(compositeColor);
                g.drawVerticalLine(x, yCenter - hh, yCenter + hh);
            }
        }

        // CUE e Playhead
        if (cuePoint >= startTime && cuePoint <= endTime) {
            auto cueX = ((cuePoint - startTime) / duration) * width;
            g.setColour(juce::Colours::orange);
            g.fillRect((float)cueX - 1.0f, 0.0f, 3.0f, height);
        }

        auto playX = ((currentPos - startTime) / duration) * width;

        // Visual do Loop: Amarelo Translúcido
        if (loopActive && loopDuration > 0.0) {
            double lStart = std::max(startTime, loopStart);
            double lEnd = std::min(endTime, loopStart + loopDuration);
            if (lEnd > lStart) {
                float lx1 = (float)(((lStart - startTime) / duration) * width);
                float lx2 = (float)(((lEnd - startTime) / duration) * width);
                g.setColour(juce::Colours::yellow.withAlpha(0.2f));
                g.fillRect(lx1, 0.0f, lx2 - lx1, height);
                // Borda do loop
                g.setColour(juce::Colours::yellow.withAlpha(0.5f));
                g.drawRect(lx1, 0.0f, lx2 - lx1, height, 1.0f);
            }
        }

        g.setColour(juce::Colours::white);
        g.fillRect((float)playX - 1.0f, 0.0f, 2.0f, height);

        // Nome da Track (Posição segura no topo esquerdo)
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(juce::Font("Roboto", 15.0f, juce::Font::bold));
        g.drawText(loadedTrackName, 10, 5, (int)width - 60, 20, juce::Justification::left);
    }
    else if (isDraggingOver) {
        g.fillAll(juce::Colours::cyan.withAlpha(0.2f));
    }
}

void HeaderComponent::WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (loadedTrackName.isEmpty() || spectralData.empty())
        return;

    // Se estiver clicando nos botões de zoom, não faz seek
    if (zoomInBtn.getBounds().contains(e.getPosition()) || zoomOutBtn.getBounds().contains(e.getPosition()))
        return;

    double visibleDuration = (trackLength > 0) ? (trackLength / zoomFactor) : 0.0;
    double startTime = currentPos - (visibleDuration * 0.5);
    
    if (zoomFactor <= 1.01) startTime = 0;

    double clickRatio = (double)e.x / (double)getWidth();
    double seekPos = startTime + (clickRatio * visibleDuration);
    
    seekPos = juce::jlimit(0.0, trackLength, seekPos);
    
    if (onSeekToPosition)
        onSeekToPosition(seekPos);
}

// ---------------------------------------------------------------
// Layout
// ---------------------------------------------------------------

void HeaderComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour ((juce::uint32)0xff252525));
}

void HeaderComponent::resized()
{
    auto area = getLocalBounds().reduced(5);
    
    // Pitch Control (Tall Fader on the far right)
    auto pArea = area.removeFromRight(50);
    pitchLabel.setBounds(pArea.removeFromTop(15));
    pitchValue.setBounds(pArea.removeFromTop(18));
    pitchSlider.setBounds(pArea.reduced(5, 5));

    auto topArea = area.removeFromTop(75);
    
    // Waveform takes most of the remaining top
    waveformDisplay.setBounds(topArea.removeFromLeft(topArea.getWidth() - 100));

    // Record block on the right of top area - Smaller button
    auto recArea = topArea;
    recordButton.setBounds(recArea.removeFromTop(25).reduced(2));
    recordDuration.setBounds(recArea.removeFromTop(20));

    // Bottom area (controls row)
    auto bottomArea = area;
    
    // Master knob + value + label
    auto mastArea = bottomArea.removeFromLeft(70);
    masterValue.setBounds(mastArea.removeFromTop(14));
    masterKnob.setBounds(mastArea.removeFromTop(mastArea.getHeight() - 14));
    masterLabel.setBounds(mastArea);

    // Volume Track knob + value + label
    auto volArea = bottomArea.removeFromLeft(70);
    volumeValue.setBounds(volArea.removeFromTop(14));
    volumeKnob.setBounds(volArea.removeFromTop(volArea.getHeight() - 14));
    volumeLabel.setBounds(volArea);

    // Transport buttons: PLAY | STOP | CUE
    auto transportArea = bottomArea.removeFromLeft(165);
    auto btnH = transportArea.getHeight();
    playBtn.setBounds(transportArea.removeFromLeft(55).reduced(2).withHeight(btnH));
    stopBtn.setBounds(transportArea.removeFromLeft(55).reduced(2).withHeight(btnH));
    cueBtn.setBounds(transportArea.removeFromLeft(55).reduced(2).withHeight(btnH));

    // BPM Display
    bpmDisplay.setBounds(bottomArea.removeFromLeft(100).reduced(5));

    // VU Meter
    vuMeter.setBounds(bottomArea.removeFromLeft(120).reduced(2, 8));

    // Right side: Clock, Eject and Settings
    gearBtn.setBounds(bottomArea.removeFromRight(40).withSizeKeepingCentre(30, 30));
    lcdClock.setBounds(bottomArea.removeFromRight(100));
    
    // Ejetar ao lado de Quantize
    auto rightBtns = bottomArea.removeFromRight(120);
    ejectButton.setBounds(rightBtns.removeFromLeft(60).withSizeKeepingCentre(55, 30));
    quantToggle.setBounds(rightBtns.removeFromLeft(60).withSizeKeepingCentre(55, 30));

    // Loop Controls (entre BPM e VU)
    auto loopArea = bottomArea.removeFromLeft(120);
    loopBtn.setBounds(loopArea.removeFromLeft(50).withSizeKeepingCentre(45, 30));
    loopSizeCombo.setBounds(loopArea.reduced(2, 8));
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
            waveformDisplay.cuePoint = 0.0;
            waveformDisplay.generateRgbWaveform(f);
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
    return details.description.toString() == "AudioFileSelected";
}

void HeaderComponent::itemDropped(const juce::DragAndDropTarget::SourceDetails& details)
{
    waveformDisplay.isDraggingOver = false;
    if (auto* tree = dynamic_cast<juce::FileTreeComponent*>(details.sourceComponent.get()))
    {
        auto selectedFile = tree->getSelectedFile();
        if (selectedFile.existsAsFile())
        {
            waveformDisplay.loadedTrackName = selectedFile.getFileNameWithoutExtension();
            waveformDisplay.isPlaying = false;
            waveformDisplay.cuePoint = 0.0;
            waveformDisplay.generateRgbWaveform(selectedFile);
            if (onFileDropped) onFileDropped(selectedFile);
        }
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

void HeaderComponent::updateBpmDisplay(double bpm) { 
    currentBpm = bpm;
    bpmDisplay.setText("BPM: " + juce::String(bpm, 2), juce::dontSendNotification); 
}

void HeaderComponent::incrementPitch(float delta) {
    float newVal = juce::jlimit(-1.0f, 1.0f, (float)pitchSlider.getValue() + delta);
    pitchSlider.setValue(newVal, juce::sendNotification);
}
