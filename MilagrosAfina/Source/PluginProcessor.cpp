#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MilagrosAfinaProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    StringArray keys;
    for (int i = 0; i < 12; ++i)
        keys.add (afina::noteNames (i));

    auto pctAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() / 100.0f; });
    auto msAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });
    auto hzAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " Hz"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });
    auto dbAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " dB"; })
        .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });

    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, es ("Tonalidad"), keys, 0));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, es ("Escala"), afina::scaleNames(), 1));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, es ("Velocidad"),
                                                             NormalisableRange<float> (0.0f, 400.0f, 1.0f), 60.0f, msAttr));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, es ("Cantidad"),
                                                             NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f, pctAttr));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "refA", 1 }, es ("Afinación base (La)"),
                                                             NormalisableRange<float> (430.0f, 450.0f, 0.1f), 440.0f, hzAttr));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "thresh", 1 }, es ("Umbral de voz"),
                                                             NormalisableRange<float> (-70.0f, -30.0f, 1.0f), -50.0f, dbAttr));

    return { params.begin(), params.end() };
}

//==============================================================================
MilagrosAfinaProcessor::MilagrosAfinaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    keyP    = apvts.getRawParameterValue ("key");
    scaleP  = apvts.getRawParameterValue ("scale");
    speedP  = apvts.getRawParameterValue ("speed");
    amountP = apvts.getRawParameterValue ("amount");
    refAP   = apvts.getRawParameterValue ("refA");
    threshP = apvts.getRawParameterValue ("thresh");

    for (auto& d : dispIn)  d.store (-1.0f);
    for (auto& d : dispOut) d.store (-1.0f);
    resetAnalysis();
}

bool MilagrosAfinaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;

    const auto& sc = layouts.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void MilagrosAfinaProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    detector.prepare (sr);
    shifter[0].prepare (sr);
    shifter[1].prepare (sr);
    keyInst.prepare (sr);
    setLatencySamples (shifter[0].latencySamples());   // FL Studio compensa esta latencia

    hopSeconds = detector.hopSeconds();
    ratioCoef = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));
    ratioTarget = ratioSm = 1.0f;
    corrCents = 0.0f;
    periodSm = (float) (sr / 150.0);
    f0Count = f0Pos = 0;
    targetValid = false;
    unvoicedFrames = 100;
    jumpCount = 0;
    voicedFlag = false;
}

//==============================================================================
float MilagrosAfinaProcessor::medianF0() const
{
    std::array<float, 5> tmp {};
    const int n = juce::jlimit (1, 5, f0Count);
    for (int i = 0; i < n; ++i)
        tmp[(size_t) i] = f0Ring[(size_t) i];
    std::sort (tmp.begin(), tmp.begin() + n);
    return tmp[(size_t) (n / 2)];
}

void MilagrosAfinaProcessor::onPitch()
{
    const auto& r = detector.getResult();
    const int root = juce::jlimit (0, 11, (int) keyP->load());
    const int scale = juce::jlimit (0, afina::numScales - 1, (int) scaleP->load());
    const float refA = refAP->load();

    float inMidi = -1.0f, outMidi = -1.0f, tgtMidi = -1.0f;

    if (r.voiced)
    {
        // Filtro anti-saltos de octava: un salto grande debe repetirse 3 veces para aceptarse
        if (f0Count >= 3)
        {
            const float pm = medianF0();
            if (r.f0 > pm * 1.3f || r.f0 < pm / 1.3f)
            {
                if (++jumpCount < 3)
                    return;
                f0Count = f0Pos = 0;
                jumpCount = 0;
            }
            else
            {
                jumpCount = 0;
            }
        }

        f0Ring[(size_t) f0Pos] = r.f0;
        f0Pos = (f0Pos + 1) % 5;
        f0Count = juce::jmin (f0Count + 1, 5);
        const float f0 = medianF0();

        unvoicedFrames = 0;
        voicedFlag = true;

        const float midi = 69.0f + 12.0f * std::log2 (f0 / refA);

        // Nota objetivo con histéresis (evita que "salte" entre dos notas)
        const int nearest = afina::nearestAllowed (midi, scale, root);
        if (! targetValid || ! afina::inScale (scale, root, ((target % 12) + 12) % 12))
        {
            target = nearest;
            targetValid = true;
        }
        else if (nearest != target && std::abs (midi - (float) nearest) + 0.2f < std::abs (midi - (float) target))
        {
            target = nearest;
        }

        const float errCents = ((float) target - midi) * 100.0f;

        // Velocidad: tiempo que tarda en llegar a la nota (0 ms = efecto robótico)
        const float tau = speedP->load() * 0.001f;
        const float alpha = tau < 0.0005f ? 1.0f : 1.0f - std::exp (-(float) hopSeconds / tau);
        corrCents += alpha * (errCents * amountP->load() - corrCents);
        ratioTarget = std::pow (2.0f, corrCents / 1200.0f);

        const float period = (float) sr / f0;
        periodSm += 0.3f * (period - periodSm);

        inMidi = midi;
        outMidi = midi + corrCents / 100.0f;
        tgtMidi = (float) target;

        // Estadísticas para el Asesor
        voicedFrames.fetch_add (1, std::memory_order_relaxed);
        if (std::abs (errCents) <= 25.0f)
            inTuneFrames.fetch_add (1, std::memory_order_relaxed);
        const int mi = juce::jlimit (0, 127, (int) std::lround (midi));
        pitchHist[mi].fetch_add (1, std::memory_order_relaxed);
        voiceCount[((mi % 12) + 12) % 12].fetch_add (1, std::memory_order_relaxed);
        avgConf.store (avgConf.load() + 0.01f * (r.confidence - avgConf.load()));
        avgLevel.store (avgLevel.load() + 0.01f * (r.rmsDb - avgLevel.load()));
    }
    else
    {
        ++unvoicedFrames;
        if (unvoicedFrames > 6)
        {
            corrCents *= 0.9f;
            ratioTarget = std::pow (2.0f, corrCents / 1200.0f);
        }
        if (unvoicedFrames > 40)
        {
            targetValid = false;
            f0Count = f0Pos = 0;
        }
        voicedFlag = unvoicedFrames < 12;
    }

    liveIn.store (inMidi);
    liveTarget.store (tgtMidi);

    if (++dispCounter >= 3)
    {
        dispCounter = 0;
        const int i = dispIdx.load();
        dispIn[i].store (inMidi);
        dispOut[i].store (outMidi);
        dispIdx.store ((i + 1) % dispSize);
    }
}

//==============================================================================
void MilagrosAfinaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBuf = getBusBuffer (buffer, true, 0);
    auto sideBuf = getBusBuffer (buffer, true, 1);

    const int numCh = juce::jmin (mainBuf.getNumChannels(), 2);
    const int n = mainBuf.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    detector.setLevelThresholdDb (threshP->load());

    // Instrumental (sidechain): solo para detectar su tonalidad
    const int scCh = sideBuf.getNumChannels();
    if (scCh > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < scCh; ++ch)
                s += sideBuf.getReadPointer (ch)[i];
            keyInst.pushSample (s / (float) scCh);
        }
    }

    float pk = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        pk = juce::jmax (pk, mainBuf.getMagnitude (ch, 0, n));
    if (pk > peakLin.load())
        peakLin.store (pk);

    float* p0 = mainBuf.getWritePointer (0);
    float* p1 = numCh > 1 ? mainBuf.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float in0 = p0[i];
        const float in1 = p1 != nullptr ? p1[i] : 0.0f;
        const float mono = p1 != nullptr ? 0.5f * (in0 + in1) : in0;

        if (detector.pushSample (mono))
            onPitch();

        ratioSm += (ratioTarget - ratioSm) * ratioCoef;

        p0[i] = shifter[0].process (in0, ratioSm, periodSm, voicedFlag);
        if (p1 != nullptr)
            p1[i] = shifter[1].process (in1, ratioSm, periodSm, voicedFlag);
    }
}

//==============================================================================
MilagrosAfinaProcessor::Live MilagrosAfinaProcessor::getLive() const
{
    Live l;
    l.inMidi = liveIn.load();
    l.targetMidi = liveTarget.load();
    l.voiced = l.inMidi > 0.0f;
    return l;
}

void MilagrosAfinaProcessor::copyDisplay (std::array<float, dispSize>& in, std::array<float, dispSize>& out) const
{
    const int start = dispIdx.load();
    for (int i = 0; i < dispSize; ++i)
    {
        const int k = (start + i) % dispSize;
        in[(size_t) i] = dispIn[k].load();
        out[(size_t) i] = dispOut[k].load();
    }
}

MilagrosAfinaProcessor::Analysis MilagrosAfinaProcessor::getAnalysis() const
{
    Analysis a;
    a.voicedFrames = voicedFrames.load();
    a.inTuneFrames = inTuneFrames.load();
    a.avgConfidence = avgConf.load();
    a.avgLevelDb = avgLevel.load();
    a.peakDb = juce::Decibels::gainToDecibels (peakLin.load());
    for (size_t i = 0; i < 128; ++i)
        a.pitchHist[i] = pitchHist[i].load();
    for (size_t i = 0; i < 12; ++i)
        a.voiceChroma[i] = (float) voiceCount[i].load();
    a.instChroma = keyInst.getChroma();
    a.instFrames = keyInst.getFrames();
    return a;
}

void MilagrosAfinaProcessor::resetAnalysis()
{
    voicedFrames.store (0);
    inTuneFrames.store (0);
    avgConf.store (0.0f);
    avgLevel.store (-100.0f);
    peakLin.store (0.0f);
    for (auto& h : pitchHist)
        h.store (0);
    for (auto& v : voiceCount)
        v.store (0);
    keyInst.reset();
}

//==============================================================================
juce::AudioProcessorEditor* MilagrosAfinaProcessor::createEditor()
{
    return new MilagrosAfinaEditor (*this);
}

void MilagrosAfinaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MilagrosAfinaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MilagrosAfinaProcessor();
}
