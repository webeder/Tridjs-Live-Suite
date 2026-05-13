#include "PadsGridComponent.h"
#include "LanguageManager.h"

PadsGridComponent::PadsGridComponent()
{
    // Cria os 9 pads simulando 3x3
    for (int i = 0; i < 9; ++i)
    {
        auto pad = std::make_unique<PadComponent> (i);
        addAndMakeVisible (pad.get());
        pads.push_back (std::move (pad));
    }

    // Tempo Control Side Panel
    pitchSlider.setSliderStyle (juce::Slider::LinearVertical);
    pitchSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pitchSlider.setRange (-0.06, 0.06, 0.001); // +/- 6%
    pitchSlider.setValue (0.0);
    pitchSlider.setColour(juce::Slider::thumbColourId, juce::Colours::cyan);
    pitchSlider.onValueChange = [this] {
        if (onPitchChanged) onPitchChanged(pitchSlider.getValue());
    };
    addAndMakeVisible (pitchSlider);

    bpmLcd.setFont(juce::Font(28.0f, juce::Font::bold));
    bpmLcd.setColour(juce::Label::textColourId, juce::Colour((juce::uint32)0xffffa500));
    bpmLcd.setJustificationType(juce::Justification::centred);
    addAndMakeVisible (bpmLcd);

    resetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
    resetBtn.onClick = [this] {
        pitchSlider.setValue(0.0, juce::sendNotification);
    };
    addAndMakeVisible (resetBtn);

    LanguageManager::getInstance().addChangeListener(this);
    updateLanguage();
}

PadsGridComponent::~PadsGridComponent()
{
    LanguageManager::getInstance().removeChangeListener(this);
}

void PadsGridComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    updateLanguage();
}

void PadsGridComponent::updateLanguage()
{
    resetBtn.setButtonText(TJS_L("HF_RESET"));
    repaint();
}

void PadsGridComponent::updateBpmDisplay(double bpm)
{
    bpmLcd.setText(juce::String(bpm, 2), juce::dontSendNotification);
}

void PadsGridComponent::updatePitchFromExtern(float value)
{
    // Mapeia valor MIDI 0..1 para o range do slider -0.06..0.06
    double pitchVal = (value * 0.12) - 0.06;
    pitchSlider.setValue(pitchVal, juce::dontSendNotification);
}

void PadsGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour ((juce::uint32)0xff121212)); // Fundo do grid
}

void PadsGridComponent::resized()
{
    auto area = getLocalBounds().reduced (10);

    // Tempo Section na direita do Grid
    auto tempoArea = area.removeFromRight(100);
    bpmLcd.setBounds(tempoArea.removeFromTop(40));
    resetBtn.setBounds(tempoArea.removeFromTop(40).reduced(5));
    pitchSlider.setBounds(tempoArea.reduced(20, 10));

    // Desconto de espaçamento central
    area.removeFromRight(20);

    // Padding grid (3x3)
    int columns = 3;
    int rows = 3;
    
    int padWidth = area.getWidth() / columns;
    int padHeight = area.getHeight() / rows;

    for (int i = 0; i < 9; ++i)
    {
        int col = i % columns;
        int row = i / columns;

        auto* pad = pads[(size_t)i].get();

        juce::Rectangle<int> r (area.getX() + col * padWidth,
                                area.getY() + row * padHeight,
                                padWidth, padHeight);

        // Deixamos um 'gap' bonitinho de 5px entre os pads
        pad->setBounds (r.reduced (5));
    }
}
