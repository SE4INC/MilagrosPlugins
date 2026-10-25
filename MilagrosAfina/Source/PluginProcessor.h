// MilagrosAfina - afinador de voz gratuito y de código abierto (AGPL-3.0)
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

#include "Scales.h"
#include "PitchDetector.h"
#include "PitchShifter.h"
#include "KeyAnalyzer.h"

inline juce::String es (const char* utf8) { return juce::String::fromUTF8 (utf8); }

class MilagrosAfinaProcessor : public juce::AudioProcessor
{
public:
    MilagrosAfinaProcessor();
    ~MilagrosAfinaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MilagrosAfina"; }
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

    // ---- Datos para la interfaz ----
    static constexpr int dispSize = 400;

    struct Live
    {
        bool voiced = false;
        float inMidi = 0.0f, targetMidi = 0.0f;
    };
    Live getLive() const;
    // Historial de notas (midi, o -1 si no hay voz), de la más antigua a la más reciente
    void copyDisplay (std::array<float, dispSize>& in, std::array<float, dispSize>& out) const;

    struct Analysis
    {
        int voicedFrames = 0, inTuneFrames = 0, instFrames = 0;
        float avgConfidence = 0.0f, avgLevelDb = -100.0f, peakDb = -100.0f;
        std::array<int, 128> pitchHist {};
        std::array<float, 12> voiceChroma {}, instChroma {};
    };
    Analysis getAnalysis() const;
    void resetAnalysis();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void onPitch();
    float medianF0() const;

    double sr = 44100.0;
    PitchDetector detector;
    PitchShifter shifter[2];
    KeyAnalyzer keyInst;

    // Estado de la corrección
    float ratioTarget = 1.0f, ratioSm = 1.0f, ratioCoef = 0.01f;
    float corrCents = 0.0f, periodSm = 300.0f;
    double hopSeconds = 0.004;
    bool voicedFlag = false, targetValid = false;
    int target = 60, unvoicedFrames = 100, jumpCount = 0, dispCounter = 0;
    std::array<float, 5> f0Ring {};
    int f0Count = 0, f0Pos = 0;

    // Parámetros
    std::atomic<float> *keyP = nullptr, *scaleP = nullptr, *speedP = nullptr, *amountP = nullptr,
                       *refAP = nullptr, *threshP = nullptr;

    // Visualización y análisis (hilo de audio -> interfaz)
    std::atomic<float> dispIn[dispSize], dispOut[dispSize];
    std::atomic<int> dispIdx { 0 };
    std::atomic<float> liveIn { -1.0f }, liveTarget { -1.0f };
    std::atomic<int> voicedFrames { 0 }, inTuneFrames { 0 };
    std::atomic<float> avgConf { 0.0f }, avgLevel { -100.0f }, peakLin { 0.0f };
    std::atomic<int> pitchHist[128];
    std::atomic<int> voiceCount[12];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilagrosAfinaProcessor)
};
