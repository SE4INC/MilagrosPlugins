// Limitador de picos con anticipación (lookahead) de 3 ms.
// Mínimo en ventana + media móvil: la ganancia baja ANTES de que llegue el pico,
// sin recortar. La recuperación (release) es lenta para no distorsionar.
#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <vector>

class LookaheadLimiter
{
public:
    void prepare (double sampleRate)
    {
        L = juce::jmax (8, (int) std::lround (0.003 * sampleRate));
        dl.assign ((size_t) L, 0.0f);
        dr.assign ((size_t) L, 0.0f);
        reqRing.assign ((size_t) L, 1.0f);
        minRing.assign ((size_t) L, 1.0f);
        reset();
    }

    void reset()
    {
        std::fill (dl.begin(), dl.end(), 0.0f);
        std::fill (dr.begin(), dr.end(), 0.0f);
        std::fill (reqRing.begin(), reqRing.end(), 1.0f);
        std::fill (minRing.begin(), minRing.end(), 1.0f);
        pos = 0;
        sumMin = (double) L;
        gS = 1.0f;
    }

    int latency() const { return L - 1; }
    float getGain() const { return gS; }

    void process (float& l, float& r, float ceiling, float relCoef)
    {
        const float peak = juce::jmax (std::abs (l), std::abs (r));
        const float req = peak > ceiling ? ceiling / peak : 1.0f;

        reqRing[(size_t) pos] = req;
        float m = 1.0f;
        for (float v : reqRing)
            m = juce::jmin (m, v);

        sumMin += (double) m - (double) minRing[(size_t) pos];
        minRing[(size_t) pos] = m;
        const float avg = (float) (sumMin / (double) L);

        if (avg < gS)
            gS = avg;
        else
            gS = avg + (gS - avg) * relCoef;

        dl[(size_t) pos] = l;
        dr[(size_t) pos] = r;
        const int rd = (pos + 1) % L;
        l = juce::jlimit (-ceiling, ceiling, dl[(size_t) rd] * gS);
        r = juce::jlimit (-ceiling, ceiling, dr[(size_t) rd] * gS);
        pos = (pos + 1) % L;
    }

private:
    int L = 144, pos = 0;
    double sumMin = 144.0;
    float gS = 1.0f;
    std::vector<float> dl, dr, reqRing, minRing;
};
