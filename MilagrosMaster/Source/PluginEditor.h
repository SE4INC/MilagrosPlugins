#pragma once

#include "PluginProcessor.h"

class MilagrosMasterEditor : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit MilagrosMasterEditor (MilagrosMasterProcessor&);
    ~MilagrosMasterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    class MeterView;
    class MainPage;
    class AdvisorPage;

    MilagrosMasterProcessor& proc;
    int advisorTick = 0;
    std::unique_ptr<MainPage> mainPage;
    std::unique_ptr<AdvisorPage> advisorPage;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilagrosMasterEditor)
};
