#pragma once

#include "PluginProcessor.h"

class MilagrosLimpioEditor : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit MilagrosLimpioEditor (MilagrosLimpioProcessor&);
    ~MilagrosLimpioEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob& k, const char* id, const juce::String& text, bool highlight = false);
    static void layoutRow (juce::Rectangle<int> area, std::vector<Knob*> knobs);
    void timerCallback() override;

    MilagrosLimpioProcessor& proc;

    juce::ComboBox modeBox;
    juce::ToggleButton learnButton, gateButton;
    juce::Label status;
    Knob reduction, smooth, sens, depth, hold, release, output;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilagrosLimpioEditor)
};
