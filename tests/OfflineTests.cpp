// Offline test harness: renders scratch scenarios through the real processor,
// checks the audio numerically, writes WAVs for listening and a UI snapshot.
//
//   cmake -B build -DCRATECUT_BUILD_TESTS=ON && cmake --build build --target CrateCutTests
//   ./build/CrateCutTests_artefacts/<config>/CrateCutTests [output-dir]

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    int failures = 0;

    void check (bool ok, const String& what)
    {
        std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << std::endl;
        if (! ok) ++failures;
    }

    void setParam (CrateCutProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    struct FakePlayHead : public AudioPlayHead
    {
        bool playing = false;
        Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            return info;
        }
    };

    /** Renders `seconds` of audio, calling `perBlock (time, midi)` before each block. */
    AudioBuffer<float> render (CrateCutProcessor& p, double seconds,
                               const std::function<void (double, MidiBuffer&)>& perBlock = {})
    {
        const int total = (int) (seconds * sampleRate);
        AudioBuffer<float> out (2, total);
        AudioBuffer<float> block (2, blockSize);
        MidiBuffer midi;

        for (int pos = 0; pos < total; pos += blockSize)
        {
            const int n = jmin (blockSize, total - pos);
            block.setSize (2, n, false, false, true);
            midi.clear();
            if (perBlock) perBlock (pos / sampleRate, midi);
            p.processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, pos, block, ch, 0, n);
        }
        return out;
    }

    std::unique_ptr<CrateCutProcessor> makeProcessor()
    {
        auto p = std::make_unique<CrateCutProcessor>();
        p->setRateAndBufferSizeDetails (sampleRate, blockSize);
        p->prepareToPlay (sampleRate, blockSize);
        // let the processor pick up the default record
        AudioBuffer<float> b (2, blockSize);
        MidiBuffer m;
        p->processBlock (b, m);
        return p;
    }

    struct Stats { float peak = 0, rms = 0, maxStep = 0; bool finite = true; };

    Stats analyse (const AudioBuffer<float>& b, int from = 0, int to = -1)
    {
        Stats s;
        if (to < 0) to = b.getNumSamples();
        double sum = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            const float* d = b.getReadPointer (ch);
            for (int i = from; i < to; ++i)
            {
                if (! std::isfinite (d[i])) s.finite = false;
                s.peak = jmax (s.peak, std::abs (d[i]));
                sum += d[i] * d[i];
                if (i > from + 1) // second difference: big spikes = clicks
                    s.maxStep = jmax (s.maxStep, std::abs (d[i] - 2.0f * d[i - 1] + d[i - 2]));
            }
        }
        s.rms = (float) std::sqrt (sum / jmax (1, (to - from) * b.getNumChannels()));
        return s;
    }

    void writeWav (const AudioBuffer<float>& b, const File& f)
    {
        f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sampleRate, 2, 24, {}, 0));
        if (w != nullptr) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    float dB (float g) { return Decibels::gainToDecibels (g, -200.0f); }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                 : File::getCurrentWorkingDirectory().getChildFile ("test-output");
    outDir.createDirectory();

    // -------------------------------------------------------------------------
    std::cout << "Playback at 33 rpm" << std::endl;
    {
        auto p = makeProcessor();
        setParam (*p, ParamID::noise, 0.0f);
        auto phono = render (*p, 2.5);

        auto q = makeProcessor();
        setParam (*q, ParamID::noise, 0.0f);
        setParam (*q, ParamID::phono, 0.0f);
        auto clean = render (*q, 2.5);

        const auto s = analyse (phono);
        check (s.finite, "output is finite");
        check (s.rms > 0.02f, "plays audio (rms " + String (dB (s.rms), 1) + " dB)");

        // At exactly 1x the RIAA cut + playback curves cancel: phono == clean (minus the 18 Hz rumble filter on both).
        AudioBuffer<float> diff (phono);
        for (int ch = 0; ch < 2; ++ch)
            diff.addFrom (ch, 0, clean, ch, 0, clean.getNumSamples(), -1.0f);
        const auto d = analyse (diff, (int) (0.2 * sampleRate));
        check (dB (d.rms) - dB (analyse (clean).rms) < -35.0f,
               "phono model is transparent at 1x (residual " + String (dB (d.rms) - dB (analyse (clean).rms), 1) + " dB)");
        writeWav (phono, outDir.getChildFile ("01-play.wav"));
    }

    // -------------------------------------------------------------------------
    std::cout << "Baby scratch via Touch + Scratch automation" << std::endl;
    {
        auto p = makeProcessor();
        auto out = render (*p, 3.0, [&] (double t, MidiBuffer&)
        {
            // grab the "ahh" at 0.25 s, then push/pull it at 2 Hz
            if (t >= 0.25 && t < 2.6)
            {
                setParam (*p, ParamID::touch, 1.0f);
                setParam (*p, ParamID::scratch, (float) (0.35 * std::sin ((t - 0.25) * MathConstants<double>::twoPi * 2.0)));
            }
            else
            {
                setParam (*p, ParamID::touch, 0.0f);
            }
        });
        const auto s = analyse (out);
        check (s.finite, "output is finite");
        check (s.rms > 0.01f, "scratch is audible (rms " + String (dB (s.rms), 1) + " dB)");
        check (s.peak < 4.0f, "no runaway level (peak " + String (dB (s.peak), 1) + " dB)");
        writeWav (out, outDir.getChildFile ("02-baby-scratch.wav"));
    }

    // -------------------------------------------------------------------------
    std::cout << "Hand physics" << std::endl;
    {
        auto p = makeProcessor();
        render (*p, 0.5);
        const double before = p->engine.uiPosition.load();
        setParam (*p, ParamID::touch, 1.0f);
        render (*p, 0.3);
        check (std::abs (p->engine.uiSpeed.load()) < 0.01f, "holding the record stops it (speed " + String (p->engine.uiSpeed.load(), 4) + ")");
        const double held = p->engine.uiPosition.load();
        check (held - before < 0.05 && held >= before - 0.001, "grab stops without spring-back (moved " + String ((held - before) * 1000.0, 1) + " ms)");

        // pushing the hand forward by 0.5 throw moves the record by 0.5 * range rotations
        setParam (*p, ParamID::scratch, 0.5f);
        render (*p, 0.3);
        const double moved = p->engine.uiPosition.load() - held;
        const double expected = 0.5 * 0.5 * TurntableEngine::secondsPerRotation;
        check (std::abs (moved - expected) < 0.01, "hand moves the record by the expected groove distance (" + String (moved, 3) + " s vs " + String (expected, 3) + " s)");

        // let go: slipmat pulls it back up to 1x within ~slipmat time
        setParam (*p, ParamID::touch, 0.0f);
        render (*p, 0.03);
        const float midRelease = p->engine.uiSpeed.load();
        render (*p, 0.2);
        check (midRelease > 0.2f && midRelease < 0.95f, "release ramps up through the slipmat (" + String (midRelease, 2) + "x after 30 ms)");
        check (std::abs (p->engine.uiSpeed.load() - 1.0f) < 0.001f, "back at 33 rpm after release");
    }

    // -------------------------------------------------------------------------
    std::cout << "Phono model: level follows hand speed, like a cartridge" << std::endl;
    {
        // Drag the record by hand at a steady speed, with and without the cartridge model.
        auto drag = [] (bool phonoOn, double handSpeed)
        {
            auto p = makeProcessor();
            setParam (*p, ParamID::noise, 0.0f);
            setParam (*p, ParamID::phono, phonoOn ? 1.0f : 0.0f);
            setParam (*p, ParamID::range, 2.0f);
            render (*p, 0.1);
            setParam (*p, ParamID::touch, 1.0f);
            const double perSecond = handSpeed / (2.0 * TurntableEngine::secondsPerRotation); // scratch units per second
            return render (*p, 1.0, [&] (double t, MidiBuffer&) { setParam (*p, ParamID::scratch, (float) jmin (1.0, t * perSecond)); });
        };
        const int from = (int) (0.2 * sampleRate);
        auto slowPhono = drag (true, 0.25), slowClean = drag (false, 0.25);
        const float slowDiff = dB (analyse (slowPhono, from).rms) - dB (analyse (slowClean, from).rms);
        check (slowDiff < -3.0f, "slow drag (0.25x) is quieter through the cartridge (" + String (slowDiff, 1) + " dB vs clean)");
        auto fastPhono = drag (true, 2.5), fastClean = drag (false, 2.5);
        const float fastDiff = dB (analyse (fastPhono, from).rms) - dB (analyse (fastClean, from).rms);
        check (fastDiff > 2.0f, "fast push (2.5x) hits harder through the cartridge (" + (fastDiff > 0 ? String ("+") : String()) + String (fastDiff, 1) + " dB vs clean)");
        writeWav (slowPhono, outDir.getChildFile ("02b-slow-drag-phono.wav"));
        writeWav (slowClean, outDir.getChildFile ("02c-slow-drag-clean.wav"));
    }

    // -------------------------------------------------------------------------
    std::cout << "Motor start/stop" << std::endl;
    {
        auto p = makeProcessor();
        setParam (*p, ParamID::motor, 0.0f);
        render (*p, 0.5);
        const float half = p->engine.uiSpeed.load();
        render (*p, 1.0);
        check (half > 0.3f && half < 0.7f, "brake is gradual (" + String (half, 2) + "x after 0.5 s of a 1 s brake)");
        check (std::abs (p->engine.uiSpeed.load()) < 0.001f, "platter stops");
        setParam (*p, ParamID::motor, 1.0f);
        render (*p, 0.35);
        const float starting = p->engine.uiSpeed.load();
        check (starting > 0.3f && starting < 0.7f, "start-up takes the motor's torque time (" + String (starting, 2) + "x after 0.35 s)");

        auto q = makeProcessor();
        auto out = render (*q, 3.0, [&] (double t, MidiBuffer&) { setParam (*q, ParamID::motor, t < 1.0 ? 1.0f : 0.0f); });
        writeWav (out, outDir.getChildFile ("03-power-off.wav"));
    }

    // -------------------------------------------------------------------------
    std::cout << "Crossfader cuts (transformer)" << std::endl;
    {
        auto p = makeProcessor();
        setParam (*p, ParamID::noise, 0.0f);
        auto out = render (*p, 2.0, [&] (double t, MidiBuffer& m)
        {
            // fader CC 11 at 12 Hz, sample accurate inside the block
            for (int i = 0; i < blockSize; i += 64)
            {
                const double tt = t + i / sampleRate;
                const bool open = std::fmod (tt * 12.0, 1.0) < 0.5;
                m.addEvent (MidiMessage::controllerEvent (1, 11, open ? 127 : 0), i);
            }
        });
        const auto s = analyse (out);
        check (s.finite, "output is finite");
        // the fader is smoothed over ~0.6 ms: no single-sample steps
        check (s.maxStep < 0.6f, "cuts are click-free (max 2nd diff " + String (s.maxStep, 3) + ")");
        writeWav (out, outDir.getChildFile ("04-transformer.wav"));
    }

    // -------------------------------------------------------------------------
    std::cout << "Notes trigger chops" << std::endl;
    {
        auto p = makeProcessor();
        setParam (*p, ParamID::noise, 0.0f);
        const int notes[] = { 60, 61, 62, 63, 60, 62, 60, 61 };
        auto out = render (*p, 4.0, [&] (double t, MidiBuffer& m)
        {
            const int step = (int) (t / 0.5);
            const double stepStart = step * 0.5;
            if (t <= stepStart && stepStart < t + blockSize / sampleRate)
                m.addEvent (MidiMessage::noteOn (1, notes[step % 8], (uint8) 100), 0);
            const double offAt = stepStart + 0.35;
            if (t <= offAt && offAt < t + blockSize / sampleRate)
                m.addEvent (MidiMessage::noteOff (1, notes[step % 8]), (int) ((offAt - t) * sampleRate));
        });
        const auto s = analyse (out);
        check (s.finite && s.rms > 0.01f, "chops play");

        // Compare against the sharpest thing in the sample itself played straight.
        auto ref = makeProcessor();
        setParam (*ref, ParamID::noise, 0.0f);
        const auto base = analyse (render (*ref, 2.6));
        check (s.maxStep <= base.maxStep * 1.05f, "needle jumps are crossfaded (max 2nd diff " + String (s.maxStep, 3)
                                                     + " vs " + String (base.maxStep, 3) + " in the sample itself)");

        auto rec = p->getRecord();
        double chops[32];
        const int n = computeChopPoints (*rec, 0, 8, chops, 32);
        String list;
        for (int i = 0; i < n; ++i) list << String (chops[i], 3) << " ";
        check (n >= 4, "transient chop points found: " + list);
        writeWav (out, outDir.getChildFile ("05-chops.wav"));
    }

    // -------------------------------------------------------------------------
    std::cout << "MIDI controller scratching" << std::endl;
    {
        // Pitch bend: off-centre grabs the record, centre lets go.
        auto p = makeProcessor();
        auto out = render (*p, 3.0, [&] (double t, MidiBuffer& m)
        {
            if (t > 0.3 && t < 2.3)
            {
                const double v = std::sin ((t - 0.3) * MathConstants<double>::pi * 3.0);
                const int bend = jlimit (0, 16383, 8192 + (int) (v * 8191.0 * 0.6));
                m.addEvent (MidiMessage::pitchWheel (1, bend), 0);
            }
            else if (t >= 2.3 && t < 2.32)
            {
                m.addEvent (MidiMessage::pitchWheel (1, 8192), 0);
            }
        });
        check (analyse (out).finite, "pitch-bend scratch is finite");
        writeWav (out, outDir.getChildFile ("06-pitchbend.wav"));

        // Relative jog encoder (offset-64) with auto-touch.
        auto q = makeProcessor();
        render (*q, 0.2);
        const double before = q->engine.uiRecordAngle.load();
        render (*q, 0.3, [&] (double, MidiBuffer& m)
        {
            for (int i = 0; i < blockSize; i += 128)
                m.addEvent (MidiMessage::controllerEvent (1, 16, 64 - 3), i); // backwards
        });
        check (q->engine.uiSpeed.load() < 0.0f, "jog wheel spins the record backwards (" + String (q->engine.uiSpeed.load(), 2) + "x)");
        check (q->engine.uiRecordAngle.load() < before, "jog moved the needle back");
        render (*q, 0.3);
        check (std::abs (q->engine.uiSpeed.load() - 1.0f) < 0.001f, "auto-touch releases after the wheel stops");
    }

    // -------------------------------------------------------------------------
    std::cout << "Host sync makes renders repeatable" << std::endl;
    {
        auto renderTake = [] (float startScratchOffset)
        {
            auto p = makeProcessor();
            FakePlayHead head;
            p->setPlayHead (&head);
            setParam (*p, ParamID::hostSync, 1.0f);
            render (*p, 0.37 + startScratchOffset); // stopped: idle time before play differs per take
            head.playing = true;
            auto out = render (*p, 1.5, [&] (double t, MidiBuffer&)
            {
                setParam (*p, ParamID::touch, t > 0.4 && t < 1.0 ? 1.0f : 0.0f);
                setParam (*p, ParamID::scratch, (float) (0.3 * std::sin (t * 20.0)));
            });
            p->setPlayHead (nullptr);
            return out;
        };
        auto a = renderTake (0.0f), b = renderTake (0.61f);
        AudioBuffer<float> diff (a);
        for (int ch = 0; ch < 2; ++ch) diff.addFrom (ch, 0, b, ch, 0, b.getNumSamples(), -1.0f);
        check (analyse (diff).peak < 1.0e-4f, "two renders with different pre-roll match (diff " + String (dB (analyse (diff).peak), 1) + " dB)");
    }

    // -------------------------------------------------------------------------
    std::cout << "Sample loading + session state" << std::endl;
    {
        const File wav = outDir.getChildFile ("test-sample.wav");
        {
            AudioBuffer<float> b (1, 44100);
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (0, i, 0.5f * (float) std::sin (MathConstants<double>::twoPi * 220.0 * i / 44100.0));
            wav.deleteFile();
            WavAudioFormat fmt;
            std::unique_ptr<AudioFormatWriter> w (fmt.createWriterFor (new FileOutputStream (wav), 44100.0, 1, 16, {}, 0));
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }

        auto p = makeProcessor();
        check (p->loadSample (wav).isEmpty(), "loads a WAV");
        setParam (*p, ParamID::slipmat, 120.0f);
        MemoryBlock state;
        p->getStateInformation (state);

        auto q = makeProcessor();
        q->setStateInformation (state.getData(), (int) state.getSize());
        check (q->getRecord() != nullptr && q->getRecord()->name == "test-sample", "state restores the sample by path");
        check (std::abs (q->apvts.getRawParameterValue (ParamID::slipmat)->load() - 120.0f) < 0.01f, "state restores parameters");

        wav.deleteFile();
        auto r = makeProcessor();
        r->setStateInformation (state.getData(), (int) state.getSize());
        const auto rec = r->getRecord();
        check (rec != nullptr && rec->name == "test-sample" && rec->getLength() == 44100,
               "missing file falls back to the copy embedded in the session");
        auto out = render (*r, 0.5);
        check (analyse (out).rms > 0.05f, "embedded sample plays");
    }

    // -------------------------------------------------------------------------
    std::cout << "Performance" << std::endl;
    {
        auto p = makeProcessor();
        const double start = Time::getMillisecondCounterHiRes();
        render (*p, 10.0, [&] (double t, MidiBuffer&)
        {
            setParam (*p, ParamID::touch, 1.0f);
            setParam (*p, ParamID::scratch, (float) (0.9 * std::sin (t * 25.0))); // fast, wide cuts: worst case kernel width
        });
        const double ms = Time::getMillisecondCounterHiRes() - start;
        check (ms < 2000.0, "10 s of aggressive scratching renders in " + String (ms, 0) + " ms (" + String (ms / 100.0, 1) + "% of one core)");
    }

    // -------------------------------------------------------------------------
    std::cout << "UI snapshot" << std::endl;
    {
        auto p = makeProcessor();
        setParam (*p, ParamID::touch, 1.0f);
        render (*p, 0.4, [&] (double t, MidiBuffer&) { setParam (*p, ParamID::scratch, (float) (0.2 * t)); });
        std::unique_ptr<AudioProcessorEditor> editor (p->createEditor());
        editor->setSize (CrateCutEditor::baseWidth, CrateCutEditor::baseHeight);
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const File png = outDir.getChildFile ("ui.png");
        png.deleteFile();
        FileOutputStream os (png);
        PNGImageFormat().writeImageToStream (img, os);
        check (img.isValid(), "rendered " + png.getFullPathName());
        editor = nullptr;
    }

    std::cout << std::endl << (failures == 0 ? "ALL PASSED" : String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
