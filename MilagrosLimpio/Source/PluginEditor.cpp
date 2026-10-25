#include "PluginEditor.h"

namespace
{
    const juce::Colour bgColour (0xff14161c);
    const juce::Colour accent   (0xffffb347);
    const juce::Colour cyan     (0xff7ad7f0);
}

MilagrosLimpioEditor::MilagrosLimpioEditor (MilagrosLimpioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("mode")))
        modeBox.addItemList (prm->choices, 1);
    addAndMakeVisible (modeBox);
    modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "mode", modeBox);

    learnButton.setButtonText (es ("Aprender ruido"));
    learnButton.setClickingTogglesState (true);
    learnButton.setColour (juce::ToggleButton::textColourId, accent);
    learnButton.setColour (juce::ToggleButton::tickColourId, accent);
    learnButton.onClick = [this]
    {
        const bool on = learnButton.getToggleState();
        proc.setLearning (on);
        learnButton.setButtonText (on ? es ("Detener aprendizaje") : es ("Aprender ruido"));
    };
    addAndMakeVisible (learnButton);

    gateButton.setButtonText (es ("Puerta de ruido"));
    gateButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    gateButton.setColour (juce::ToggleButton::tickColourId, cyan);
    addAndMakeVisible (gateButton);
    gateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "gate", gateButton);

    status.setJustificationType (juce::Justification::centred);
    status.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    status.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (status);

    setupKnob (reduction, "reduction", es ("Reducción"), true);
    setupKnob (smooth,    "smooth",    es ("Suavidad"));
    setupKnob (sens,      "sens",      es ("Sensibilidad"));
    setupKnob (depth,     "depth",     es ("Profundidad"));
    setupKnob (hold,      "hold",      es ("Mantener"));
    setupKnob (release,   "release",   es ("Liberación"));
    setupKnob (output,    "output",    es ("Salida"));

    setSize (580, 420);
    startTimerHz (5);
    timerCallback();
}

MilagrosLimpioEditor::~MilagrosLimpioEditor()
{
    stopTimer();
    proc.setLearning (false);
}

void MilagrosLimpioEditor::setupKnob (Knob& k, const char* id, const juce::String& text, bool highlight)
{
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, highlight ? accent : cyan);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, highlight ? accent : juce::Colours::white);
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void MilagrosLimpioEditor::layoutRow (juce::Rectangle<int> area, std::vector<Knob*> knobs)
{
    const int w = area.getWidth() / (int) knobs.size();
    for (auto* k : knobs)
    {
        auto cell = area.removeFromLeft (w).reduced (4);
        k->label.setBounds (cell.removeFromTop (20));
        k->slider.setBounds (cell);
    }
}

void MilagrosLimpioEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
    g.setColour (juce::Colours::grey);
    g.setFont (13.0f);
    g.drawText (es ("MilagrosLimpio v0.1 · gratis y de código abierto"),
                getLocalBounds().removeFromBottom (24), juce::Justification::centred);
}

void MilagrosLimpioEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    r.removeFromBottom (24);

    auto top = r.removeFromTop (30);
    modeBox.setBounds (top.removeFromLeft (170));
    top.removeFromLeft (10);
    learnButton.setBounds (top.removeFromLeft (170));
    top.removeFromLeft (10);
    gateButton.setBounds (top);

    r.removeFromTop (8);
    status.setBounds (r.removeFromTop (24));
    r.removeFromTop (8);

    auto row1 = r.removeFromTop (r.getHeight() / 2);
    layoutRow (row1, { &reduction, &smooth, &sens, &depth });
    layoutRow (r,    { &hold, &release, &output });
}

void MilagrosLimpioEditor::timerCallback()
{
    const auto s = proc.getStatus();
    juce::String text;

    if (s.learning)
    {
        text = es ("Aprendiendo ruido… quédate en silencio 2 o 3 segundos y pulsa de nuevo.");
    }
    else
    {
        text = es ("Puerta: ") + (s.gateOpen ? es ("abierta") : es ("cerrada"))
             + es ("   |   Ruido de fondo: ") + juce::String (s.floorDb, 0) + " dB"
             + es ("   |   Perfil: ") + (s.profileValid ? es ("aprendido") : es ("no"));
    }

    if (status.getText() != text)
        status.setText (text, juce::dontSendNotification);
}
