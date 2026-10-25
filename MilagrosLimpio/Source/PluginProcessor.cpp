#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MilagrosLimpioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    auto pctAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() / 100.0f; });

    auto msAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });

    auto dbAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });

    auto addPct = [&] (const char* id, const String& name, float def)
    {
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def, pctAttr));
    };

    addPct ("reduction", es ("Reducción"), 0.6f);
    addPct ("smooth",    es ("Suavidad"), 0.5f);
    addPct ("sens",      es ("Sensibilidad"), 0.5f);
    addPct ("depth",     es ("Profundidad"), 0.5f);

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "hold", 1 }, es ("Mantener"), NormalisableRange<float> (20.0f, 500.0f, 1.0f), 120.0f, msAttr));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "release", 1 }, es ("Liberación"), NormalisableRange<float> (20.0f, 800.0f, 1.0f), 200.0f, msAttr));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, es ("Salida"), NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "mode", 1 }, es ("Modo de ruido"),
        StringArray { es ("Automático"), es ("Perfil aprendido") }, 0));

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "gate", 1 }, es ("Puerta de ruido"), true));

    return { params.begin(), params.end() };
}

//==============================================================================
MilagrosLimpioProcessor::MilagrosLimpioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    reductionP = apvts.getRawParameterValue ("reduction");
    smoothP    = apvts.getRawParameterValue ("smooth");
    modeP      = apvts.getRawParameterValue ("mode");
    gateOnP    = apvts.getRawParameterValue ("gate");
    sensP      = apvts.getRawParameterValue ("sens");
    holdP      = apvts.getRawParameterValue ("hold");
    releaseP   = apvts.getRawParameterValue ("release");
    depthP     = apvts.getRawParameterValue ("depth");
    outputP    = apvts.getRawParameterValue ("output");
}

bool MilagrosLimpioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void MilagrosLimpioProcessor::resetChannel (Channel& c)
{
    c.inBuf.fill (0.0f);
    c.outBuf.fill (0.0f);
    c.profile.fill (0.0f);
    c.smooth.fill (0.0f);
    c.tracker.fill (0.0f);
    c.gainPrev.fill (1.0f);
    c.pos = c.hopCount = c.learnFrames = 0;
    c.trackerInit = false;
}

void MilagrosLimpioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    setLatencySamples (fftSize);   // FL Studio compensa esta latencia automáticamente

    for (int j = 0; j < fftSize; ++j)   // ventana raíz-Hann (análisis y síntesis)
        window[(size_t) j] = std::sqrt (0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * (float) j / (float) fftSize)));

    resetChannel (chan[0]);
    resetChannel (chan[1]);
    profileValid.store (false);

    // Detector de la puerta: solo escucha la zona de la voz (120 Hz - 4.5 kHz)
    detHp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 120.0, 0.707f);
    detLp.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, juce::jmin (4500.0, sr * 0.45), 0.707f);
    detHp.reset();
    detLp.reset();

    envCoef = 1.0f - std::exp (-1.0f / (0.010f * (float) sr));   // ventana de 10 ms
    atkCoef = std::exp (-1.0f / (0.003f * (float) sr));          // apertura en 3 ms
    env = 0.0f;
    curMin = 0.0f;
    floorDb = 0.0f;
    subMin.fill (0.0f);
    subIdx = subCount = holdCounter = 0;
    subLen = (int) (0.5 * sr);
    gateOpen = false;
    gateGain = 1.0f;
}

//==============================================================================
float MilagrosLimpioProcessor::stftProcess (Channel& c, float x)
{
    c.inBuf[(size_t) c.pos] = x;
    const float out = c.outBuf[(size_t) c.pos];
    c.outBuf[(size_t) c.pos] = 0.0f;
    c.pos = (c.pos + 1) & (fftSize - 1);

    if (++c.hopCount >= hopSize)
    {
        c.hopCount = 0;
        processFrame (c);
    }
    return out;
}

void MilagrosLimpioProcessor::processFrame (Channel& c)
{
    for (int j = 0; j < fftSize; ++j)
        fftData[(size_t) j] = c.inBuf[(size_t) ((c.pos + j) & (fftSize - 1))] * window[(size_t) j];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

    fft.performRealOnlyForwardTransform (fftData.data(), true);

    for (int k = 0; k < numBins; ++k)
    {
        const size_t ks = (size_t) k;
        const float re = fftData[2 * ks], im = fftData[2 * ks + 1];
        const float pw = re * re + im * im;

        if (! c.trackerInit)
        {
            c.smooth[ks] = pw;
            c.tracker[ks] = pw;
        }

        // Seguimiento automático del ruido: baja rápido, sube muy despacio
        c.smooth[ks] = 0.6f * c.smooth[ks] + 0.4f * pw;
        if (c.smooth[ks] < c.tracker[ks])
            c.tracker[ks] = 0.7f * c.tracker[ks] + 0.3f * c.smooth[ks];
        else
            c.tracker[ks] *= 1.001f;

        // Aprendizaje del perfil (promedio de los fragmentos en silencio)
        if (learningNow)
            c.profile[ks] += (pw - c.profile[ks]) / (float) (c.learnFrames + 1);

        const float noise = (useProfile && profileOk) ? c.profile[ks] : c.tracker[ks] * 2.0f;

        float g = 1.0f;
        if (reductionOn)
        {
            const float g2 = 1.0f - alpha * noise / (pw + 1.0e-12f);   // resta espectral
            g = std::sqrt (juce::jmax (g2, floorGain * floorGain));
        }

        // Suavizado en el tiempo: sube rápido (no apaga la voz), baja despacio (evita "burbujas")
        float& prev = c.gainPrev[ks];
        prev += (g - prev) * (g > prev ? 0.7f : relCoefGain);
        gains[ks] = prev;
    }

    c.trackerInit = true;
    if (learningNow)
        ++c.learnFrames;

    // Suavizado en frecuencia y aplicación de la ganancia
    for (int k = 0; k < numBins; ++k)
    {
        const size_t ks = (size_t) k;
        const float gs = (k == 0 || k == numBins - 1)
                             ? gains[ks]
                             : 0.25f * gains[ks - 1] + 0.5f * gains[ks] + 0.25f * gains[ks + 1];
        fftData[2 * ks] *= gs;
        fftData[2 * ks + 1] *= gs;
    }

    fft.performRealOnlyInverseTransform (fftData.data());

    for (int j = 0; j < fftSize; ++j)
        c.outBuf[(size_t) ((c.pos + j) & (fftSize - 1))] += fftData[(size_t) j] * window[(size_t) j] * 0.5f;
}

//==============================================================================
void MilagrosLimpioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    // Cambios del botón "Aprender ruido"
    const bool learn = learning.load();
    if (learn && ! wasLearning)
    {
        for (auto& c : chan)
        {
            c.profile.fill (0.0f);
            c.learnFrames = 0;
        }
        profileValid.store (false);
    }
    else if (! learn && wasLearning)
    {
        profileValid.store (chan[0].learnFrames >= 8);
    }
    wasLearning = learn;
    learningNow = learn;

    // Parámetros del bloque
    const float red = reductionP->load();
    reductionOn = red > 0.005f;
    alpha = 0.5f + 3.0f * red;
    floorGain = juce::Decibels::decibelsToGain (-40.0f * red);
    relCoefGain = 0.6f - 0.45f * smoothP->load();
    useProfile = modeP->load() > 0.5f;
    profileOk = profileValid.load();

    const bool gateEnabled = gateOnP->load() > 0.5f;
    const float margin = 18.0f - 12.0f * sensP->load();
    const float closedGain = juce::Decibels::decibelsToGain (-(6.0f + 54.0f * depthP->load()));
    // La puerta decide con la señal de entrada, que va 1024 muestras por delante del audio
    // limpio: así abre ANTES de la primera palabra. Al cerrar, sumamos esa misma latencia.
    const int holdSamples = (int) (holdP->load() * 0.001f * (float) sr) + fftSize;
    const float relCoef = std::exp (-1.0f / (releaseP->load() * 0.001f * (float) sr));
    const float outGain = juce::Decibels::decibelsToGain (outputP->load());

    float* ptr0 = buffer.getWritePointer (0);
    float* ptr1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float lastFloor = floorDb;

    for (int i = 0; i < n; ++i)
    {
        const float in0 = ptr0[i];
        const float in1 = ptr1 != nullptr ? ptr1[i] : 0.0f;
        const float mono = ptr1 != nullptr ? 0.5f * (in0 + in1) : in0;

        // 1) Reducción de ruido espectral
        const float y0 = stftProcess (chan[0], in0);
        const float y1 = ptr1 != nullptr ? stftProcess (chan[1], in1) : 0.0f;

        // 2) Detector de voz de la puerta
        float d = detHp.processSample (mono);
        d = detLp.processSample (d);
        env += (d * d - env) * envCoef;
        const float envDb = 10.0f * std::log10 (env + 1.0e-12f);

        // Ruido de fondo = mínimo de los últimos ~3 segundos
        curMin = juce::jmin (curMin, envDb);
        if (++subCount >= subLen)
        {
            subMin[(size_t) subIdx] = curMin;
            subIdx = (subIdx + 1) % (int) subMin.size();
            curMin = 0.0f;
            subCount = 0;
            float m = 0.0f;
            for (float v : subMin)
                m = juce::jmin (m, v);
            floorDb = m;
        }
        lastFloor = juce::jlimit (-75.0f, -20.0f, juce::jmin (floorDb, curMin));

        // Umbral automático con histéresis y tiempo de mantenimiento
        const float thrOpen = lastFloor + margin;
        const float thrClose = thrOpen - 4.0f;

        if (envDb > thrOpen)
        {
            gateOpen = true;
            holdCounter = holdSamples;
        }
        else if (gateOpen)
        {
            if (envDb > thrClose)
                holdCounter = holdSamples;
            else if (--holdCounter <= 0)
                gateOpen = false;
        }

        const float target = (! gateEnabled || gateOpen) ? 1.0f : closedGain;
        const float coef = target > gateGain ? atkCoef : relCoef;
        gateGain = coef * gateGain + (1.0f - coef) * target;

        const float g = gateGain * outGain;
        ptr0[i] = y0 * g;
        if (ptr1 != nullptr)
            ptr1[i] = y1 * g;
    }

    gateOpenFlag.store (gateOpen);
    floorShown.store (lastFloor);
}

MilagrosLimpioProcessor::Status MilagrosLimpioProcessor::getStatus() const
{
    Status s;
    s.learning = learning.load();
    s.profileValid = profileValid.load();
    s.gateOpen = gateOpenFlag.load();
    s.floorDb = floorShown.load();
    return s;
}

//==============================================================================
juce::AudioProcessorEditor* MilagrosLimpioProcessor::createEditor()
{
    return new MilagrosLimpioEditor (*this);
}

void MilagrosLimpioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MilagrosLimpioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MilagrosLimpioProcessor();
}
