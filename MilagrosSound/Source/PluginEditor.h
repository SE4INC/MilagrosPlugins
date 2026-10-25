#pragma once

#include "PluginProcessor.h"

class MilagrosSoundEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit MilagrosSoundEditor (MilagrosSoundProcessor&);
    ~MilagrosSoundEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    class MainPage;
    class AdvisorPage;

    MilagrosSoundProcessor& proc;
    std::unique_ptr<MainPage> mainPage;
    std::unique_ptr<AdvisorPage> advisorPage;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilagrosSoundEditor)
};
