// Medidor de volumen en LUFS (ponderación K, ITU-R BS.1770) con integrado "gated" (EBU R128)
#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>

class LoudnessMeter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        blockN = juce::jmax (1, (int) std::lround (0.1 * sr));   // bloques de 100 ms
        const double pi = juce::MathConstants<double>::pi;

        {   // Etapa 1: realce de agudos
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / sr);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            for (auto& f : s1)
            {
                f.b0 = (Vh + Vb * K / Q + K * K) / a0;
                f.b1 = 2.0 * (K * K - Vh) / a0;
                f.b2 = (Vh - Vb * K / Q + K * K) / a0;
                f.a1 = 2.0 * (K * K - 1.0) / a0;
                f.a2 = (1.0 - K / Q + K * K) / a0;
            }
        }
        {   // Etapa 2: filtro de graves
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (pi * f0 / sr);
            const double a0 = 1.0 + K / Q + K * K;
            for (auto& f : s2)
            {
                f.b0 = 1.0;
                f.b1 = -2.0;
                f.b2 = 1.0;
                f.a1 = 2.0 * (K * K - 1.0) / a0;
                f.a2 = (1.0 - K / Q + K * K) / a0;
            }
        }
        reset();
    }

    void reset()
    {
        for (auto& f : s1) f.clear();
        for (auto& f : s2) f.clear();
        sumSq = 0.0;
        count = 0;
        ring.fill (0.0);
        ringPos = ringCount = 0;
        binCount.fill (0);
        binEnergy.fill (0.0);
        momentaryA.store (-120.0f);
        shortTermA.store (-120.0f);
        integratedA.store (-120.0f);
        validBlocksA.store (0);
    }

    void process (float l, float r)
    {
        const double kl = s2[0].process (s1[0].process ((double) l));
        const double kr = s2[1].process (s1[1].process ((double) r));
        sumSq += kl * kl + kr * kr;
        if (++count >= blockN)
            endBlock();
    }

    float momentary() const { return momentaryA.load(); }
    float shortTerm() const { return shortTermA.load(); }
    float integrated() const { return integratedA.load(); }
    int validBlocks() const { return validBlocksA.load(); }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() { z1 = z2 = 0.0; }
    };

    static float toLufs (double e) { return e > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (e)) : -120.0f; }

    void endBlock()
    {
        const double e = sumSq / (double) blockN;
        sumSq = 0.0;
        count = 0;

        ring[(size_t) ringPos] = e;
        ringPos = (ringPos + 1) % ringSize;
        ringCount = juce::jmin (ringCount + 1, ringSize);

        auto meanLast = [this] (int n)
        {
            n = juce::jmin (n, ringCount);
            double s = 0.0;
            for (int i = 0; i < n; ++i)
                s += ring[(size_t) ((ringPos - 1 - i + ringSize * 2) % ringSize)];
            return n > 0 ? s / (double) n : 0.0;
        };

        const double mom = meanLast (4);
        momentaryA.store (toLufs (mom));
        shortTermA.store (toLufs (meanLast (ringSize)));

        if (ringCount >= 4)
        {
            const float L = toLufs (mom);
            if (L > -70.0f)
            {
                const int idx = juce::jlimit (0, numBins - 1, (int) std::floor ((L + 70.0f) * 10.0f));
                binCount[(size_t) idx] += 1;
                binEnergy[(size_t) idx] += mom;
                updateIntegrated();
            }
        }
    }

    void updateIntegrated()
    {
        double totE = 0.0;
        long totN = 0;
        for (size_t i = 0; i < (size_t) numBins; ++i)
        {
            totE += binEnergy[i];
            totN += binCount[i];
        }
        if (totN == 0)
            return;

        const double thr = 0.1 * (totE / (double) totN);   // -10 LU
        double e2 = 0.0;
        long n2 = 0;
        for (size_t i = 0; i < (size_t) numBins; ++i)
        {
            if (binCount[i] > 0 && binEnergy[i] / (double) binCount[i] >= thr)
            {
                e2 += binEnergy[i];
                n2 += binCount[i];
            }
        }
        integratedA.store (n2 > 0 ? toLufs (e2 / (double) n2) : -120.0f);
        validBlocksA.store ((int) n2);
    }

    static constexpr int ringSize = 30;      // 3 segundos
    static constexpr int numBins = 1000;     // -70 a +30 LUFS en pasos de 0,1

    double sr = 44100.0;
    int blockN = 4410, count = 0, ringPos = 0, ringCount = 0;
    double sumSq = 0.0;
    Biquad s1[2], s2[2];
    std::array<double, ringSize> ring {};
    std::array<long, numBins> binCount {};
    std::array<double, numBins> binEnergy {};
    std::atomic<float> momentaryA { -120.0f }, shortTermA { -120.0f }, integratedA { -120.0f };
    std::atomic<int> validBlocksA { 0 };
};
