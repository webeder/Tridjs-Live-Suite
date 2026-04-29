#include "TridjsLookAndFeel.h"

TridjsLookAndFeel::TridjsLookAndFeel()
{
    // Define paleta de cores Dark Mode / Minimalista
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff121212));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a2a));
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff00ffff)); // Vibrant Cyan
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
    if (button.getButtonText() == "AUTO LOOP")
        g.setColour(juce::Colour(0xffffa500).withAlpha(0.6f));
    else if (button.getButtonText() == "IN" || button.getButtonText() == "OUT")
        g.setColour(juce::Colours::white.withAlpha(0.4f));
    else
        g.setColour(baseColour.brighter(0.2f));
        
    g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.0f);
}

void TridjsLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    auto area = button.getLocalBounds().toFloat();
    auto text = button.getButtonText();
    
    // Design especial para botões de transporte (PLAY, STOP, CUE) conforme imagem
    if (text == "PLAY" || text == "STOP" || text == "CUE")
    {
        g.setColour(juce::Colours::black.withAlpha(0.8f)); // Ícone e texto em preto/escuro
        
        float iconSize = area.getHeight() * 0.35f;
        auto iconArea = area.removeFromTop(area.getHeight() * 0.65f);
        
        juce::Path p;
        if (text == "PLAY") {
            p.addTriangle(iconArea.getCentreX() - iconSize * 0.4f, iconArea.getCentreY() - iconSize * 0.5f,
                          iconArea.getCentreX() - iconSize * 0.4f, iconArea.getCentreY() + iconSize * 0.5f,
                          iconArea.getCentreX() + iconSize * 0.5f, iconArea.getCentreY());
        } else if (text == "STOP") {
            p.addRectangle(iconArea.getCentreX() - iconSize * 0.4f, iconArea.getCentreY() - iconSize * 0.4f, iconSize * 0.8f, iconSize * 0.8f);
        } else if (text == "CUE") {
            // Ícone CUE estilo imagem (duas barras verticais)
            float barW = iconSize * 0.25f;
            float barH = iconSize * 0.7f;
            float gap = iconSize * 0.15f;
            p.addRoundedRectangle(iconArea.getCentreX() - barW - gap/2, iconArea.getCentreY() - barH/2, barW, barH, 1.0f);
            p.addRoundedRectangle(iconArea.getCentreX() + gap/2, iconArea.getCentreY() - barH/2, barW, barH, 1.0f);
        }
        
        g.fillPath(p);
        
        g.setFont(juce::Font("Roboto", area.getHeight() * 0.8f, juce::Font::bold));
        g.drawText(text, area.withTrimmedTop(-5.0f), juce::Justification::centredTop);
    }
    else if (text == "IN" || text == "OUT")
    {
        g.setFont(juce::Font("Roboto", area.getHeight() * 0.6f, juce::Font::bold));
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.drawText(text, area, juce::Justification::centred);
    }
    else if (text == "AUTO LOOP")
    {
        g.setFont(juce::Font("Roboto", area.getHeight() * 0.35f, juce::Font::bold));
        g.setColour(juce::Colour(0xffffa500));
        g.drawText(text, area, juce::Justification::centred);
    }
    else
    {
        g.setFont(juce::Font("Roboto", 14.0f, juce::Font::bold));
        g.setColour(button.findColour(juce::TextButton::textColourOffId));
        g.drawText(text, area, juce::Justification::centred);
    }
}

void TridjsLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    auto isVertical = slider.isVertical();
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    
    if (isVertical)
    {
        // Background container border
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 3.0f, 1.0f);

        float centreX = bounds.getCentreX();
        
        // Track line (thin and dark)
        g.setColour(juce::Colours::black);
        g.fillRect(centreX - 1.0f, bounds.getY() + 35, 2.0f, bounds.getHeight() - 70);
        
        // Range Labels (como na imagem)
        g.setColour(juce::Colours::grey.withAlpha(0.5f));
        g.setFont(juce::Font("Roboto", 12.0f, juce::Font::bold));
        
        juce::String topText = "+10";
        juce::String bottomText = "-10";
        
        if (slider.getName() == "PitchSlider") {
            topText = "+6";
            bottomText = "-6";
        }
        
        g.drawText(topText, (int)bounds.getX(), (int)bounds.getY() + 10, (int)bounds.getWidth(), 20, juce::Justification::centred);
        g.drawText(bottomText, (int)bounds.getX(), (int)bounds.getBottom() - 30, (int)bounds.getWidth(), 20, juce::Justification::centred);

        // Thumb - Large rectangle style DJ
        float thumbW = bounds.getWidth() * 0.75f;
        float thumbH = 45.0f;
        juce::Rectangle<float> thumb(centreX - thumbW*0.5f, sliderPos - thumbH*0.5f, thumbW, thumbH);
        
        // Draw thumb body
        g.setColour(juce::Colour(0xff2d2d35));
        g.fillRoundedRectangle(thumb, 2.0f);
        
        // Cyan indicator line in the middle
        g.setColour(juce::Colours::cyan);
        g.fillRect(thumb.getX(), thumb.getCentreY() - 1.0f, thumb.getWidth(), 2.0f);
    }
    else
    {
        // Fallback for horizontal or default
        auto trackWidth = 6.0f;
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.fillRect((float)x, (float)y + (float)height * 0.5f - trackWidth * 0.5f, (float)width, trackWidth);
        
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillRoundedRectangle(sliderPos - 5.0f, (float)y, 10.0f, (float)height, 2.0f);
    }
}

void TridjsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider)
{
    // Knob Circular estilo imagem (Minimalista Dark)
    auto radius = (float)juce::jmin(width / 2, height / 2) - 2.0f;
    auto centreX = (float)x + (float)width  * 0.5f;
    auto centreY = (float)y + (float)height * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Knob Base - Darker Charcoal
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillEllipse(rx, ry, rw, rw);
    
    // Subtle outer border for depth
    g.setColour(juce::Colour(0xff050505));
    g.drawEllipse(rx, ry, rw, rw, 1.0f);

    // Indicator Line (Pointer) - Like image 1
    juce::Path p;
    auto pointerLength = radius * 0.65f;
    auto pointerThickness = radius * 0.28f;
    
    // Draw a thick rectangle from the top edge towards the center
    p.addRoundedRectangle(-pointerThickness * 0.5f, -radius + 1.0f, pointerThickness, pointerLength, 1.5f);
    
    p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    
    // Cyan pointer with a slight glow
    auto thumbColor = slider.findColour(juce::Slider::thumbColourId);
    g.setColour(thumbColor);
    g.fillPath(p);
}
