// MilagrosLimpio - reducción de ruido + puerta de ruido inteligente (AGPL-3.0)
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

inline juce::String es (const char* utf8) { return juce::String::fromUTF8 (utf8); }

class MilagrosLimpioProcessor : public juce::AudioProcessor
{
public:
    MilagrosLimpioProcessor();
    ~MilagrosLimpioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MilagrosLimpio"; }
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

    // Aprender el perfil de ruido (el botón de la interfaz lo activa)
    void setLearning (bool shouldLearn) { learning.store (shouldLearn); }

    struct Status
    {
        bool learning = false, profileValid = false, gateOpen = false;
        float floorDb = -75.0f;
    };
    Status getStatus() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- STFT: 1024 muestras, solapamiento del 75 % (latencia = 1024 muestras) ----
    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int numBins = fftSize / 2 + 1;

    struct Channel
    {
        std::array<float, fftSize> inBuf {}, outBuf {};
        std::array<float, numBins> profile {}, smooth {}, tracker {}, gainPrev {};
        int pos = 0, hopCount = 0, learnFrames = 0;
        bool trackerInit = false;
    };

    void resetChannel (Channel& c);
    float stftProcess (Channel& c, float x);
    void processFrame (Channel& c);

    double sr = 44100.0;
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> window {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, numBins> gains {};
    Channel chan[2];

    // Parámetros del bloque actual (denoiser)
    bool reductionOn = true, useProfile = false, profileOk = false, learningNow = false;
    float alpha = 2.0f, floorGain = 0.1f, relCoefGain = 0.3f;

    // ---- Puerta inteligente ----
    juce::dsp::IIR::Filter<float> detHp, detLp;
    float env = 0.0f, envCoef = 0.0f, atkCoef = 0.0f;
    float curMin = 0.0f, floorDb = 0.0f;
    std::array<float, 6> subMin {};
    int subIdx = 0, subCount = 0, subLen = 22050, holdCounter = 0;
    bool gateOpen = false;
    float gateGain = 1.0f;

    // Estado compartido con la interfaz
    std::atomic<bool> learning { false }, profileValid { false }, gateOpenFlag { false };
    std::atomic<float> floorShown { -75.0f };
    bool wasLearning = false;

    std::atomic<float> *reductionP = nullptr, *smoothP = nullptr, *modeP = nullptr, *gateOnP = nullptr,
                       *sensP = nullptr, *holdP = nullptr, *releaseP = nullptr, *depthP = nullptr,
                       *outputP = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilagrosLimpioProcessor)
};
