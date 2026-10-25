#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Tablas según tipo de voz: masculina, femenina, rap/urbano, locución
    const float hpfHz[4]  = { 80.0f, 120.0f, 90.0f, 70.0f };
    const float mudHz[4]  = { 280.0f, 350.0f, 300.0f, 260.0f };
    const float presHz[4] = { 3500.0f, 4500.0f, 4000.0f, 3200.0f };
    // Cuánto necesita corrección el micrófono: BM800/económico, gama media, gama alta
    const float micScale[3] = { 1.0f, 0.7f, 0.4f };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MilagrosSoundProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    auto pctAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() / 100.0f; });

    auto dbAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });

    auto addPct = [&] (const char* id, const String& name, float def)
    {
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def, pctAttr));
    };

    addPct ("enhance", es ("Mejorar voz"), 0.7f);
    addPct ("clarity", es ("Claridad"), 0.6f);
    addPct ("warmth",  es ("Calidez"), 0.4f);
    addPct ("air",     es ("Aire"), 0.5f);
    addPct ("mud",     es ("Anti-barro"), 0.6f);
    addPct ("deess",   es ("De-esser"), 0.5f);
    addPct ("comp",    es ("Compresión"), 0.5f);
    addPct ("mix",     es ("Mezcla"), 1.0f);

    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, es ("Salida"), NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "voice", 1 }, es ("Tipo de voz"),
        StringArray { es ("Voz masculina"), es ("Voz femenina"), es ("Rap / Urbano"), es ("Locución") }, 0));

    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "mic", 1 }, es ("Micrófono"),
        StringArray { es ("Mic: BM800 / económico"), es ("Mic: gama media"), es ("Mic: gama alta") }, 0));

    return { params.begin(), params.end() };
}

//==============================================================================
MilagrosSoundProcessor::MilagrosSoundProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    enhanceP = apvts.getRawParameterValue ("enhance");
    clarityP = apvts.getRawParameterValue ("clarity");
    warmthP  = apvts.getRawParameterValue ("warmth");
    airP     = apvts.getRawParameterValue ("air");
    mudP     = apvts.getRawParameterValue ("mud");
    deessP   = apvts.getRawParameterValue ("deess");
    compP    = apvts.getRawParameterValue ("comp");
    mixP     = apvts.getRawParameterValue ("mix");
    outputP  = apvts.getRawParameterValue ("output");
    voiceP   = apvts.getRawParameterValue ("voice");
    micP     = apvts.getRawParameterValue ("mic");

    resetAnalysis();
}

bool MilagrosSoundProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void MilagrosSoundProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    envAtk  = std::exp (-1.0f / (0.001f * (float) sr));   // 1 ms
    envRel  = std::exp (-1.0f / (0.040f * (float) sr));   // 40 ms
    fullRel = std::exp (-1.0f / (0.050f * (float) sr));   // 50 ms

    const auto sibCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, juce::jmin (7000.0, sr * 0.45), 1.0f);
    for (int ch = 0; ch < 2; ++ch)
        sibBand[ch].coefficients = sibCoeffs;

    updateFilters (true);

    for (int ch = 0; ch < 2; ++ch)
    {
        hpf[ch].reset(); mud[ch].reset(); presence[ch].reset();
        warm[ch].reset(); air[ch].reset(); sibBand[ch].reset();
        sibEnv[ch] = fullEnv[ch] = 0.0f;
    }

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };
    compressor.prepare (spec);
    compressor.reset();

    dryBuffer.setSize (2, samplesPerBlock);
    fifoIndex = 0;
}

void MilagrosSoundProcessor::updateFilters (bool force)
{
    const int voice = juce::jlimit (0, 3, (int) voiceP->load());
    const int mic   = juce::jlimit (0, 2, (int) micP->load());
    const float enh = enhanceP->load(), cla = clarityP->load(), war = warmthP->load(),
                ai  = airP->load(), mu = mudP->load(), co = compP->load();

    const std::array<float, 8> key { (float) voice, (float) mic, enh, cla, war, ai, mu, co };
    if (! force && key == lastKey)
        return;
    lastKey = key;

    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    const float scale = micScale[mic];
    const double nyq = sr * 0.45;

    auto cHpf  = Coeffs::makeHighPass (sr, hpfHz[voice], 0.707f);
    auto cMud  = Coeffs::makePeakFilter (sr, mudHz[voice], 1.0f,
                                         juce::Decibels::decibelsToGain (-mu * enh * 6.0f * scale));
    auto cPres = Coeffs::makePeakFilter (sr, juce::jmin ((double) presHz[voice], nyq), 0.8f,
                                         juce::Decibels::decibelsToGain (cla * enh * 5.0f * scale));
    auto cWarm = Coeffs::makeLowShelf (sr, 160.0, 0.707f,
                                       juce::Decibels::decibelsToGain (war * enh * 3.0f));
    auto cAir  = Coeffs::makeHighShelf (sr, juce::jmin (10000.0, nyq), 0.707f,
                                        juce::Decibels::decibelsToGain (ai * enh * 5.0f * scale));

    for (int ch = 0; ch < 2; ++ch)
    {
        hpf[ch].coefficients = cHpf;
        mud[ch].coefficients = cMud;
        presence[ch].coefficients = cPres;
        warm[ch].coefficients = cWarm;
        air[ch].coefficients = cAir;
    }

    const float amt = co * enh;
    compressor.setThreshold (-10.0f - 14.0f * amt);
    compressor.setRatio (1.0f + 3.0f * amt);
    compressor.setAttack (12.0f);
    compressor.setRelease (120.0f);
}

//==============================================================================
void MilagrosSoundProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    analyse (buffer);   // el Asesor escucha la señal de entrada
    updateFilters (false);

    dryBuffer.setSize (numCh, n, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

    const float enh = enhanceP->load();
    const float deessAmt = deessP->load() * enh;
    const float deessThr = 0.9f - 0.6f * deessAmt;

    // 1) Corte de graves + EQ + De-esser
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float x = hpf[ch].processSample (d[i]);
            x = mud[ch].processSample (x);
            x = presence[ch].processSample (x);
            x = warm[ch].processSample (x);
            x = air[ch].processSample (x);

            // De-esser por banda: resta solo las "s" cuando sobresalen
            const float sib = sibBand[ch].processSample (x);
            const float a = std::abs (sib), f = std::abs (x);
            sibEnv[ch]  = a > sibEnv[ch]  ? envAtk * sibEnv[ch]  + (1.0f - envAtk) * a
                                          : envRel * sibEnv[ch]  + (1.0f - envRel) * a;
            fullEnv[ch] = f > fullEnv[ch] ? envAtk * fullEnv[ch] + (1.0f - envAtk) * f
                                          : fullRel * fullEnv[ch] + (1.0f - fullRel) * f;

            if (deessAmt > 0.01f)
            {
                const float ratio = sibEnv[ch] / (fullEnv[ch] + 1.0e-6f);
                if (ratio > deessThr)
                    x -= sib * (1.0f - deessThr / ratio);
            }
            d[i] = x;
        }
    }

    // 2) Compresor suave
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) numCh);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        compressor.process (ctx);
    }

    // 3) Saturación ligera + mezcla + salida
    const float drive = 1.0f + warmthP->load() * enh * 2.0f;
    const float makeup = juce::Decibels::decibelsToGain (compP->load() * enh * 4.0f);
    const float mix = mixP->load();
    const float outGain = juce::Decibels::decibelsToGain (outputP->load());

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        const auto* dry = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float wet = d[i] * makeup;
            if (drive > 1.001f)
                wet = std::tanh (drive * wet) / drive;
            d[i] = (dry[i] * (1.0f - mix) + wet * mix) * outGain;
        }
    }
}

//==============================================================================
// ASESOR: promedia el espectro de la voz de entrada en bandas
void MilagrosSoundProcessor::analyse (const juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int chs = juce::jmin (buffer.getNumChannels(), 2);

    float pk = 0.0f;
    for (int ch = 0; ch < chs; ++ch)
        pk = juce::jmax (pk, buffer.getMagnitude (ch, 0, n));
    if (pk > peak.load())
        peak.store (pk);

    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int ch = 0; ch < chs; ++ch)
            s += buffer.getReadPointer (ch)[i];
        fifo[(size_t) fifoIndex++] = s / (float) chs;

        if (fifoIndex == fftSize)
        {
            processAnalysisFrame();
            fifoIndex = 0;
        }
    }
}

void MilagrosSoundProcessor::processAnalysisFrame()
{
    float energy = 0.0f;
    for (float s : fifo)
        energy += s * s;
    const float rms = std::sqrt (energy / (float) fftSize);
    if (rms < 0.003f)      // silencio (aprox. -50 dBFS): no se analiza
        return;

    std::copy (fifo.begin(), fifo.end(), fftData.begin());
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const int frames = frameCount.load();
    const float a = frames == 0 ? 1.0f : 0.05f;   // suavizado lento

    for (int b = 0; b < numBands; ++b)
    {
        const int lo = (int) std::ceil (bandLo[b] * (float) fftSize / (float) sr);
        const int hi = juce::jmin ((int) std::floor (bandHi[b] * (float) fftSize / (float) sr), fftSize / 2 - 1);

        float sum = 0.0f;
        int cnt = 0;
        for (int k = lo; k <= hi; ++k)
        {
            sum += fftData[(size_t) k] * fftData[(size_t) k];
            ++cnt;
        }
        const float p = cnt > 0 ? sum / (float) cnt : 1.0e-12f;
        const float old = bandPower[b].load();
        bandPower[b].store (old + a * (p - old));
    }

    const float oldRms = rmsAvg.load();
    rmsAvg.store (oldRms + a * (rms - oldRms));
    frameCount.store (frames + 1);
}

MilagrosSoundProcessor::Analysis MilagrosSoundProcessor::getAnalysis() const
{
    auto pdb = [] (float p) { return 10.0f * std::log10 (p + 1.0e-20f); };

    Analysis a;
    a.frames     = frameCount.load();
    a.lowDb      = pdb (bandPower[0].load());
    a.mudDb      = pdb (bandPower[1].load());
    a.midDb      = pdb (bandPower[2].load());
    a.presenceDb = pdb (bandPower[3].load());
    a.sibDb      = pdb (bandPower[4].load());
    a.airDb      = pdb (bandPower[5].load());
    a.peakDb     = juce::Decibels::gainToDecibels (peak.load());
    a.rmsDb      = juce::Decibels::gainToDecibels (rmsAvg.load());
    return a;
}

void MilagrosSoundProcessor::resetAnalysis()
{
    for (auto& b : bandPower)
        b.store (0.0f);
    rmsAvg.store (0.0f);
    peak.store (0.0f);
    frameCount.store (0);
}

//==============================================================================
juce::AudioProcessorEditor* MilagrosSoundProcessor::createEditor()
{
    return new MilagrosSoundEditor (*this);
}

void MilagrosSoundProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MilagrosSoundProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MilagrosSoundProcessor();
}
