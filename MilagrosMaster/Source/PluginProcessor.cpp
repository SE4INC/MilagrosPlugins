#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MilagrosMasterProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    auto pctAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() / 100.0f; });
    auto dbAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });

    auto addDb = [&] (const char* id, const String& name, float lo, float hi, float def)
    {
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), def, dbAttr));
    };
    auto addPct = [&] (const char* id, const String& name, float hi, float def)
    {
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, hi, 0.001f), def, pctAttr));
    };

    addDb  ("trim",    es ("Entrada"), -12.0f, 12.0f, 0.0f);
    addDb  ("lows",    es ("Graves"), -6.0f, 6.0f, 0.0f);
    addDb  ("bright",  es ("Brillo"), -6.0f, 6.0f, 0.0f);
    addPct ("glue",    es ("Pegamento"), 1.0f, 0.4f);
    addPct ("width",   es ("Ancho"), 2.0f, 1.0f);
    addPct ("warm",    es ("Calidez"), 1.0f, 0.25f);
    addDb  ("drive",   es ("Intensidad"), 0.0f, 18.0f, 4.0f);
    addDb  ("ceiling", es ("Techo"), -3.0f, -0.1f, -1.0f);

    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bassMono", 1 }, es ("Graves en mono"), true));
    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "compare", 1 }, es ("Comparar con el original"), false));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "target", 1 }, es ("Objetivo de volumen"),
        StringArray { es ("Streaming (-14 LUFS)"), es ("Redes sociales (-12 LUFS)"), es ("Fuerte urbano (-9 LUFS)"),
                      es ("Club / máximo (-8 LUFS)"), es ("Suave / acústico (-16 LUFS)") }, 0));

    return { params.begin(), params.end() };
}

//==============================================================================
MilagrosMasterProcessor::MilagrosMasterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    trimP     = apvts.getRawParameterValue ("trim");
    lowsP     = apvts.getRawParameterValue ("lows");
    brightP   = apvts.getRawParameterValue ("bright");
    glueP     = apvts.getRawParameterValue ("glue");
    widthP    = apvts.getRawParameterValue ("width");
    warmP     = apvts.getRawParameterValue ("warm");
    driveP    = apvts.getRawParameterValue ("drive");
    ceilingP  = apvts.getRawParameterValue ("ceiling");
    bassMonoP = apvts.getRawParameterValue ("bassMono");
    compareP  = apvts.getRawParameterValue ("compare");
}

bool MilagrosMasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void MilagrosMasterProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    meterIn.prepare (sr);
    meterOut.prepare (sr);
    limiter.prepare (sr);
    setLatencySamples (limiter.latency());

    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    for (int ch = 0; ch < 2; ++ch)
        hpf[ch].coefficients = Coeffs::makeHighPass (sr, 25.0, 0.707f);
    sideHp.coefficients = Coeffs::makeHighPass (sr, 150.0, 0.707f);
    lowLp.coefficients = Coeffs::makeLowPass (sr, 120.0, 0.707f);

    avgCoef = 1.0f - std::exp (-1.0f / (2.0f * (float) sr));       // promedio de 2 s
    atkCoef = std::exp (-1.0f / (0.030f * (float) sr));            // ataque 30 ms
    relCoef = std::exp (-1.0f / (0.150f * (float) sr));            // liberación 150 ms
    limRelCoef = std::exp (-1.0f / (0.100f * (float) sr));         // limitador: 100 ms
    meterCoef = 1.0 - std::exp (-1.0 / (0.3 * sr));                // medidores de estéreo: 300 ms

    dryL.assign ((size_t) limiter.latency() + 1, 0.0f);
    dryR.assign ((size_t) limiter.latency() + 1, 0.0f);

    lastLows = lastBright = 1000.0f;
    updateEq();
    resetInternal();
}

void MilagrosMasterProcessor::resetInternal()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        hpf[ch].reset();
        lowShelf[ch].reset();
        highShelf[ch].reset();
    }
    sideHp.reset();
    lowLp.reset();
    limiter.reset();
    meterIn.reset();
    meterOut.reset();

    compGrDb = 0.0f;
    avgPow = 1.0e-4f;
    sLR = sLL = sRR = sMM = sSS = lowE = totE = 0.0;
    std::fill (dryL.begin(), dryL.end(), 0.0f);
    std::fill (dryR.begin(), dryR.end(), 0.0f);
    dryPos = 0;
    matchDb = 0.0f;
    heavySamples = totalSamples = 0.0;

    inPeakLin.store (0.0f);
    outPeakLin.store (0.0f);
    corrA.store (1.0f);
    sideMidA.store (-60.0f);
    lowRatioA.store (0.0f);
    limGrA.store (0.0f);
    limGrMaxA.store (0.0f);
    heavyFracA.store (0.0f);
    matchA.store (0.0f);
}

void MilagrosMasterProcessor::updateEq()
{
    const float lows = lowsP->load(), bright = brightP->load();
    if (lows == lastLows && bright == lastBright)
        return;
    lastLows = lows;
    lastBright = bright;

    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    auto cLow = Coeffs::makeLowShelf (sr, 100.0, 0.707f, juce::Decibels::decibelsToGain (lows));
    auto cHigh = Coeffs::makeHighShelf (sr, juce::jmin (9000.0, sr * 0.45), 0.707f, juce::Decibels::decibelsToGain (bright));
    for (int ch = 0; ch < 2; ++ch)
    {
        lowShelf[ch].coefficients = cLow;
        highShelf[ch].coefficients = cHigh;
    }
}

//==============================================================================
void MilagrosMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    // Reinicio de medidores: botón o inicio de reproducción desde el comienzo del tema
    bool doReset = resetRequested.exchange (false);
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            const bool playing = pos->getIsPlaying();
            if (playing && ! wasPlaying)
                if (auto t = pos->getTimeInSamples())
                    if (*t < (juce::int64) (sr * 0.5))
                        doReset = true;
            wasPlaying = playing;
        }
    }
    if (doReset)
        resetInternal();

    updateEq();

    const float trimG = juce::Decibels::decibelsToGain (trimP->load());
    const float driveG = juce::Decibels::decibelsToGain (driveP->load());
    const float ceilingLin = juce::Decibels::decibelsToGain (ceilingP->load());
    const float glue = glueP->load();
    const float ratio = 1.0f + 2.0f * glue;
    const bool compOn = glue > 0.01f;
    const float warm = warmP->load();
    const float satDrive = 1.0f + warm * 1.5f;
    const float width = widthP->load();
    const bool bassMono = bassMonoP->load() > 0.5f;
    const bool compare = compareP->load() > 0.5f;
    const float matchLin = juce::Decibels::decibelsToGain (matchDb);
    const int dryLen = (int) dryL.size();

    float* p0 = buffer.getWritePointer (0);
    float* p1 = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    float blockInPk = 0.0f, blockOutPk = 0.0f, blockMinGr = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float inL = p0[i];
        const float inR = p1 != nullptr ? p1[i] : inL;

        meterIn.process (inL, inR);
        blockInPk = juce::jmax (blockInPk, juce::jmax (std::abs (inL), std::abs (inR)));

        // Original retrasado para comparar sin saltos de tiempo
        dryL[(size_t) dryPos] = inL;
        dryR[(size_t) dryPos] = inR;
        const int rd = (dryPos + 1) % dryLen;
        const float dL = dryL[(size_t) rd], dR = dryR[(size_t) rd];
        dryPos = (dryPos + 1) % dryLen;

        float l = inL * trimG;
        float r = inR * trimG;

        // 1) Filtro de graves infrasónicos + EQ
        l = highShelf[0].processSample (lowShelf[0].processSample (hpf[0].processSample (l)));
        r = highShelf[1].processSample (lowShelf[1].processSample (hpf[1].processSample (r)));

        // 2) Compresor "pegamento" (detector enlazado: no mueve la imagen estéreo)
        {
            avgPow += (0.5f * (l * l + r * r) - avgPow) * avgCoef;
            float targetGr = 0.0f;
            if (compOn)
            {
                const float lvlDb = 20.0f * std::log10 (juce::jmax (std::abs (l), std::abs (r)) + 1.0e-9f);
                const float thr = 10.0f * std::log10 (avgPow + 1.0e-12f) + 6.0f - 2.0f * glue;
                const float over = lvlDb - thr;
                const float knee = 6.0f;
                const float slope = 1.0f - 1.0f / ratio;
                if (2.0f * over < -knee)
                    targetGr = 0.0f;
                else if (2.0f * std::abs (over) <= knee)
                    targetGr = -slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
                else
                    targetGr = -slope * over;
            }
            const float c = targetGr < compGrDb ? atkCoef : relCoef;
            compGrDb = c * compGrDb + (1.0f - c) * targetGr;
            const float g = juce::Decibels::decibelsToGain (compGrDb);
            l *= g;
            r *= g;
        }

        // 3) Calidez (saturación suave)
        if (warm > 0.001f)
        {
            const float wl = std::tanh (satDrive * l) / satDrive;
            const float wr = std::tanh (satDrive * r) / satDrive;
            l = l * (1.0f - warm) + wl * warm;
            r = r * (1.0f - warm) + wr * warm;
        }

        // 4) Ancho estéreo (con los graves siempre centrados)
        {
            const float mid = 0.5f * (l + r);
            float side = 0.5f * (l - r);
            if (bassMono)
                side = sideHp.processSample (side);
            side *= width;
            l = mid + side;
            r = mid - side;
        }

        // 5) Intensidad + limitador
        l *= driveG;
        r *= driveG;
        limiter.process (l, r, ceilingLin, limRelCoef);

        const float gS = limiter.getGain();
        const float grDb = 20.0f * std::log10 (juce::jmax (gS, 1.0e-6f));
        blockMinGr = juce::jmin (blockMinGr, grDb);
        if (juce::jmax (std::abs (inL), std::abs (inR)) > 0.001f)
        {
            totalSamples += 1.0;
            if (gS < 0.708f)
                heavySamples += 1.0;
        }

        // Medidores de la señal de salida
        meterOut.process (l, r);
        blockOutPk = juce::jmax (blockOutPk, juce::jmax (std::abs (l), std::abs (r)));
        {
            sLR += ((double) l * r - sLR) * meterCoef;
            sLL += ((double) l * l - sLL) * meterCoef;
            sRR += ((double) r * r - sRR) * meterCoef;
            const double m = 0.5 * ((double) l + r), s = 0.5 * ((double) l - r);
            sMM += (m * m - sMM) * meterCoef;
            sSS += (s * s - sSS) * meterCoef;
            const float lowSig = lowLp.processSample ((float) m);
            lowE += ((double) lowSig * lowSig - lowE) * meterCoef;
            totE += (m * m - totE) * meterCoef;
        }

        // 6) Comparar: el original, con el mismo volumen que el master
        if (compare)
        {
            l = dL * matchLin;
            r = dR * matchLin;
        }

        p0[i] = l;
        if (p1 != nullptr)
            p1[i] = r;
    }

    // Igualar volumen para la comparación (suave, por bloques)
    {
        const float o = meterOut.shortTerm(), in = meterIn.shortTerm();
        const float tgt = (o > -100.0f && in > -100.0f) ? juce::jlimit (-24.0f, 24.0f, o - in) : 0.0f;
        matchDb += (tgt - matchDb) * 0.2f;
    }

    // Datos para la interfaz
    if (blockInPk > inPeakLin.load())
        inPeakLin.store (blockInPk);
    if (blockOutPk > outPeakLin.load())
        outPeakLin.store (blockOutPk);
    corrA.store ((float) (sLR / std::sqrt (sLL * sRR + 1.0e-20)));
    sideMidA.store ((float) (10.0 * std::log10 ((sSS + 1.0e-12) / (sMM + 1.0e-12))));
    lowRatioA.store ((float) (lowE / (totE + 1.0e-12)));
    limGrA.store (blockMinGr);
    if (blockMinGr < limGrMaxA.load())
        limGrMaxA.store (blockMinGr);
    heavyFracA.store (totalSamples > 0.0 ? (float) (heavySamples / totalSamples) : 0.0f);
    matchA.store (matchDb);
}

//==============================================================================
MilagrosMasterProcessor::Meters MilagrosMasterProcessor::getMeters() const
{
    Meters m;
    m.momentary = meterOut.momentary();
    m.shortTerm = meterOut.shortTerm();
    m.integrated = meterOut.integrated();
    m.validBlocks = meterOut.validBlocks();
    m.inPeakDb = juce::Decibels::gainToDecibels (inPeakLin.load(), -120.0f);
    m.outPeakDb = juce::Decibels::gainToDecibels (outPeakLin.load(), -120.0f);
    m.corr = corrA.load();
    m.sideMidDb = sideMidA.load();
    m.lowRatio = lowRatioA.load();
    m.limGrDb = limGrA.load();
    m.limGrMaxDb = limGrMaxA.load();
    m.heavyFrac = heavyFracA.load();
    m.matchDb = matchA.load();
    return m;
}

juce::AudioProcessorEditor* MilagrosMasterProcessor::createEditor()
{
    return new MilagrosMasterEditor (*this);
}

void MilagrosMasterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MilagrosMasterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MilagrosMasterProcessor();
}
