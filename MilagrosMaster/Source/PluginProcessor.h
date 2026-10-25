// MilagrosMaster - master fácil, gratuito y de código abierto (AGPL-3.0)
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

#include "LoudnessMeter.h"
#include "Limiter.h"

inline juce::String es (const char* utf8) { return juce::String::fromUTF8 (utf8); }

class MilagrosMasterProcessor : public juce::AudioProcessor
{
public:
    MilagrosMasterProcessor();
    ~MilagrosMasterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MilagrosMaster"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Objetivos de volumen (LUFS) en el mismo orden que el parámetro "target"
    static float targetLufs (int idx)
    {
        static const float t[5] = { -14.0f, -12.0f, -9.0f, -8.0f, -16.0f };
        return t[juce::jlimit (0, 4, idx)];
    }

    struct Meters
    {
        float momentary = -120.0f, shortTerm = -120.0f, integrated = -120.0f;
        int validBlocks = 0;
        float inPeakDb = -120.0f, outPeakDb = -120.0f;
        float corr = 1.0f, sideMidDb = -60.0f, lowRatio = 0.0f;
        float limGrDb = 0.0f, limGrMaxDb = 0.0f, heavyFrac = 0.0f, matchDb = 0.0f;
    };
    Meters getMeters() const;
    void resetMeters() { resetRequested.store (true); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateEq();
    void resetInternal();

    double sr = 44100.0;
    LoudnessMeter meterIn, meterOut;
    LookaheadLimiter limiter;

    juce::dsp::IIR::Filter<float> hpf[2], lowShelf[2], highShelf[2], sideHp, lowLp;
    float lastLows = 1000.0f, lastBright = 1000.0f;

    // Compresor "pegamento"
    float compGrDb = 0.0f, avgPow = 0.0f, avgCoef = 0.0f, atkCoef = 0.0f, relCoef = 0.0f, limRelCoef = 0.0f;

    // Medidores de estéreo y graves
    double sLR = 0.0, sLL = 0.0, sRR = 0.0, sMM = 0.0, sSS = 0.0, lowE = 0.0, totE = 0.0, meterCoef = 0.0;

    // Comparar con el original
    std::vector<float> dryL, dryR;
    int dryPos = 0;
    float matchDb = 0.0f;

    bool wasPlaying = false;
    double heavySamples = 0.0, totalSamples = 0.0;
    std::atomic<bool> resetRequested { false };

    std::atomic<float> *trimP = nullptr, *lowsP = nullptr, *brightP = nullptr, *glueP = nullptr,
                       *widthP = nullptr, *warmP = nullptr, *driveP = nullptr, *ceilingP = nullptr,
                       *bassMonoP = nullptr, *compareP = nullptr;

    std::atomic<float> inPeakLin { 0.0f }, outPeakLin { 0.0f }, corrA { 1.0f }, sideMidA { -60.0f },
                       lowRatioA { 0.0f }, limGrA { 0.0f }, limGrMaxA { 0.0f }, heavyFracA { 0.0f }, matchA { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilagrosMasterProcessor)
};
