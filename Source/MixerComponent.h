#pragma once
#include <JuceHeader.h>
#include "AudioCore.h"
#include "TrackBrowserComponent.h"
#include "VstControlWidget.h"

// --- Helper Components ---

class SimpleWaveform : public juce::Component, 
                      public juce::FileDragAndDropTarget,
                      private juce::ChangeListener, 
                      private juce::Timer {
public:
    SimpleWaveform(AudioCore& ac, int idx) : audioCore(ac), deckIdx(idx) {
        audioCore.getThumbnail(deckIdx).addChangeListener(this);
        setBufferedToImage(true);
        startTimer(16); // ~60 FPS
        
        addAndMakeVisible(zoomInBtn);
        addAndMakeVisible(zoomOutBtn);
        
        zoomInBtn.setButtonText("+");
        zoomOutBtn.setButtonText("-");
        
        auto setupZoomBtn = [](juce::TextButton& b) {
            b.setColour(juce::TextButton::buttonColourId, juce::Colours::black.withAlpha(0.6f));
            b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            b.setConnectedEdges(juce::Button::ConnectedOnBottom | juce::Button::ConnectedOnTop);
        };
        setupZoomBtn(zoomInBtn);
        setupZoomBtn(zoomOutBtn);
        
        zoomInBtn.onClick = [this] { zoomLevel = juce::jlimit(1.0, 32.0, zoomLevel * 0.8); repaint(); };
        zoomOutBtn.onClick = [this] { zoomLevel = juce::jlimit(1.0, 32.0, zoomLevel * 1.25); repaint(); };
    }
    ~SimpleWaveform() override { audioCore.getThumbnail(deckIdx).removeChangeListener(this); }
    
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray& files, int, int) override {
        if (files.size() > 0) {
            bool success = (deckIdx == 0) ? audioCore.loadDeckA(juce::File(files[0])) : audioCore.loadDeckB(juce::File(files[0]));
            if (success) repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        lastMouseX = e.x;
        isDragging = true;
        
        // Click to Seek
        double totalLen = audioCore.getDeckLength(deckIdx);
        if (totalLen > 0.0) {
            double pos = audioCore.getDeckPosition(deckIdx);
            double visibleSeconds = zoomLevel;
            float centerWeight = 0.5f;
            double startTime = pos - (visibleSeconds * centerWeight);
            
            double clickTime = startTime + ((double)e.x / getWidth()) * visibleSeconds;
            if (deckIdx == 0) audioCore.seekDeckA(clickTime);
            else if (deckIdx == 1) audioCore.seekDeckB(clickTime);
            else audioCore.seekHandsFreeDeck(clickTime);
            
            // Update lastMouseX to current so drag starts from here
            lastMouseX = e.x;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (isDragging) {
            int deltaX = e.x - lastMouseX;
            lastMouseX = e.x;
            
            double visibleSeconds = zoomLevel; // Use dynamic zoom level
            double secondsPerPixel = visibleSeconds / (double)getWidth();
            double deltaSeconds = (double)deltaX * secondsPerPixel;
            
            double currentPos = audioCore.getDeckPosition(deckIdx);
            double newPos = juce::jlimit(0.0, audioCore.getDeckLength(deckIdx), currentPos - deltaSeconds);
            
            if (deckIdx == 0) audioCore.seekDeckA(newPos);
            else audioCore.seekDeckB(newPos);
            
            if (!audioCore.isDeckPlaying(deckIdx)) {
                audioCore.setDeckCuePoint(deckIdx, newPos);
            }
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override {
        isDragging = false;
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        if (wheel.deltaY > 0) zoomLevel = juce::jlimit(1.0, 32.0, zoomLevel * 0.9);
        else if (wheel.deltaY < 0) zoomLevel = juce::jlimit(1.0, 32.0, zoomLevel * 1.1);
        repaint();
    }
    
    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds();
        g.setColour(juce::Colour(0xff0a0a0a));
        g.fillRoundedRectangle(area.toFloat(), 4.0f);

        auto& thumb = audioCore.getThumbnail(deckIdx);
        double totalLen = audioCore.getDeckLength(deckIdx);
        double pos      = audioCore.getDeckPosition(deckIdx);

        if (totalLen > 0.0) {
            double visibleSeconds = zoomLevel;
            // Original formula — allows negative startTime so the playhead
            // stays perfectly centred and JUCE AudioThumbnail draws empty
            // space for t < 0 (near track start). DO NOT clamp to 0.
            double startTime = pos - visibleSeconds * 0.5;
            double endTime   = startTime + visibleSeconds;

            // Helper: time → pixel X (same origin as startTime)
            auto tx = [&](double t) -> float {
                return (float)((t - startTime) / visibleSeconds) * (float)area.getWidth();
            };

            // ── 1. Beat / Bar / Phrase Grid ──────────────────────────────────
            double bpm = audioCore.getDeckBpm(deckIdx);
            if (bpm > 0.0) {
                double beat = 60.0 / bpm;
                double firstBeat = std::floor(startTime / beat) * beat;
                for (double t = firstBeat; t <= endTime; t += beat) {
                    float gx = tx(t);
                    if (gx < 0 || gx > area.getWidth()) continue;
                    int bn = (int)std::round(t / beat);
                    if (bn % 16 == 0) {
                        // Phrase line — bright cyan, 2 px wide
                        g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.55f));
                        g.fillRect(gx - 1.0f, 0.0f, 2.0f, (float)area.getHeight());
                    } else if (bn % 4 == 0) {
                        // Bar line — medium cyan
                        g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.28f));
                        g.drawVerticalLine((int)gx, 0.0f, (float)area.getHeight());
                    } else {
                        // Beat line — subtle white
                        g.setColour(juce::Colours::white.withAlpha(0.08f));
                        g.drawVerticalLine((int)gx, 0.0f, (float)area.getHeight());
                    }
                }
            }

            // ── 2. Waveform ───────────────────────────────────────────────────
            g.setColour(deckIdx == 0 ? juce::Colours::cyan.withAlpha(0.7f)
                                     : juce::Colours::tomato.withAlpha(0.7f));
            thumb.drawChannels(g, area.reduced(0, 5), startTime, endTime, 1.0f);

            // ── 3. Loop Region ────────────────────────────────────────────────
            if (audioCore.isDeckLoopEnabled(deckIdx)) {
                double loopStart = audioCore.getDeckLoopStart(deckIdx);
                double loopEnd   = loopStart + audioCore.getDeckLoopLength(deckIdx);
                if (loopEnd > startTime && loopStart < endTime) {
                    float lx = tx(std::max(loopStart, startTime));
                    float rx = tx(std::min(loopEnd,   endTime));
                    // Tinted fill
                    g.setColour(juce::Colours::yellow.withAlpha(0.22f));
                    g.fillRect(lx, 0.0f, rx - lx, (float)area.getHeight());
                    // Bracket vertical lines
                    g.setColour(juce::Colours::yellow.withAlpha(0.80f));
                    g.fillRect(lx,        0.0f, 2.0f, (float)area.getHeight());
                    g.fillRect(rx - 2.0f, 0.0f, 2.0f, (float)area.getHeight());
                    // Bracket corners (top & bottom)
                    g.fillRect(lx,         0.0f,                          10.0f, 3.0f);
                    g.fillRect(lx,         (float)area.getHeight() - 3.0f, 10.0f, 3.0f);
                    g.fillRect(rx - 10.0f, 0.0f,                          10.0f, 3.0f);
                    g.fillRect(rx - 10.0f, (float)area.getHeight() - 3.0f, 10.0f, 3.0f);
                }
            }

            // ── 4. CUE Point ──────────────────────────────────────────────────
            double cue = audioCore.getDeckCuePoint(deckIdx);
            if (cue >= startTime && cue <= endTime) {
                float cx = tx(cue);
                g.setColour(juce::Colours::orange);
                g.fillRect(cx - 1.0f, 0.0f, 2.0f, (float)area.getHeight());
                juce::Path tri;
                tri.addTriangle(cx - 6, 0, cx + 6, 0, cx, 8);
                g.fillPath(tri);
            }
            // NOTE: playhead line is drawn by MixerComponent::paintOverChildren()
            // at the exact visual centre — do NOT duplicate it here.

        } else {
            g.setColour(juce::Colours::grey.withAlpha(0.2f));
            g.drawRoundedRectangle(area.toFloat(), 4.0f, 1.0f);
            g.setFont(14.0f);
            g.drawText("DRAG TRACK HERE", area, juce::Justification::centred);
        }
    }
    void changeListenerCallback(juce::ChangeBroadcaster*) override { 
        juce::MessageManager::callAsync([this] { repaint(); }); 
    }
    void timerCallback() override { repaint(); }
    
    void resized() override {
        auto area = getLocalBounds();
        auto btnArea = area.removeFromRight(25).withSizeKeepingCentre(22, 50);
        zoomInBtn.setBounds(btnArea.removeFromTop(25).reduced(1));
        zoomOutBtn.setBounds(btnArea.reduced(1));
    }
private:
    AudioCore& audioCore;
    int deckIdx;
    int lastMouseX = 0;
    bool isDragging = false;
    double zoomLevel = 8.0;
    juce::TextButton zoomInBtn, zoomOutBtn;
};

class VUMeter : public juce::Component, private juce::Timer {
public:
    VUMeter(AudioCore& ac, int idx) : audioCore(ac), deckIdx(idx) { startTimer(30); }
    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().reduced(1);
        g.setColour(juce::Colours::black);
        g.fillRect(area);
        float level = audioCore.getDeckPeakLevel(deckIdx);
        int h = (int)(area.getHeight() * level);
        auto levelArea = area.withTop(area.getBottom() - h);
        g.setGradientFill(juce::ColourGradient(juce::Colours::green, 0, (float)area.getBottom(), juce::Colours::red, 0, (float)area.getY(), false));
        g.fillRect(levelArea);
    }
private:
    void timerCallback() override { repaint(); }
    AudioCore& audioCore;
    int deckIdx;
};

class MicControlWidget : public juce::Component, private juce::Timer {
public:
    struct MicLF : public juce::LookAndFeel_V4 {
        juce::Font getTextButtonFont(juce::TextButton&, int) override {
            return juce::Font(13.0f, juce::Font::bold); 
        }

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                             float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override {
            // Forçar área quadrada para evitar distorção (knob redondo)
            float side = (float)std::min(width, height);
            auto bounds = juce::Rectangle<float>(x + (width - side) * 0.5f, y + (height - side) * 0.5f, side, side).reduced(3);
            
            auto radius = bounds.getWidth() / 2.0f;
            auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
            auto center = bounds.getCentre();

            // 1. Contorno claro e bem visível
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawEllipse(bounds, 2.0f);

            // 2. Corpo do Knob mais claro para destaque
            g.setColour(juce::Colour(0xff333333));
            g.fillEllipse(bounds.reduced(1.5f));
            
            // Brilho no topo do knob
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.fillEllipse(bounds.reduced(4).translated(0, -2));

            // 3. Indicador (Tick) em branco puro
            g.setColour(juce::Colours::white);
            juce::Path p;
            p.addRoundedRectangle(-2.5f, -radius + 2, 5.0f, radius * 0.6f, 1.5f);
            p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center));
            g.fillPath(p);
        }
    };
    MicLF micLF;

    MicControlWidget(AudioCore& ac) : audioCore(ac) {
        micBtn.setButtonText("MIC");
        micBtn.setClickingTogglesState(true);
        micBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222222));
        micBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
        micBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        micBtn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        micBtn.setLookAndFeel(&micLF);
        micBtn.onClick = [this] { audioCore.setMicEnabled(micBtn.getToggleState()); };
        addAndMakeVisible(micBtn);

        micKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        micKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        micKnob.setLookAndFeel(&micLF);
        micKnob.setRange(0.0, 1.0, 0.01);
        micKnob.setValue(audioCore.getMicVolume());
        micKnob.onValueChange = [this] { audioCore.setMicVolume((float)micKnob.getValue()); };
        addAndMakeVisible(micKnob);

        startTimer(30);
    }

    ~MicControlWidget() override {
        micBtn.setLookAndFeel(nullptr);
        micKnob.setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds();
        
        // Pequeno fundo para agrupar
        g.setColour(juce::Colour(0xff151515));
        g.fillRoundedRectangle(area.toFloat(), 6.0f);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawRoundedRectangle(area.toFloat(), 6.0f, 1.0f);

        // VU Meter horizontal
        auto vuArea = juce::Rectangle<int>(micBtn.getRight() + 5, 8, area.getWidth() - micBtn.getRight() - 15, 8);
        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(vuArea.toFloat(), 2.0f);
        
        if (audioCore.isMicEnabled()) {
            float level = audioCore.getInputLevel();
            int w = (int)(vuArea.getWidth() * level);
            g.setColour(juce::Colours::lime);
            g.fillRoundedRectangle(vuArea.withWidth(w).toFloat(), 2.0f);
        }
        
        g.setColour(juce::Colours::grey);
        g.setFont(10.0f);
        // Removido o texto "MIC" debaixo pois agora está dentro do botão
    }

    void resized() override {
        auto area = getLocalBounds().reduced(8);
        micBtn.setBounds(area.removeFromLeft(50).withSizeKeepingCentre(44, 44).withY(area.getY() + 10));
        
        auto rightSide = area;
        rightSide.removeFromTop(20); 
        // Forçar o knob a ser um quadrado centralizado para não distorcer
        int side = std::min(rightSide.getWidth(), rightSide.getHeight());
        micKnob.setBounds(rightSide.withSizeKeepingCentre(side, side));
    }

    void timerCallback() override {
        micBtn.setToggleState(audioCore.isMicEnabled(), juce::dontSendNotification);
        repaint();
    }

private:
    AudioCore& audioCore;
    juce::TextButton micBtn;
    juce::Slider micKnob;
};

class JogWheel : public juce::Component, private juce::Timer {
public:
    JogWheel(AudioCore& ac, int idx, juce::Colour accent, bool showRemaining) 
        : audioCore(ac), deckIdx(idx), accentColour(accent), remainingMode(showRemaining) { startTimer(16); }
    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().reduced(2).toFloat();
        float diameter = std::min(area.getWidth(), area.getHeight());
        auto circleArea = area.withSizeKeepingCentre(diameter, diameter);
        float radius = diameter / 2.0f;
        auto center = circleArea.getCentre();
        g.setColour(juce::Colour(0xff1a1a1a)); g.fillEllipse(circleArea);
        double pos = audioCore.getDeckPosition(deckIdx);
        double duration = audioCore.getDeckLength(deckIdx);
        if (duration > 0) {
            float angle = (float)(pos / duration) * juce::MathConstants<float>::twoPi;
            juce::Path p; p.addCentredArc(center.x, center.y, radius - 4, radius - 4, 0, 0, angle, true);
            g.setColour(accentColour); g.strokePath(p, juce::PathStrokeType(4.0f));
        }
        g.setColour(juce::Colour(0xff050505));
        g.fillEllipse(center.x - radius * 0.7f, center.y - radius * 0.7f, radius * 1.4f, radius * 1.4f);
        double displayTime = remainingMode ? std::max(0.0, duration - pos) : pos;
        int mins = (int)(displayTime / 60); int secs = (int)(displayTime) % 60;
        juce::String timeStr = juce::String::formatted("%02d:%02d", mins, secs);
        g.setColour(juce::Colours::white); g.setFont(juce::Font(radius * 0.45f, juce::Font::bold));
        g.drawText(timeStr, circleArea.withHeight(radius * 0.8f).withY(center.y - radius * 0.35f), juce::Justification::centred);
    }
private:
    void timerCallback() override { repaint(); }
    AudioCore& audioCore; int deckIdx; juce::Colour accentColour; bool remainingMode;
};

class LoopControlGroup : public juce::Component {
public:
    LoopControlGroup(juce::Colour accent) : accentColour(accent) {
        addAndMakeVisible(title); title.setText("LOOP", juce::dontSendNotification);
        title.setFont(juce::Font(10.0f, juce::Font::bold));
        addAndMakeVisible(beats); beats.setText("4 BEATS", juce::dontSendNotification);
        beats.setFont(juce::Font(9.0f, juce::Font::bold)); beats.setColour(juce::Label::textColourId, accentColour);
        setupBtn(inBtn, "IN"); setupBtn(outBtn, "OUT"); setupBtn(autoBtn, "AUTO LOOP");
        
        inBtn.onClick = [this] { if (onInPressed) onInPressed(); };
        outBtn.onClick = [this] { if (onOutPressed) onOutPressed(); };
        autoBtn.onClick = [this] { if (onAutoPressed) onAutoPressed(); };
    }
    void setupBtn(juce::TextButton& b, const juce::String& t) {
        addAndMakeVisible(b); b.setButtonText(t); b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff111111));
    }
    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        g.setColour(accentColour.withAlpha(0.4f)); g.drawRoundedRectangle(area, 6.0f, 1.5f);
    }
    void resized() override {
        auto area = getLocalBounds().reduced(6); auto header = area.removeFromTop(15);
        title.setBounds(header.removeFromLeft(header.getWidth() / 2)); beats.setBounds(header);
        autoBtn.setBounds(area.removeFromBottom(22));
        int btnW = area.reduced(0, 5).getWidth() / 2;
        inBtn.setBounds(area.removeFromLeft(btnW).reduced(1, 4)); outBtn.setBounds(area.reduced(1, 4));
    }
public:
    void setBeats(double b) { 
        if (b < 1.0) {
            int denom = (int)(1.0 / b + 0.5);
            beats.setText("1/" + juce::String(denom) + " BEAT", juce::dontSendNotification);
        } else {
            beats.setText(juce::String((int)b) + " BEATS", juce::dontSendNotification);
        }
    }
    std::function<void()> onInPressed;
    std::function<void()> onOutPressed;
    std::function<void()> onAutoPressed;
    juce::TextButton inBtn, outBtn, autoBtn;

private:
    juce::Colour accentColour; juce::Label title, beats; 
};

class DeckSection : public juce::Component, private juce::Timer {
public:
    DeckSection(AudioCore& ac, int idx, bool isLeft) 
        : audioCore(ac), deckIdx(idx), leftSide(isLeft), jog(ac, idx, isLeft ? juce::Colours::cyan : juce::Colours::tomato, isLeft),
          vu(ac, idx), loopControls(isLeft ? juce::Colours::cyan : juce::Colours::tomato) {
        addAndMakeVisible(deckLabel); deckLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        addAndMakeVisible(badge); 
        badge.onClick = [this] { audioCore.setMasterDeck(deckIdx); };
        addAndMakeVisible(bpmLabel); bpmLabel.setFont(juce::Font(28.0f, juce::Font::bold));
        bpmLabel.setColour(juce::Label::textColourId, leftSide ? juce::Colours::cyan : juce::Colours::red);
        addAndMakeVisible(jog); addAndMakeVisible(vu); addAndMakeVisible(loopControls);
        addAndMakeVisible(pitchSlider); pitchSlider.setSliderStyle(juce::Slider::LinearVertical);
        pitchSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pitchSlider.setRange(-1.0, 1.0, 0.001);
        pitchSlider.setValue(0.0);
        pitchSlider.onValueChange = [this] {
            audioCore.setDeckPitch(deckIdx, pitchSlider.getValue());
        };
        setupButton(playBtn, "PLAY", leftSide ? juce::Colours::blue : juce::Colours::red);
        
        playBtn.onClick = [this] { 
            if (audioCore.isDeckPlaying(deckIdx)) {
                if (deckIdx == 0) audioCore.stopDeckA(); 
                else audioCore.stopDeckB(); 
            } else {
                // Resume exactly from CUE point as requested
                audioCore.triggerDeckCue(deckIdx); 
                if (deckIdx == 0) audioCore.playDeckA(); 
                else audioCore.playDeckB(); 
            }
        };

        setupButton(cueBtn, "CUE", juce::Colour(0xff222222));
        cueBtn.onClick = [this] { 
            if (audioCore.isDeckLoopEnabled(deckIdx)) {
                // If loop is active, trigger CUE (go back to loop start)
                audioCore.triggerDeckCue(deckIdx);
            } else {
                // If loop is inactive, update CUE to current playhead position
                audioCore.setDeckCuePoint(deckIdx, audioCore.getDeckPosition(deckIdx));
            }
        };

        setupButton(syncBtn, "SYNC", juce::Colour(0xff222222)); 
        syncBtn.onClick = [this] { 
            audioCore.setSyncEnabled(deckIdx, !audioCore.isSyncEnabled(deckIdx)); 
        };
        setupButton(revBtn, "REV", juce::Colour(0xff222222));
        
        setupButton(ejectBtn, juce::String::fromUTF8("\xe2\x96\xb2"), juce::Colour(0xff151515)); 
        ejectBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
        ejectBtn.onClick = [this] { 
            if (deckIdx == 0) audioCore.ejectDeckA(); 
            else audioCore.ejectDeckB(); 
            repaint();
        };

        auto updateLoopWithCurrentBeats = [this](bool activate) {
            double bpm = audioCore.getDeckBpm(deckIdx);
            if (bpm <= 0) bpm = 120.0;
            
            double beatOptions[] = { 0.03125, 0.0625, 0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32 };
            double numBeats = beatOptions[currentBeatIdx];
            double duration = (60.0 / bpm) * numBeats;
            
            double startPos = audioCore.isDeckLoopEnabled(deckIdx) ? 
                              audioCore.getDeckLoopStart(deckIdx) : 
                              audioCore.getDeckPosition(deckIdx);
            
            audioCore.setDeckLoopRange(deckIdx, startPos, duration);
            if (activate) audioCore.setDeckLoopEnabled(deckIdx, true);
            
            audioCore.setDeckCuePoint(deckIdx, startPos);
            loopControls.setBeats(numBeats);
        };

        loopControls.onInPressed = [this, updateLoopWithCurrentBeats] {
            if (currentBeatIdx > 0) {
                currentBeatIdx--;
                updateLoopWithCurrentBeats(true);
            }
        };

        loopControls.onOutPressed = [this, updateLoopWithCurrentBeats] {
            if (currentBeatIdx < 10) { // 0.03125 to 32 is 11 options (0 to 10)
                currentBeatIdx++;
                updateLoopWithCurrentBeats(true);
            }
        };

        loopControls.onAutoPressed = [this, updateLoopWithCurrentBeats] {
            bool isLooping = audioCore.isDeckLoopEnabled(deckIdx);
            if (isLooping) {
                audioCore.setDeckLoopEnabled(deckIdx, false);
            } else {
                currentBeatIdx = 7; // Start at 4 beats (Index 7: 0.03,0.06,0.12,0.25,0.5,1,2,4...)
                // Wait, let's recount: 0:1/32, 1:1/16, 2:1/8, 3:1/4, 4:1/2, 5:1, 6:2, 7:4, 8:8, 9:16, 10:32
                updateLoopWithCurrentBeats(true);
            }
        };

        startTimer(16);
    }

    void setupButton(juce::TextButton& b, const juce::String& t, juce::Colour c) { addAndMakeVisible(b); b.setButtonText(t); b.setColour(juce::TextButton::buttonColourId, c); }
    void paint(juce::Graphics& g) override { g.setColour(juce::Colour(0xff151515)); g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f); }
    void resized() override {
        auto area = getLocalBounds().reduced(15); auto header = area.removeFromTop(40);
        if (leftSide) { badge.setBounds(header.removeFromLeft(70).removeFromTop(20)); deckLabel.setBounds(header.removeFromLeft(100)); bpmLabel.setBounds(header.removeFromRight(100)); }
        else { badge.setBounds(header.removeFromRight(70).removeFromTop(20)); deckLabel.setBounds(header.removeFromRight(100)); bpmLabel.setBounds(header.removeFromLeft(100)); }
        auto footer = area.removeFromBottom(45); int btnW = footer.getWidth() / 4 - 5;
        playBtn.setBounds(footer.removeFromLeft(btnW)); footer.removeFromLeft(5); cueBtn.setBounds(footer.removeFromLeft(btnW)); footer.removeFromLeft(5);
        syncBtn.setBounds(footer.removeFromLeft(btnW)); footer.removeFromLeft(5); revBtn.setBounds(footer);
        
        // Position EJECT button discretely below BPM
        auto ejectArea = leftSide ? getLocalBounds().withWidth(100).withX(getWidth() - 110) : getLocalBounds().withWidth(100).withX(10);
        ejectArea = ejectArea.withHeight(20).withY(58); // Moved down for better spacing
        ejectBtn.setBounds(ejectArea.withSizeKeepingCentre(24, 20));
        if (leftSide) { pitchSlider.setBounds(area.removeFromLeft(35).reduced(5, 10)); loopControls.setBounds(area.removeFromLeft(110).withHeight(85).withY(pitchSlider.getBottom() - 85)); vu.setBounds(area.removeFromRight(12).reduced(0, 20)); }
        else { pitchSlider.setBounds(area.removeFromRight(35).reduced(5, 10)); loopControls.setBounds(area.removeFromRight(110).withHeight(85).withY(pitchSlider.getBottom() - 85)); vu.setBounds(area.removeFromLeft(12).reduced(0, 20)); }
        jog.setBounds(area.reduced(10));
    }

    void timerCallback() override {
        bpmLabel.setText(juce::String(audioCore.getDeckBpm(deckIdx), 1), juce::dontSendNotification);
        deckLabel.setText(audioCore.getDeckName(deckIdx).isEmpty() ? (deckIdx == 0 ? "DECK A" : "DECK B") : audioCore.getDeckName(deckIdx), juce::dontSendNotification);
        bool isMaster = (audioCore.getMasterDeck() == deckIdx);
        badge.setButtonText(isMaster ? "MASTER" : "SLAVE");
        badge.setColour(juce::TextButton::buttonColourId, isMaster ? juce::Colours::blue : juce::Colours::white);
        badge.setColour(juce::TextButton::textColourOffId, isMaster ? juce::Colours::white : juce::Colours::black);
        badge.setColour(juce::TextButton::textColourOnId, isMaster ? juce::Colours::white : juce::Colours::black);
        bool playing = audioCore.isDeckPlaying(deckIdx); playBtn.setButtonText(playing ? "PAUSE" : "PLAY");
        playBtn.setColour(juce::TextButton::buttonColourId, playing ? juce::Colours::green : (leftSide ? juce::Colours::blue : juce::Colours::red));
        
        bool isLooping = audioCore.isDeckLoopEnabled(deckIdx);
        loopControls.autoBtn.setToggleState(isLooping, juce::dontSendNotification);
        loopControls.autoBtn.setColour(juce::TextButton::buttonColourId, isLooping ? juce::Colours::orange : juce::Colour(0xff111111));

        bool sync = audioCore.isSyncEnabled(deckIdx);
        syncBtn.setColour(juce::TextButton::buttonColourId, sync ? juce::Colours::white : juce::Colour(0xff222222));
        syncBtn.setColour(juce::TextButton::textColourOffId, sync ? juce::Colour(0xffcccc00) : juce::Colours::white);
    }
    AudioCore& audioCore; int deckIdx; bool leftSide; juce::Label deckLabel, bpmLabel; juce::TextButton badge; JogWheel jog; VUMeter vu; juce::Slider pitchSlider; juce::TextButton playBtn, cueBtn, syncBtn, revBtn, ejectBtn; LoopControlGroup loopControls;
    double loopInPoint = 0.0;
    int currentBeatIdx = 7; // Default 4 beats (Index 7)
};

class MixerCenterSection : public juce::Component {
public:
    MixerCenterSection(AudioCore& ac) 
        : audioCore(ac), 
          fxA(ac, 0), fxB(ac, 1),
          micControl(ac),
          vstControl(ac.getVocalVstChain(), ac.getVstManager())
    {
        setLookAndFeel(&lookAndFeel);
        
        addAndMakeVisible(content);
        content.addAndMakeVisible(fxA);
        content.addAndMakeVisible(fxB);

        content.addAndMakeVisible(micControl);
        content.addAndMakeVisible(vstControl);


        // Side Knobs
        setupKnobCol(knobsA, 0);
        setupKnobCol(knobsB, 1);

        // Faders
        setupFader(faderA, 0);
        setupFader(faderB, 1);

        // Cue Buttons
        setupCue(cueA, 0);
        setupCue(cueB, 1);

        content.addAndMakeVisible(crossfader);
        crossfader.setSliderStyle(juce::Slider::LinearHorizontal);
        crossfader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        crossfader.onValueChange = [this] { audioCore.setCrossfaderPosition((float)crossfader.getValue()); };
        crossfader.setRange(0.0, 1.0);
        crossfader.setValue(0.5);
    }

    ~MixerCenterSection() override { setLookAndFeel(nullptr); }

    struct SimpleXY : public juce::Component {
        std::function<void(float, float)> onPointChanged;
        std::function<void(bool)> onEnableChanged;
        float curX = 0.5f, curY = 0.5f;
        bool active = false;
        void paint(juce::Graphics& g) override {
            auto area = getLocalBounds();
            
            // 1. Dark Background
            g.setColour(juce::Colour(0xff0a0a0a));
            g.fillRoundedRectangle(area.toFloat(), 6.0f);

            // 2. Header
            auto headerArea = area.removeFromTop(40);
            g.setColour(juce::Colour(0xff222222));
            g.fillRoundedRectangle(headerArea.withSizeKeepingCentre(160, 24).toFloat(), 12.0f);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("DJ FILTER", headerArea, juce::Justification::centred);

            int cx = area.getX() + area.getWidth()  / 2;
            int cy = area.getY() + area.getHeight() / 2;

            // 3. Quadrant tinted backgrounds
            auto tl = area.withRight(cx).withBottom(cy);
            auto tr = area.withLeft(cx).withBottom(cy);
            auto bl = area.withRight(cx).withTop(cy);
            auto br = area.withLeft(cx).withTop(cy);

            g.setColour(juce::Colour(0xff003322).withAlpha(0.55f)); g.fillRect(tl); // Top-Left: cyan-green (Flanger)
            g.setColour(juce::Colour(0xff002233).withAlpha(0.55f)); g.fillRect(tr); // Top-Right: blue (Echo)
            g.setColour(juce::Colour(0xff220033).withAlpha(0.45f)); g.fillRect(bl); // Bot-Left: purple (LP+Res)
            g.setColour(juce::Colour(0xff330022).withAlpha(0.45f)); g.fillRect(br); // Bot-Right: red (HP+Res)

            // 4. Grid lines
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            int gridStep = 40;
            for (int x = area.getX() + gridStep; x < area.getRight(); x += gridStep) g.drawVerticalLine(x, (float)area.getY(), (float)area.getBottom());
            for (int y = area.getY() + gridStep; y < area.getBottom(); y += gridStep) g.drawHorizontalLine(y, (float)area.getX(), (float)area.getRight());

            // 5. Center crosshair
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.drawVerticalLine(cx, (float)area.getY(), (float)area.getBottom());
            g.drawHorizontalLine(cy, (float)area.getX(), (float)area.getRight());

            // 6. Quadrant labels (corners)
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.setColour(juce::Colours::lime.withAlpha(0.5f));
            g.drawText("LP+FLANGER", tl.reduced(4, 2), juce::Justification::topLeft);
            g.setColour(juce::Colours::cyan.withAlpha(0.5f));
            g.drawText("HP+ECHO", tr.reduced(4, 2), juce::Justification::topRight);
            g.setColour(juce::Colour(0xffaa88ff).withAlpha(0.5f));
            g.drawText("LP+RESONANCE", bl.reduced(4, 2), juce::Justification::bottomLeft);
            g.setColour(juce::Colour(0xffff88aa).withAlpha(0.5f));
            g.drawText("HP+RESONANCE", br.reduced(4, 2), juce::Justification::bottomRight);

            // 7. Glowing ball indicator
            float px = curX * area.getWidth()  + area.getX();
            float py = (1.0f - curY) * area.getHeight() + area.getY();
            
            juce::Colour lime = juce::Colours::lime;
            if (active) {
                for (int i = 5; i > 0; --i) {
                    float r = 12.0f + i * 8.0f;
                    g.setColour(lime.withAlpha(0.1f * (6-i)));
                    g.fillEllipse(px - r, py - r, r * 2, r * 2);
                }
            }
            float ballR = active ? 10.0f : 4.0f;
            g.setColour(active ? lime : lime.withAlpha(0.15f));
            g.fillEllipse(px - ballR, py - ballR, ballR * 2, ballR * 2);
        }
        void mouseDown(const juce::MouseEvent& e) override {
            active = true;
            // Apply effect from the exact touch position immediately, without passing through center
            handleMouse(e);
            if (onEnableChanged) onEnableChanged(true);
        }
        void mouseDrag(const juce::MouseEvent& e) override { handleMouse(e); }
        void mouseUp(const juce::MouseEvent&) override {
            active = false;
            // Disable the filter FIRST (stop the effect), then silently reset ball to center
            if (onEnableChanged) onEnableChanged(false);
            // Reset ball position visually without sending any filter value (no zero-crossing cut)
            curX = 0.5f;
            curY = 0.5f;
            repaint();
        }
        void handleMouse(const juce::MouseEvent& e) {
            auto area = getLocalBounds();
            area.removeFromTop(40);
            curX = juce::jlimit(0.0f, 1.0f, (float)e.x / area.getWidth());
            curY = juce::jlimit(0.0f, 1.0f, 1.0f - (float)(e.y - 40) / area.getHeight());
            if (onPointChanged) onPointChanged(curX, curY);
            repaint();
        }
    };

    void paint(juce::Graphics& g) override {
        auto area = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff0d0d0d));
        g.fillRoundedRectangle(area, 12.0f);
        

        // Draw fader scales
        g.setColour(juce::Colours::grey.withAlpha(0.5f));
        g.setFont(10.0f);
        auto drawScale = [&](juce::Rectangle<int> r) {
            g.drawText("+10", r.getX() - 25, r.getY(), 20, 15, juce::Justification::right);
            g.drawText("-inf", r.getX() - 25, r.getBottom() - 15, 20, 15, juce::Justification::right);
        };
        drawScale(faderA.getBounds());
        drawScale(faderB.getBounds());
    }

    void resized() override {
        auto area = getLocalBounds();
        
        // 1200x800 is the reference size for 'content'
        float refW = 1200.0f;
        float refH = 800.0f;
        
        float scaleX = (float)area.getWidth() / refW;
        float scaleY = (float)area.getHeight() / refH;
        float scale = std::min(scaleX, scaleY);
        
        content.setBounds(0, 0, (int)refW, (int)refH);
        content.setTransform(juce::AffineTransform::scale(scale));
        content.setCentrePosition(area.getCentre());

        // --- Actual Layout logic inside content ---
        int w = (int)refW, h = (int)refH;
        

        int sideW = 80;
        int faderW = 60;
        int gap = 15;
        int fxW = (w - (sideW * 2 + faderW * 2 + gap * 5)) / 2;
        int mainY = 30; // Moved up from 120
        int mainH = 650; // Increased from 550

        int curX = (w - (sideW * 2 + faderW * 2 + fxW * 2 + gap * 5)) / 2;
        
        // --- SYMMETRICAL LAYOUT ---
        // Left Column (Side Knobs A)
        layoutKnobCol(knobsA, curX, mainY, sideW, mainH); 
        curX += sideW + gap;

        // Left Fader A + Cue A
        faderA.setBounds(curX, mainY + 50, faderW, mainH - 150);
        cueA.setBounds(curX - 10, mainY + mainH - 70, faderW + 20, 35);
        curX += faderW + gap;

        // FX Panel A
        fxA.setBounds(curX, mainY, fxW, mainH); 
        curX += fxW + gap;

        // FX Panel B
        fxB.setBounds(curX, mainY, fxW, mainH); 
        curX += fxW + gap;

        // Right Fader B + Cue B
        faderB.setBounds(curX, mainY + 50, faderW, mainH - 150);
        cueB.setBounds(curX - 10, mainY + mainH - 70, faderW + 20, 35);
        curX += faderW + gap;

        // Right Column (Side Knobs B)
        layoutKnobCol(knobsB, curX, mainY, sideW, mainH);

        crossfader.setBounds((w - 400) / 2, h - 80, 400, 50);

        // Posicionar controles de Mic e VST mais à ESQUERDA (no espaço indicado no print)
        int cfY = h - 85;
        
        vstControl.setBounds(40, cfY, 95, 75);
        micControl.setBounds(145, cfY, 230, 85);
    }

private:
    struct CustomLookAndFeel : public juce::LookAndFeel_V4 {
        CustomLookAndFeel() {
            setColour(juce::Slider::thumbColourId, juce::Colours::cyan);
            setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::cyan);
            setColour(juce::Slider::trackColourId, juce::Colour(0xff222222));
        }
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                             float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override {
            float side = (float)std::min(width, height);
            auto bounds = juce::Rectangle<float>(x + (width - side) * 0.5f, y + (height - side) * 0.5f, side, side).reduced(3);
            
            auto radius = bounds.getWidth() / 2.0f;
            auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
            auto center = bounds.getCentre();

            // 1. Contorno claro e bem visível
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawEllipse(bounds, 1.5f);

            // 2. Corpo do Knob mais claro
            g.setColour(juce::Colour(0xff333333));
            g.fillEllipse(bounds.reduced(1.0f));

            // 3. Indicador (Tick) em branco puro
            g.setColour(juce::Colours::white);
            juce::Path p;
            p.addRoundedRectangle(-2.0f, -radius + 2, 4.0f, radius * 0.5f, 1.0f);
            p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center));
            g.fillPath(p);
        }

        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPos, float minSliderPos, float maxSliderPos,
                             const juce::Slider::SliderStyle style, juce::Slider& slider) override {
            bool isVertical = (style == juce::Slider::LinearVertical);

            // ── Track ────────────────────────────────────────────────────────
            auto trackArea = isVertical ?
                juce::Rectangle<float>((float)x + width * 0.45f, (float)y, width * 0.1f, (float)height) :
                juce::Rectangle<float>((float)x, (float)y + height * 0.45f, (float)width, height * 0.1f);

            if (!isVertical) {
                // Crossfader: layered glowing track
                float ty = trackArea.getY();
                float th = trackArea.getHeight();
                float tx = trackArea.getX();
                float tw = trackArea.getWidth();

                // 1. Outer soft glow halo
                juce::ColourGradient halo(
                    juce::Colour(0xff00e5ff).withAlpha(0.08f), tx, ty + th * 0.5f,
                    juce::Colours::transparentBlack,           tx, ty - th * 2.5f,
                    false);
                g.setGradientFill(halo);
                g.fillRoundedRectangle(tx - 2, ty - 3, tw + 4, th + 6, 4.0f);

                // 2. Dark base track
                g.setColour(juce::Colour(0xff111418));
                g.fillRoundedRectangle(trackArea, 3.0f);

                // 3. Inner highlight line (top edge shimmer)
                juce::ColourGradient shimmer(
                    juce::Colours::white.withAlpha(0.10f), tx,        ty,
                    juce::Colours::transparentBlack,        tx + tw * 0.5f, ty,
                    false);
                g.setGradientFill(shimmer);
                g.fillRoundedRectangle(tx, ty, tw * 0.5f, th * 0.4f, 2.0f);

                // 4. Fader position — left tint (A side, cyan)
                float fillW = sliderPos - tx;
                if (fillW > 2.0f) {
                    juce::ColourGradient fillA(
                        juce::Colour(0xff00e5ff).withAlpha(0.55f), tx, ty,
                        juce::Colours::transparentBlack,             tx + fillW, ty,
                        false);
                    g.setGradientFill(fillA);
                    g.fillRoundedRectangle(tx + 1, ty + 1, fillW - 2, th - 2, 2.0f);
                }

                // 5. Right tint (B side, warm white/magenta)
                float rightStart = sliderPos;
                float fillRight = (tx + tw) - rightStart;
                if (fillRight > 2.0f) {
                    juce::ColourGradient fillB(
                        juce::Colours::transparentBlack,                    rightStart, ty,
                        juce::Colour(0xffff44aa).withAlpha(0.45f), tx + tw,  ty,
                        false);
                    g.setGradientFill(fillB);
                    g.fillRoundedRectangle(rightStart, ty + 1, fillRight - 1, th - 2, 2.0f);
                }

                // 6. Center reference tick
                float cx = tx + tw * 0.5f;
                g.setColour(juce::Colours::white.withAlpha(0.35f));
                g.fillRect(cx - 0.75f, ty - 3.0f, 1.5f, th + 6.0f);

                // 7. Track border
                g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.18f));
                g.drawRoundedRectangle(trackArea, 3.0f, 0.8f);

            } else {
                // Vertical faders: keep original clean style
                g.setColour(juce::Colour(0xff050505));
                g.fillRoundedRectangle(trackArea, 2.0f);
            }

            // ── Thumb cap ────────────────────────────────────────────────────
            float thumbW = isVertical ? (float)width * 0.85f : 45.0f;
            float thumbH = isVertical ? 25.0f : (float)height * 0.85f;
            float thumbX = isVertical ? (float)x + (width - thumbW) * 0.5f : sliderPos - thumbW * 0.5f;
            float thumbY = isVertical ? sliderPos - thumbH * 0.5f : (float)y + (height - thumbH) * 0.5f;

            juce::Rectangle<float> thumb(thumbX, thumbY, thumbW, thumbH);

            // Subtle outer glow on thumb
            if (!isVertical) {
                g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.20f));
                g.fillRoundedRectangle(thumb.expanded(3.0f, 2.0f), 5.0f);
            }

            // Main Cap Body (Dark Gradient)
            juce::ColourGradient grad(juce::Colour(0xff2e3035), thumbX, thumbY,
                                      juce::Colour(0xff1a1c1f), thumbX, thumbY + thumbH, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(thumb, 3.0f);

            // Cap Outline — cyan tint for crossfader, plain for faders
            g.setColour(isVertical ? juce::Colour(0xff333333) : juce::Colour(0xff00e5ff).withAlpha(0.5f));
            g.drawRoundedRectangle(thumb, 3.0f, 1.0f);

            // Central Cyan Line (marker)
            g.setColour(juce::Colours::cyan);
            if (isVertical) {
                g.fillRect(thumbX + 2, thumbY + thumbH * 0.5f - 1.0f, thumbW - 4, 2.0f);
                g.setColour(juce::Colours::cyan.withAlpha(0.3f));
                g.fillRect(thumbX + 2, thumbY + thumbH * 0.5f - 2.0f, thumbW - 4, 4.0f);
            } else {
                g.fillRect(thumbX + thumbW * 0.5f - 1.0f, thumbY + 2, 2.0f, thumbH - 4);
                g.setColour(juce::Colours::cyan.withAlpha(0.4f));
                g.fillRect(thumbX + thumbW * 0.5f - 2.5f, thumbY + 2, 5.0f, thumbH - 4);
            }
        }
    };

    struct FxKnobLookAndFeel : public CustomLookAndFeel {
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                             float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override {
            float side = (float)std::min(width, height);
            auto bounds = juce::Rectangle<float>(x + (width - side) * 0.5f, y + (height - side) * 0.5f, side, side).reduced(3);
            auto radius = bounds.getWidth() / 2.0f;
            auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
            auto center = bounds.getCentre();
            
            juce::Colour neonColor = juce::Colour(0xffff00ff); 
            bool isActive = slider.getProperties()["isActive"];

            // 1. Contorno claro
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawEllipse(bounds, 1.5f);

            // 2. Corpo do Knob
            g.setColour(juce::Colour(0xff333333));
            g.fillEllipse(bounds.reduced(1.0f));
            
            // Value Arc (Neon)
            if (isActive) {
                float lineW = 3.0f;
                float arcRadius = radius - 1.5f;
                g.setColour(neonColor.withAlpha(0.8f));
                juce::Path valueArc;
                valueArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
                g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // 3. Indicador (Tick)
            g.setColour(juce::Colours::white);
            juce::Path p;
            p.addRoundedRectangle(-2.0f, -radius + 2, 4.0f, radius * 0.5f, 1.0f);
            p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center.x, center.y));
            g.fillPath(p);
        }
    };

    struct ToggleSlider : public juce::Slider {
        std::function<void(bool)> onToggle;
        void mouseUp(const juce::MouseEvent& e) override {
            juce::Slider::mouseUp(e);
            if (e.getDistanceFromDragStart() < 3) {
                bool newState = !getProperties()["isActive"];
                getProperties().set("isActive", newState);
                if (onToggle) onToggle(newState);
                repaint();
            }
        }
    };

    struct FxPanel : public juce::Component {
        int deckIdx;
        AudioCore& audioCore;
        int activeTab = 0;
        
        struct FxSlot : public juce::Component {
            ToggleSlider knob;
            juce::Label label;
            FxKnobLookAndFeel fxLF;
            
            FxSlot(const juce::String& name) {
                addAndMakeVisible(knob); 
                knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                knob.setLookAndFeel(&fxLF);
                knob.getProperties().set("isActive", false);

                addAndMakeVisible(label); 
                label.setText(name, juce::dontSendNotification);
                label.setFont(juce::Font(18.0f, juce::Font::bold));
                label.setJustificationType(juce::Justification::centred);
                label.setColour(juce::Label::textColourId, juce::Colours::white);
            }
            ~FxSlot() { knob.setLookAndFeel(nullptr); }

            void resized() override {
                auto area = getLocalBounds();
                label.setBounds(area.removeFromBottom(28));
                // Force a perfect square so the rotary circle is always symmetric
                int side = std::min(area.getWidth(), area.getHeight()) - 16;
                side = std::max(side, 10); // minimum safety
                knob.setBounds(area.withSizeKeepingCentre(side, side));
            }
        };

        juce::OwnedArray<FxSlot> slots;
        MixerCenterSection::SimpleXY xyPad;

        FxPanel(AudioCore& ac, int idx) : deckIdx(idx), audioCore(ac) {
            static const char* fxNames[] = { "DELAY", "ECHO", "REVERB", "FLANGE", "SPACE", "DUB ECHO" };
            for (int i = 0; i < 6; ++i) {
                auto* s = slots.add(new FxSlot(fxNames[i]));
                addAndMakeVisible(s);
                s->knob.onValueChange = [this, i, s] { audioCore.setFxAmount(deckIdx, i, (float)s->knob.getValue()); };
                s->knob.onToggle = [this, i](bool active) { audioCore.setFxEnabled(deckIdx, i, active); };
                s->knob.setRange(0.0, 1.0); 
                s->knob.setValue(0.5); // Starts in the middle
            }
            addAndMakeVisible(xyPad);
            xyPad.onPointChanged = [this](float x, float y) { audioCore.setXyFilter(deckIdx, x, y); };
            xyPad.onEnableChanged = [this](bool e) { audioCore.setXyFilterEnabled(deckIdx, e); };
            xyPad.setVisible(false);
        }

        void paint(juce::Graphics& g) override {
            auto area = getLocalBounds();
            g.setColour(juce::Colour(0xff1a1a1a));
            g.fillRoundedRectangle(area.toFloat(), 8.0f);
            
            // Draw Tabs
            g.setFont(juce::Font(14.0f, juce::Font::bold));
            auto tabArea = area.removeFromTop(40);
            
            auto leftTab = tabArea.removeFromLeft(tabArea.getWidth() / 2);
            auto rightTab = tabArea;

            g.setColour(activeTab == 0 ? juce::Colours::white : juce::Colours::grey);
            g.drawText("EFFECTS", leftTab, juce::Justification::centred);
            
            g.setColour(activeTab == 1 ? juce::Colours::white : juce::Colours::grey);
            g.drawText("TOUCH", rightTab, juce::Justification::centred);
            
            g.setColour(juce::Colours::cyan);
            if (activeTab == 0) g.fillRect(leftTab.withSizeKeepingCentre(leftTab.getWidth() - 40, 2).withY(35));
            else g.fillRect(rightTab.withSizeKeepingCentre(rightTab.getWidth() - 40, 2).withY(35));
            
            g.setColour(juce::Colours::grey.withAlpha(0.3f));
            g.drawVerticalLine(area.getWidth() / 2, 8, 32);
        }

        void mouseDown(const juce::MouseEvent& e) override {
            if (e.y < 40) {
                int newTab = (e.x < getWidth() / 2) ? 0 : 1;
                if (newTab != activeTab) {
                    activeTab = newTab;
                    bool fxVisible = (activeTab == 0);
                    for (auto* s : slots) s->setVisible(fxVisible);
                    xyPad.setVisible(!fxVisible);
                    resized(); // Recalculate layout for the new active component
                    repaint();
                }
            }
        }

        void resized() override {
            auto area = getLocalBounds();
            area.removeFromTop(45); // Reserve space for tabs
            
            if (activeTab == 0) {
                auto grid = area.reduced(10);
                int kw = grid.getWidth() / 2, kh = grid.getHeight() / 3;
                for (int i = 0; i < 6; ++i) {
                    slots[i]->setBounds(grid.getX() + (i % 2) * kw, grid.getY() + (i / 2) * kh, kw, kh);
                    slots[i]->setVisible(true);
                }
                xyPad.setVisible(false);
            } else {
                for (auto* s : slots) s->setVisible(false);
                xyPad.setBounds(area.reduced(15));
                xyPad.setVisible(true);
            }
        }
    };

    void setupKnobCol(juce::OwnedArray<juce::Slider>& knobs, int deckIdx) {
        static const char* labelsText[] = { "TRIM", "HIGH", "MID", "LOW", "COLOR" };
        for (int i = 0; i < 5; ++i) {
            auto* lbl = knobLabels.add(new juce::Label());
            content.addAndMakeVisible(lbl);
            lbl->setText(labelsText[i], juce::dontSendNotification);
            lbl->setFont(juce::Font(12.0f, juce::Font::bold));
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setColour(juce::Label::textColourId, juce::Colours::white);

            auto* s = knobs.add(new juce::Slider());
            content.addAndMakeVisible(s);
            s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            
            if (i == 0) { 
                // TRIM: 0.0 to 2.0, default 1.0 (Middle)
                s->setRange(0.0, 2.0); 
                s->setValue(1.0); 
                s->onValueChange = [this, deckIdx, s] { audioCore.setDeckGain(deckIdx, (float)s->getValue()); }; 
            }
            else if (i < 4) { 
                // EQ HIGH/MID/LOW: -12.0 to +12.0, default 0.0 (Middle/Flat)
                s->setRange(-12.0, 12.0); 
                s->setValue(0.0); 
                s->onValueChange = [this, deckIdx, i, s] { audioCore.setDeckEQ(deckIdx, 3-i, (float)s->getValue()); }; 
            }
            else { 
                // COLOR (Filter): -1.0 to 1.0, default 0.0 (Middle/Off)
                s->setRange(-1.0, 1.0); 
                s->setValue(0.0); 
                s->onValueChange = [this, deckIdx, s] { audioCore.setDeckFilter(deckIdx, (float)s->getValue()); }; 
            }
        }
    }

    void layoutKnobCol(juce::OwnedArray<juce::Slider>& knobs, int x, int y, int w, int h) {
        int kh = h / 5;
        // Find labels starting from the end of the list (since we add them in pairs of 5)
        int labelStartIdx = (&knobs == &knobsA) ? 0 : 5;
        for (int i = 0; i < 5; ++i) {
            auto rect = juce::Rectangle<int>(x, y + i * kh, w, kh);
            knobLabels[labelStartIdx + i]->setBounds(rect.removeFromTop(15));
            knobs[i]->setBounds(rect.reduced(2));
        }
    }

    void setupFader(juce::Slider& s, int deckIdx) {
        content.addAndMakeVisible(s);
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setRange(0.0, 1.0);
        s.setValue(1.0);
        s.onValueChange = [this, deckIdx, &s] { audioCore.setDeckVolume(deckIdx, (float)s.getValue()); };
    }

    void setupCue(juce::TextButton& b, int deckIdx) {
        content.addAndMakeVisible(b);
        b.setButtonText("CUE");
        b.setClickingTogglesState(true);
        // Neon Green Style
        b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff003300)); // Darker background when off
        b.setColour(juce::TextButton::buttonOnColourId, juce::Colours::lime);
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::lime);
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    }

    AudioCore& audioCore;
    CustomLookAndFeel lookAndFeel;
    juce::Component content;
    juce::OwnedArray<juce::Label> knobLabels;
    FxPanel fxA, fxB;
    juce::OwnedArray<juce::Slider> knobsA, knobsB;
    juce::Slider faderA, faderB, crossfader;
    juce::TextButton cueA, cueB;
    MicControlWidget micControl;
    VstControlWidget vstControl;

};

class MixerComponent : public juce::Component, 
                      public juce::FileDragAndDropTarget,
                      public juce::DragAndDropTarget,
                      private juce::Timer {
public:
    MixerComponent(AudioCore& ac, TrackBrowserComponent* b) 
        : audioCore(ac), browser(b), waveA(ac, 0), waveB(ac, 1), deckA(ac, 0, true), deckB(ac, 1, false), mixer(ac) {
        addAndMakeVisible(waveA); addAndMakeVisible(waveB); addAndMakeVisible(deckA); addAndMakeVisible(deckB); addAndMakeVisible(mixer);
        startTimer(30); // Para Sync e outras lógicas de UI
    }
    
    void timerCallback() override {
        // Mover a lógica de Sync para cá para evitar travamentos na thread de áudio
        audioCore.handleSyncLogic();
    }
    
    // External File Drag (Explorer)
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray& files, int x, int) override {
        if (files.size() > 0) { 
            if (x < getWidth() / 2) audioCore.loadDeckA(juce::File(files[0])); 
            else audioCore.loadDeckB(juce::File(files[0])); 
        }
    }

    // Internal Drag (Track Browser Grid)
    bool isInterestedInDragSource(const SourceDetails& details) override {
        // TrackBrowserComponent returns pipe-separated paths as a string
        juce::String desc = details.description.toString();
        if (desc.contains("|")) return true;
        return juce::File::isAbsolutePath(desc);
    }
    void itemDropped(const SourceDetails& details) override {
        juce::String desc = details.description.toString();
        // Extract first path from pipe-separated string
        juce::String firstPath = desc.contains("|")
            ? desc.upToFirstOccurrenceOf("|", false, false).trim()
            : desc.trim();
        if (juce::File::isAbsolutePath(firstPath)) {
            int x = details.localPosition.getX();
            if (x < getWidth() / 2) audioCore.loadDeckA(juce::File(firstPath));
            else audioCore.loadDeckB(juce::File(firstPath));
        }
    }


    void paintOverChildren(juce::Graphics& g) override {
        auto area = getLocalBounds();
        auto h = getHeight();
        auto waveAreaHeight = (int)(h * 0.18f); 
        
        float centerX = area.getWidth() / 2.0f;
        
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillRect(centerX - 1.5f, 0.0f, 3.0f, (float)waveAreaHeight);
        
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.fillRect(centerX - 2.5f, 0.0f, 5.0f, (float)waveAreaHeight);
    }

    void resized() override {
        auto area = getLocalBounds();
        if (area.isEmpty()) return;
        auto w = area.getWidth();
        auto h = area.getHeight();

        // 1. Browser (Bottom 35%) - Fills width
        auto browserArea = area.removeFromBottom((int)(h * 0.35f));
        if (browser != nullptr) {
            browser->setTransform(juce::AffineTransform());
            browser->setBounds(browserArea);
            browser->setVisible(true);
        }

        // 2. Waveforms (Top 18%) - Fills width intelligently
        auto waveArea = area.removeFromTop((int)(h * 0.18f));
        waveA.setTransform(juce::AffineTransform());
        waveB.setTransform(juce::AffineTransform());
        waveA.setBounds(waveArea.removeFromTop(waveArea.getHeight() / 2).reduced(2)); 
        waveB.setBounds(waveArea.reduced(2));

        // 3. Middle Section: [ Deck A ] [ Mixer ] [ Deck B ]
        area.reduce(10, 5);
        auto middleRow = area;
        int centerW = (int)(w * 0.35f); 
        int sideW = (w - centerW) / 2;
        
        deckA.setVisible(true);
        deckB.setVisible(true);
        
        deckA.setTransform(juce::AffineTransform());
        deckB.setTransform(juce::AffineTransform());
        mixer.setTransform(juce::AffineTransform());

        deckA.setBounds(middleRow.removeFromLeft(sideW).reduced(2));
        mixer.setBounds(middleRow.removeFromLeft(centerW).reduced(2));
        deckB.setBounds(middleRow.reduced(2));
    }
private:
    AudioCore& audioCore; TrackBrowserComponent* browser; SimpleWaveform waveA, waveB; DeckSection deckA, deckB; MixerCenterSection mixer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerComponent)
};
