#include "RecordData.h"
#include "Riaa.h"

using namespace juce;

namespace
{
    constexpr double maxLengthSeconds = 15.0 * 60.0;

    void preEmphasise (const float* in, float* out, int numSamples, double sampleRate)
    {
        Biquad pre = Riaa::makePreEmphasis (sampleRate);

        // Warm the filter up on the tail so the loop point is seamless.
        const int warm = jmin (numSamples, (int) (sampleRate * 0.25));
        for (int i = numSamples - warm; i < numSamples; ++i)
            pre.process (in[i]);

        for (int i = 0; i < numSamples; ++i)
            out[i] = (float) pre.process (in[i]);
    }

    /** Surface noise as it would sound at 1x, then "cut" into the groove like the music. */
    std::vector<float> makeSurfaceNoise (int numSamples, double sampleRate)
    {
        std::vector<float> playback ((size_t) numSamples, 0.0f);
        Random rng (0x5eed); // deterministic: same crackles at the same groove positions every render

        // Hiss: band-limited white noise.
        Biquad hp = Riaa::makeHighPass (400.0, sampleRate);
        Biquad lp = Riaa::makeLowPass (jmin (9000.0, sampleRate * 0.45), 0.7, sampleRate);
        for (auto& s : playback)
        {
            const double white = (rng.nextDouble() * 2.0 - 1.0) + (rng.nextDouble() * 2.0 - 1.0);
            s = (float) (lp.process (hp.process (white)) * 0.0035);
        }

        // Crackle: sparse clicks with a short ringing tail. Positions are fixed to
        // the groove, so scratching back over a crackle replays it - like vinyl.
        auto addClick = [&] (int64_t at, float amp)
        {
            const int len = 6 + rng.nextInt (24);
            const float decay = 0.55f + 0.3f * rng.nextFloat();
            const float sign = rng.nextBool() ? 1.0f : -1.0f;
            float env = amp;
            for (int k = 0; k < len && at + k < numSamples; ++k)
            {
                const float v = (k == 0 ? 1.0f : (rng.nextFloat() * 2.0f - 1.0f));
                playback[(size_t) (at + k)] += sign * env * v;
                env *= decay;
            }
        };

        const double seconds = numSamples / sampleRate;
        const int smallClicks = (int) (seconds * 9.0);
        const int bigPops = (int) (seconds * 0.35);
        for (int i = 0; i < smallClicks; ++i)
            addClick ((int64_t) (rng.nextDouble() * numSamples),
                      0.006f + 0.04f * std::pow (rng.nextFloat(), 3.0f));
        for (int i = 0; i < bigPops; ++i)
            addClick ((int64_t) (rng.nextDouble() * numSamples), 0.08f + 0.15f * rng.nextFloat());

        std::vector<float> groove ((size_t) numSamples);
        preEmphasise (playback.data(), groove.data(), numSamples, sampleRate);
        return groove;
    }

    std::vector<RecordData::Peak> makeOverview (const AudioBuffer<float>& audio)
    {
        constexpr int buckets = 4096;
        std::vector<RecordData::Peak> peaks ((size_t) buckets, { 0.0f, 0.0f });
        const int n = audio.getNumSamples();
        if (n == 0) return peaks;

        for (int b = 0; b < buckets; ++b)
        {
            const int start = (int) ((int64_t) b * n / buckets);
            const int end = jmax (start + 1, (int) ((int64_t) (b + 1) * n / buckets));
            float lo = 0.0f, hi = 0.0f;
            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            {
                const auto range = FloatVectorOperations::findMinAndMax (audio.getReadPointer (ch, start), jmin (end, n) - start);
                lo = jmin (lo, range.getStart());
                hi = jmax (hi, range.getEnd());
            }
            peaks[(size_t) b] = { lo, hi };
        }
        return peaks;
    }

    std::vector<RecordData::Onset> detectOnsets (const AudioBuffer<float>& audio, double sampleRate)
    {
        std::vector<RecordData::Onset> onsets;
        const int n = audio.getNumSamples();
        const int hop = jmax (16, (int) std::round (sampleRate * 0.005));
        const int frames = n / hop;
        if (frames < 4) return onsets;

        // Energy of the first difference (emphasises attacks over sustained bass).
        std::vector<double> energy ((size_t) frames, 0.0);
        const float* l = audio.getReadPointer (0);
        const float* r = audio.getReadPointer (audio.getNumChannels() > 1 ? 1 : 0);
        float prev = 0.0f;
        for (int f = 0; f < frames; ++f)
        {
            double e = 0.0;
            for (int i = f * hop; i < (f + 1) * hop; ++i)
            {
                const float m = 0.5f * (l[i] + r[i]);
                const float d = m - prev;
                prev = m;
                e += (double) d * d + 0.05 * (double) m * m;
            }
            energy[(size_t) f] = e / hop;
        }

        const double maxE = *std::max_element (energy.begin(), energy.end());
        if (maxE <= 0.0) return onsets;
        const double floorE = maxE * 1.0e-3;

        std::vector<double> novelty ((size_t) frames, 0.0);
        for (int f = 0; f < frames; ++f)
        {
            double past = 0.0;
            int count = 0;
            for (int k = f - 4; k < f; ++k)
                if (k >= 0) { past += energy[(size_t) k]; ++count; }
            past = count > 0 ? past / count : 0.0;
            if (energy[(size_t) f] > floorE)
                novelty[(size_t) f] = jmax (0.0, std::log (energy[(size_t) f] + 1.0e-12) - std::log (past + floorE * 0.1 + 1.0e-12));
        }

        constexpr int neighbourhood = 10; // +/- 50 ms
        for (int f = 0; f < frames; ++f)
        {
            const double v = novelty[(size_t) f];
            if (v < 1.0) continue;
            bool isPeak = true;
            for (int k = jmax (0, f - neighbourhood); k <= jmin (frames - 1, f + neighbourhood) && isPeak; ++k)
                if (novelty[(size_t) k] > v || (exactlyEqual (novelty[(size_t) k], v) && k < f))
                    isPeak = false;
            if (! isPeak) continue;

            const double time = jmax (0.0, (double) (f - 1) * hop / sampleRate);
            onsets.push_back ({ time, (float) (v * std::sqrt (energy[(size_t) f] / maxE)) });
        }

        std::sort (onsets.begin(), onsets.end(), [] (auto& a, auto& b) { return a.strength > b.strength; });
        if (onsets.size() > 256) onsets.resize (256);
        return onsets;
    }
}

RecordData::Ptr RecordData::create (const AudioBuffer<float>& audio, double sr, const String& name, const File& source)
{
    Ptr rec = new RecordData();
    rec->name = name;
    rec->sourceFile = source;
    rec->sampleRate = sr;

    const int n = audio.getNumSamples();
    rec->clean.setSize (2, n);
    for (int ch = 0; ch < 2; ++ch)
        rec->clean.copyFrom (ch, 0, audio, jmin (ch, audio.getNumChannels() - 1), 0, n);

    rec->groove.setSize (2, n);
    for (int ch = 0; ch < 2; ++ch)
        preEmphasise (rec->clean.getReadPointer (ch), rec->groove.getWritePointer (ch), n, sr);

    rec->surface = makeSurfaceNoise (n, sr);
    rec->overview = makeOverview (rec->clean);
    rec->onsets = detectOnsets (rec->clean, sr);
    return rec;
}

RecordData::Ptr RecordData::loadFromFile (const File& file, AudioFormatManager& formats, String& error)
{
    std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Couldn't read " + file.getFileName();
        return nullptr;
    }

    if (reader->lengthInSamples <= 0 || reader->sampleRate <= 0)
    {
        error = file.getFileName() + " is empty";
        return nullptr;
    }

    const auto maxSamples = (int64) (reader->sampleRate * maxLengthSeconds);
    const int n = (int) jmin (reader->lengthInSamples, maxSamples);
    AudioBuffer<float> buffer ((int) jlimit (1u, 2u, reader->numChannels), n);
    reader->read (&buffer, 0, n, 0, true, buffer.getNumChannels() > 1);
    return create (buffer, reader->sampleRate, file.getFileNameWithoutExtension(), file);
}

RecordData::Ptr RecordData::loadFromMemory (const MemoryBlock& data, AudioFormatManager& formats,
                                            const String& name, const File& originalFile)
{
    auto stream = std::make_unique<MemoryInputStream> (data, false);
    std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (std::move (stream)));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;

    const int n = (int) reader->lengthInSamples;
    AudioBuffer<float> buffer ((int) jlimit (1u, 2u, reader->numChannels), n);
    reader->read (&buffer, 0, n, 0, true, buffer.getNumChannels() > 1);
    return create (buffer, reader->sampleRate, name, originalFile);
}

bool RecordData::encodeToFlac (MemoryBlock& dest) const
{
    FlacAudioFormat flac;
    auto* stream = new MemoryOutputStream (dest, false);
    std::unique_ptr<AudioFormatWriter> writer (flac.createWriterFor (stream, sampleRate, 2, 24, {}, 0));
    if (writer == nullptr)
    {
        delete stream;
        return false;
    }
    return writer->writeFromAudioSampleBuffer (clean, 0, clean.getNumSamples());
}

int computeChopPoints (const RecordData& record, int mode, int count, double* dest, int maxCount)
{
    count = jlimit (1, maxCount, count);
    const double duration = record.getDurationSeconds();

    if (mode == 0 && ! record.onsets.empty())
    {
        const int n = jmin (count, (int) record.onsets.size());
        for (int i = 0; i < n; ++i)
            dest[i] = record.onsets[(size_t) i].time;
        std::sort (dest, dest + n);
        return n;
    }

    for (int i = 0; i < count; ++i)
        dest[i] = duration * i / count;
    return count;
}
