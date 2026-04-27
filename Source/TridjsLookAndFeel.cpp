#include "TridjsLookAndFeel.h"

TridjsLookAndFeel::TridjsLookAndFeel()
{
    // Define paleta de cores Dark Mode / Minimalista
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff121212));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff00d1b2)); // Tema verde cyan
    setColour(juce::Slider::trackColourId, juce::Colour(0xff1e1e1e));

    setDefaultSansSerifTypefaceName("Inter");
}

void TridjsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto cornerSize = 8.0f;

    juce::Colour baseColour = backgroundColour;
    
    // Lógica dinâmica de cores (Glow pulsante/Toggle)
    bool isToggled = button.getToggleState();
    if (isToggled)
    {
        baseColour = baseColour.brighter(1.0f); // Max brightness
        // Desenhar "Glow" em volta (sombreamento do próprio botão)
        g.setColour(baseColour.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.expanded(2.0f), cornerSize + 2.0f);
    }
    else if (shouldDrawButtonAsDown)        
    {
        baseColour = baseColour.brighter(0.2f);
    }
    else if (shouldDrawButtonAsHighlighted) 
    {
        baseColour = baseColour.brighter(0.1f);
    }

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, cornerSize);

    // Efeito sutil de borda
    g.setColour(baseColour.brighter(0.2f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.0f);
}

void TridjsLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    // Implementaçao Linear estilo DJ / Recordbox Pitch
    auto trackWidth = juce::jmin(8.0f, (float)width * 0.2f);
    juce::Point<float> startPoint(slider.isHorizontal() ? (float)x : (float)x + (float)width * 0.5f,
                                  slider.isHorizontal() ? (float)y + (float)height * 0.5f : (float)(height + y));
    juce::Point<float> endPoint(slider.isHorizontal() ? (float)(width + x) : startPoint.x,
                                slider.isHorizontal() ? startPoint.y : (float)y);

    juce::Path backgroundTrack;
    backgroundTrack.startNewSubPath(startPoint);
    backgroundTrack.lineTo(endPoint);
    g.setColour(slider.findColour(juce::Slider::trackColourId));
    g.strokePath(backgroundTrack, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Thumb
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    juce::Rectangle<float> thumbInfo;
    if (slider.isHorizontal()) {
        thumbInfo = juce::Rectangle<float>(sliderPos - 5.0f, (float)y, 10.0f, (float)height);
    } else {
        thumbInfo = juce::Rectangle<float>((float)x, sliderPos - 5.0f, (float)width, 10.0f);
    }
    g.fillRoundedRectangle(thumbInfo, 2.0f);
}

void TridjsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider)
{
    // Knob Circular estilo Mixer Master
    auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
    auto centreX = (float)x + (float)width  * 0.5f;
    auto centreY = (float)y + (float)height * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Knob Base
    g.setColour(juce::Colour(0xff252525));
    g.fillEllipse(rx, ry, rw, rw);
    g.setColour(juce::Colour(0xff0a0a0a));
    g.drawEllipse(rx, ry, rw, rw, 1.0f);

    // Pointer
    juce::Path p;
    auto pointerLength = radius * 0.8f;
    auto pointerThickness = 3.0f;
    p.addRoundedRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength, 1.0f);
    p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.fillPath(p);
}
