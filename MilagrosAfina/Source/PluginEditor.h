#pragma once

#include "PluginProcessor.h"

class MilagrosAfinaEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit MilagrosAfinaEditor (MilagrosAfinaProcessor&);
    ~MilagrosAfinaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    class PitchView;
    class MainPage;
    class AdvisorPage;

    MilagrosAfinaProcessor& proc;
    int advisorTick = 0;
    std::unique_ptr<MainPage> mainPage;
    std::unique_ptr<AdvisorPage> advisorPage;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilagrosAfinaEditor)
};
