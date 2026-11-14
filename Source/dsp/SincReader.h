#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

/**
    Band-limited, bidirectional, variable-rate sample reader.

    A windowed-sinc kernel whose width (and cutoff) scales with playback speed:
    when the record is pushed faster than 1x the cutoff drops by the same factor,
    so fast cuts don't alias into grainy digital junk. Slow drags use the full
    16-tap kernel, so the low "groan" stays clean instead of turning into the
    stair-step buzz of linear interpolation.
*/
class SincReader
{
public:
    static constexpr int halfTaps = 8;     // zero crossings per side at 1x
    static constexpr int resolution = 512; // table points per zero crossing
    static constexpr double maxScale = 8.0;
    static constexpr int maxTaps = 2 * halfTaps * 8 + 2;

    SincReader()
    {
        constexpr double cutoff = 0.92; // leave a small transition band below Nyquist
        constexpr double beta = 8.6;
        const double i0b = besselI0 (beta);

        for (int i = 0; i < (int) table.size(); ++i)
        {
            const double u = (double) i / resolution;
            if (u >= halfTaps) { table[(size_t) i] = 0.0f; continue; }

            const double x = cutoff * u;
            const double sinc = std::abs (x) < 1.0e-12 ? 1.0 : std::sin (pi * x) / (pi * x);
            const double r = u / halfTaps;
            const double window = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            table[(size_t) i] = (float) (cutoff * sinc * window);
        }
    }

    struct Source
    {
        const float* left = nullptr;
        const float* right = nullptr;
        const float* noise = nullptr; // optional
        int64_t length = 0;
        bool loop = true;
    };

    struct Frame { float left = 0, right = 0, noise = 0; };

    /** position in source samples, speed in source samples per output sample. */
    Frame read (const Source& src, double position, double speed) const noexcept
    {
        Frame out;
        if (src.length <= 0) return out;

        if (src.loop)
        {
            position = std::fmod (position, (double) src.length);
            if (position < 0.0) position += (double) src.length;
        }

        const double scale = std::clamp (std::abs (speed), 1.0, maxScale);
        const double halfWidth = halfTaps * scale;
        const auto first = (int64_t) std::ceil (position - halfWidth);
        const auto last  = (int64_t) std::floor (position + halfWidth);
        const float inv = (float) (resolution / scale);
        const bool inRange = first >= 0 && last < src.length;

        float sumW = 0, l = 0, r = 0, n = 0;

        for (int64_t i = first; i <= last; ++i)
        {
            const float u = (float) std::abs (position - (double) i) * inv;
            const auto idx0 = (int) u;
            if (idx0 >= halfTaps * resolution) continue;
            const float frac = u - (float) idx0;
            const float w = table[(size_t) idx0] + frac * (table[(size_t) idx0 + 1] - table[(size_t) idx0]);
            sumW += w;

            int64_t idx = i;
            if (! inRange)
            {
                if (src.loop)
                {
                    idx %= src.length;
                    if (idx < 0) idx += src.length;
                }
                else if (idx < 0 || idx >= src.length)
                {
                    continue; // silence before the lead-in / after the run-out
                }
            }

            l += w * src.left[idx];
            r += w * src.right[idx];
            if (src.noise != nullptr) n += w * src.noise[idx];
        }

        if (sumW > 1.0e-6f)
        {
            const float norm = 1.0f / sumW;
            out.left = l * norm;
            out.right = r * norm;
            out.noise = n * norm;
        }
        return out;
    }

private:
    static constexpr double pi = 3.14159265358979323846;

    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1.0e-12 * sum) break;
        }
        return sum;
    }

    std::array<float, halfTaps * resolution + 2> table {};
};
