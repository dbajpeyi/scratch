#include "DefaultSample.h"
#include "Riaa.h"

using namespace juce;

namespace
{
    constexpr double sr = 44100.0;
    constexpr double twoPi = MathConstants<double>::twoPi;

    float envelope (double t, double attack, double length, double release)
    {
        if (t < 0.0 || t > length) return 0.0f;
        if (t < attack) return (float) (t / attack);
        if (t > length - release) return (float) ((length - t) / release);
        return 1.0f;
    }

    /** A sung "ahh": additive glottal source through three vowel formants. */
    void addAhh (AudioBuffer<float>& out, double start, double length)
    {
        const int s0 = (int) (start * sr), n = (int) (length * sr);
        std::vector<double> src ((size_t) n);
        double phase = 0.0;
        Random rng (7);

        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double f0 = (148.0 - 22.0 * (t / length)) * (1.0 + 0.012 * std::sin (twoPi * 5.2 * t) * jmin (1.0, t * 3.0));
            phase += f0 / sr;
            double v = 0.0;
            for (int k = 1; k * f0 < 5500.0; ++k)
                v += std::sin (twoPi * k * phase) / std::pow ((double) k, 1.1);
            src[(size_t) i] = v + 0.08 * (rng.nextDouble() * 2.0 - 1.0);
        }

        struct Formant { double f, q, g; };
        const Formant formants[] = { { 730, 8.0, 1.0 }, { 1090, 10.0, 0.55 }, { 2440, 14.0, 0.28 }, { 3400, 16.0, 0.12 } };
        std::vector<double> voiced ((size_t) n, 0.0);
        for (auto& f : formants)
        {
            Biquad bp = Riaa::makeBandPass (f.f, f.q, sr);
            for (int i = 0; i < n; ++i)
                voiced[(size_t) i] += f.g * bp.process (src[(size_t) i]);
        }

        for (int i = 0; i < n; ++i)
        {
            const float e = envelope (i / sr, 0.035, length, 0.15);
            const float v = (float) voiced[(size_t) i] * e * 0.55f;
            out.addSample (0, s0 + i, v);
            out.addSample (1, s0 + i, v);
        }
    }

    /** A brassy major-chord stab, the classic thing to cut up. */
    void addStab (AudioBuffer<float>& out, double start, double length)
    {
        const int s0 = (int) (start * sr), n = (int) (length * sr);
        const double notes[] = { 233.08, 293.66, 349.23, 466.16 };
        double phases[4] = {};

        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double bright = 1.0 + 14.0 * std::exp (-t * 9.0);
            const double drop = t > length - 0.08 ? 1.0 - 0.06 * (t - (length - 0.08)) / 0.08 : 1.0;
            double v = 0.0;
            for (int nIdx = 0; nIdx < 4; ++nIdx)
            {
                const double f = notes[nIdx] * drop * (1.0 + 0.003 * (nIdx - 1.5));
                phases[nIdx] += f / sr;
                for (int k = 1; k * f < 9000.0; ++k)
                {
                    const double roll = 1.0 / (1.0 + std::pow (k * f / (220.0 * bright), 2.0));
                    v += std::sin (twoPi * k * phases[nIdx]) / k * roll;
                }
            }
            const float e = envelope (t, 0.012, length, 0.06);
            out.addSample (0, s0 + i, (float) v * e * 0.16f);
            out.addSample (1, s0 + i, (float) v * e * 0.16f);
        }
    }

    void addKick (AudioBuffer<float>& out, double start)
    {
        const int s0 = (int) (start * sr), n = (int) (0.35 * sr);
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double f = 46.0 + 110.0 * std::exp (-t * 28.0);
            phase += f / sr;
            const float v = (float) (std::sin (twoPi * phase) * std::exp (-t * 9.0)
                                     + 0.3 * std::exp (-t * 400.0) * std::sin (twoPi * 1800.0 * t));
            out.addSample (0, s0 + i, v * 0.8f);
            out.addSample (1, s0 + i, v * 0.8f);
        }
    }

    void addSnare (AudioBuffer<float>& out, double start)
    {
        const int s0 = (int) (start * sr), n = (int) (0.3 * sr);
        Random rng (11);
        Biquad bp = Riaa::makeBandPass (3500.0, 0.7, sr);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double noise = bp.process (rng.nextDouble() * 2.0 - 1.0) * std::exp (-t * 16.0);
            const double body = std::sin (twoPi * 190.0 * t) * std::exp (-t * 30.0);
            const float v = (float) (noise * 1.4 + body * 0.5);
            out.addSample (0, s0 + i, v * 0.7f);
            out.addSample (1, s0 + i, v * 0.7f);
        }
    }
}

RecordData::Ptr createDefaultRecord()
{
    AudioBuffer<float> audio (2, (int) (2.6 * sr));
    audio.clear();
    addAhh (audio, 0.02, 0.85);
    addStab (audio, 1.05, 0.42);
    addKick (audio, 1.65);
    addSnare (audio, 2.05);

    const float peak = audio.getMagnitude (0, audio.getNumSamples());
    if (peak > 0.0f)
        audio.applyGain (0.89f / peak);

    return RecordData::create (audio, sr, "Built-in: Ahh / Stab / Kick / Snare");
}
