#pragma once

#include <JuceHeader.h>

class TridjsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TridjsLookAndFeel();
    ~TridjsLookAndFeel() override = default;

    // Override para desenhar botões customizados com cantos arredondados (8px) e cores
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // Override para criar sliders modernos
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override;


private:
    juce::Font getCustomFont();
};
