#pragma once
#include <juce_dsp/juce_dsp.h>

// Coeficientes de toda la cadena de EQ (se usan en el audio y en la curva de la interfaz)
struct EqSettings
{
    float hpf = 80, lowF = 150, lowG = 0, mudF = 350, mudG = 0, presF = 3000, presG = 0, airF = 10000, airG = 0, tilt = 0;
};

struct EqCoeffs
{
    using C = juce::dsp::IIR::Coefficients<float>;
    C::Ptr hpf1, hpf2, low, mud, pres, air, tiltLow, tiltHigh;

    static EqCoeffs make (double sr, const EqSettings& s)
    {
        const float ny = (float) sr * 0.45f;
        auto g = [] (float db) { return juce::Decibels::decibelsToGain (db); };
        EqCoeffs c;
        c.hpf1 = C::makeHighPass (sr, juce::jmin (s.hpf, ny), 0.54f);
        c.hpf2 = C::makeHighPass (sr, juce::jmin (s.hpf, ny), 1.31f);   // Butterworth 4.o orden (24 dB/oct)
        c.low  = C::makeLowShelf (sr, juce::jmin (s.lowF, ny), 0.7f, g (s.lowG));
        c.mud  = C::makePeakFilter (sr, juce::jmin (s.mudF, ny), 1.2f, g (s.mudG));
        c.pres = C::makePeakFilter (sr, juce::jmin (s.presF, ny), 0.9f, g (s.presG));
        c.air  = C::makeHighShelf (sr, juce::jmin (s.airF, ny), 0.7f, g (s.airG));
        c.tiltLow  = C::makeLowShelf (sr, 300.0f, 0.5f, g (-s.tilt * 0.5f));
        c.tiltHigh = C::makeHighShelf (sr, juce::jmin (2500.0f, ny), 0.5f, g (s.tilt * 0.5f));
        return c;
    }

    double magnitudeDb (double freq, double sr) const
    {
        double m = 1.0;
        for (auto* p : { hpf1.get(), hpf2.get(), low.get(), mud.get(), pres.get(), air.get(), tiltLow.get(), tiltHigh.get() })
            if (p != nullptr) m *= p->getMagnitudeForFrequency (freq, sr);
        return juce::Decibels::gainToDecibels (m, -60.0);
    }
};
