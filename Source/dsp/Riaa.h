#pragma once

#include <cmath>
#include <complex>

/**
    Phono-stage maths.

    A record does not store the audio itself: the cutting lathe stores the audio
    with RIAA *pre-emphasis* (treble boosted, bass cut), and a magnetic cartridge
    outputs a voltage proportional to stylus *velocity*. The phono preamp then
    applies RIAA *de-emphasis* at fixed frequencies.

    When the record is moved at rate r, the groove content is shifted in frequency
    by r and the stylus velocity is multiplied by r, but the de-emphasis stays put.
    So the level follows the hand: slow drags fade out towards silence, fast pushes
    hit harder than the source, and the RIAA mismatch adds a gentle tonal tilt.
    That speed-dependent dynamics is a big part of why real scratches sound alive
    and "digital" scratches sound flat.

    We reproduce it exactly: the loaded audio is pre-emphasised once at load time
    ("cutting the record"), then at playback the read signal is multiplied by the
    platter speed and passed through de-emphasis at the host rate.
*/
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0; }

    double magnitudeAt (double freq, double sampleRate) const
    {
        const std::complex<double> z = std::polar (1.0, -2.0 * 3.14159265358979323846 * freq / sampleRate);
        const auto num = b0 + b1 * z + b2 * z * z;
        const auto den = 1.0 + a1 * z + a2 * z * z;
        return std::abs (num / den);
    }
};

namespace Riaa
{
    // RIAA time constants (seconds) + the Neumann 3.18us zero that keeps the
    // pre-emphasis bounded and invertible.
    inline constexpr double T1 = 3180e-6, T2 = 318e-6, T3 = 75e-6, T4 = 3.18e-6;

    /** De-emphasis (playback) curve, normalised to 0 dB at 1 kHz. */
    inline Biquad makeDeEmphasis (double sampleRate)
    {
        const double K = 2.0 * sampleRate;

        // (1 + sT) -> [(1 + TK) + (1 - TK) z^-1] / (1 + z^-1); the (1 + z^-1)
        // terms cancel because numerator and denominator are both 2nd order.
        auto factor = [K] (double T, double& c0, double& c1) { c0 = 1.0 + T * K; c1 = 1.0 - T * K; };

        double n20, n21, n40, n41, d10, d11, d30, d31;
        factor (T2, n20, n21);
        factor (T4, n40, n41);
        factor (T1, d10, d11);
        factor (T3, d30, d31);

        const double nb0 = n20 * n40, nb1 = n20 * n41 + n21 * n40, nb2 = n21 * n41;
        const double da0 = d10 * d30, da1 = d10 * d31 + d11 * d30, da2 = d11 * d31;

        Biquad f;
        f.b0 = nb0 / da0; f.b1 = nb1 / da0; f.b2 = nb2 / da0;
        f.a1 = da1 / da0; f.a2 = da2 / da0;

        const double g = 1.0 / f.magnitudeAt (1000.0, sampleRate);
        f.b0 *= g; f.b1 *= g; f.b2 *= g;
        return f;
    }

    /** Exact inverse of makeDeEmphasis at the same sample rate (cutting curve). */
    inline Biquad makePreEmphasis (double sampleRate)
    {
        const Biquad d = makeDeEmphasis (sampleRate);
        Biquad p;
        // swap numerator and denominator, normalise by the new a0 (= d.b0)
        p.b0 = 1.0 / d.b0;
        p.b1 = d.a1 / d.b0;
        p.b2 = d.a2 / d.b0;
        p.a1 = d.b1 / d.b0;
        p.a2 = d.b2 / d.b0;
        return p;
    }

    /** 2nd-order Butterworth high-pass (the "subsonic" / rumble filter of a phono stage). */
    inline Biquad makeHighPass (double freq, double sampleRate)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * freq / sampleRate;
        const double cosw = std::cos (w0), alpha = std::sin (w0) / (2.0 * 0.7071067811865476);
        const double a0 = 1.0 + alpha;
        Biquad f;
        f.b0 = (1.0 + cosw) * 0.5 / a0;
        f.b1 = -(1.0 + cosw) / a0;
        f.b2 = f.b0;
        f.a1 = -2.0 * cosw / a0;
        f.a2 = (1.0 - alpha) / a0;
        return f;
    }

    /** RBJ band-pass (constant 0 dB peak gain). */
    inline Biquad makeBandPass (double freq, double q, double sampleRate)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * freq / sampleRate;
        const double cosw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        Biquad f;
        f.b0 = alpha / a0;
        f.b1 = 0.0;
        f.b2 = -alpha / a0;
        f.a1 = -2.0 * cosw / a0;
        f.a2 = (1.0 - alpha) / a0;
        return f;
    }

    inline Biquad makeLowPass (double freq, double q, double sampleRate)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * freq / sampleRate;
        const double cosw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        Biquad f;
        f.b0 = (1.0 - cosw) * 0.5 / a0;
        f.b1 = (1.0 - cosw) / a0;
        f.b2 = f.b0;
        f.a1 = -2.0 * cosw / a0;
        f.a2 = (1.0 - alpha) / a0;
        return f;
    }
}
