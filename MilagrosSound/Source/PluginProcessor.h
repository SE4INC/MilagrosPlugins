// MilagrosSound - cadena vocal gratuita y de código abierto (AGPL-3.0)
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

// Convierte texto UTF-8 (con tildes, ñ...) a juce::String sin problemas.
inline juce::String es (const char* utf8) { return juce::String::fromUTF8 (utf8); }

class MilagrosSoundProcessor : public juce::AudioProcessor
{
public:
    MilagrosSoundProcessor();
    ~MilagrosSoundProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MilagrosSound"; }
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

    // ---- Asesor: análisis del espectro de la voz de entrada ----
    struct Analysis
    {
        int frames = 0;                       // cuántos fragmentos con voz se han analizado
        float lowDb = -200, mudDb = -200, midDb = -200,
              presenceDb = -200, sibDb = -200, airDb = -200; // densidad de potencia por banda (dB)
        float peakDb = -100, rmsDb = -100;
    };
    Analysis getAnalysis() const;
    void resetAnalysis();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters (bool force);
    void analyse (const juce::AudioBuffer<float>& buffer);
    void processAnalysisFrame();

    double sr = 44100.0;

    // Cadena (hasta 2 canales)
    juce::dsp::IIR::Filter<float> hpf[2], mud[2], presence[2], warm[2], air[2], sibBand[2];
    juce::dsp::Compressor<float> compressor;
    juce::AudioBuffer<float> dryBuffer;
    float sibEnv[2] = { 0.0f, 0.0f }, fullEnv[2] = { 0.0f, 0.0f };
    float envAtk = 0.0f, envRel = 0.0f, fullRel = 0.0f;
    std::array<float, 8> lastKey {};

    // Parámetros
    std::atomic<float> *enhanceP = nullptr, *clarityP = nullptr, *warmthP = nullptr, *airP = nullptr,
                       *mudP = nullptr, *deessP = nullptr, *compP = nullptr, *mixP = nullptr,
                       *outputP = nullptr, *voiceP = nullptr, *micP = nullptr;

    // Análisis (FFT)
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numBands = 6;
    static constexpr float bandLo[numBands] = { 20.0f, 200.0f, 500.0f, 2500.0f, 5000.0f, 10000.0f };
    static constexpr float bandHi[numBands] = { 80.0f, 400.0f, 2000.0f, 5000.0f, 9000.0f, 16000.0f };

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> fifo {};
    std::array<float, fftSize * 2> fftData {};
    int fifoIndex = 0;
    std::atomic<float> bandPower[numBands];
    std::atomic<float> rmsAvg { 0.0f }, peak { 0.0f };
    std::atomic<int> frameCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilagrosSoundProcessor)
};
