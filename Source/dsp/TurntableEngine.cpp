#include "TurntableEngine.h"

using namespace juce;

namespace
{
    constexpr double seekFadeSeconds = 0.003;
    constexpr double jogAutoTouchSeconds = 0.08;
    constexpr double ccAutoTouchSeconds = 0.15;
    constexpr double maxSpeed = 24.0; // a hard flick tops out around here
    constexpr float  noiseScale = 2.5f;
}

void TurntableEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    dt = 1.0 / sampleRate;

    for (int i = 0; i < 3; ++i)
    {
        deEmph[i] = Riaa::makeDeEmphasis (sampleRate);
        subsonic[i] = Riaa::makeHighPass (18.0, sampleRate);
    }
    reset();
}

void TurntableEngine::reset()
{
    position = 0.0;
    speed = platterSpeed = 1.0;
    hand = handMid = handRaw = handVelocity = 0.0;
    touching = holdTouch = paramTouch = ccTouch = noteTouch = bendTouch = handStopTouch = false;
    autoTouchSamples = 0;
    noteBrake = noteCut = false;
    cutNotesHeld = 0;
    lastChopNote = -1;
    noteGain = noteGainTarget = 1.0f;
    faderRampRemaining = 0;
    ghost = {};
    mainGain = 1.0f;
    fadeStep = 0.0f;
    firstBlock = true;
    lastBend = 0.0;
    lastScratchCC = -2.0;
    for (auto& f : deEmph) f.reset();
    for (auto& f : subsonic) f.reset();
}

void TurntableEngine::setRecord (const RecordData* newRecord)
{
    if (newRecord == record) return;
    record = newRecord;
    position = hand = handMid = handRaw = 0.0;
    ghost = {};
    mainGain = 1.0f;
    chopsRecord = nullptr; // force refresh
}

double TurntableEngine::platterTarget (const EngineParams& p) const noexcept
{
    return (double) p.rpmFactor * (1.0 + (double) p.pitch) * (double) p.speed * (p.reverse ? -1.0 : 1.0);
}

// ---------------------------------------------------------------------------
// Hand & touch
// ---------------------------------------------------------------------------
void TurntableEngine::updateTouch()
{
    const bool shouldTouch = paramTouch || holdTouch || ccTouch || noteTouch || bendTouch || handStopTouch || autoTouchSamples > 0;
    if (shouldTouch == touching) return;

    touching = shouldTouch;
    if (touching)
    {
        // Put the hand where the record will come to rest, so grabbing a spinning
        // record stops it without the spring yanking it backwards afterwards.
        // For a critically damped spring starting at speed v, a target at
        // position + v * tau gives a monotonic stop.
        hand = handMid = handRaw = position + speed * currentHandTau;
        handVelocity = 0.0;
    }
}

void TurntableEngine::moveHand (double grooveSeconds, double autoTouchSeconds)
{
    if (autoTouchSeconds > 0.0)
    {
        autoTouchSamples = jmax (autoTouchSamples, (int) (autoTouchSeconds * sampleRate));
        updateTouch();
    }
    if (touching)
        handRaw += grooveSeconds;
}

void TurntableEngine::jogRotations (double rotations)
{
    moveHand (rotations * secondsPerRotation * currentSensitivity, jogAutoTouchSeconds);
}

void TurntableEngine::seekSeconds (double seconds)
{
    // Crossfade from the old needle position to the new one (3 ms) so jumps
    // don't click, and drag the hand along if it is on the record.
    ghost.position = position;
    ghost.speed = speed;
    ghost.gain = mainGain;
    mainGain = 0.0f;
    fadeStep = (float) (1.0 / (seekFadeSeconds * sampleRate));

    const double delta = seconds - position;
    position = seconds;
    hand += delta;
    handMid += delta;
    handRaw += delta;
}

// ---------------------------------------------------------------------------
// Chops
// ---------------------------------------------------------------------------
void TurntableEngine::refreshChops (const EngineParams& p)
{
    if (record == chopsRecord && p.chopMode == chopsMode && p.slices == chopsCount)
        return;

    chopsRecord = record;
    chopsMode = p.chopMode;
    chopsCount = p.slices;
    numChops = record != nullptr ? computeChopPoints (*record, p.chopMode, p.slices, chops.data(), maxChops) : 0;
}

void TurntableEngine::restartForHostPlayback (const EngineParams& p)
{
    // Every render starts from exactly the same state - no crossfade from wherever
    // the needle was, no filter history - so automation plays back identically.
    position = hand = handMid = handRaw = 0.0;
    handVelocity = 0.0;
    ghost = {};
    mainGain = 1.0f;
    fadeStep = 0.0f;
    for (auto& f : deEmph) f.reset();
    for (auto& f : subsonic) f.reset();
    lastParamScratch = p.scratch;
    lastParamFader = p.fader;
    faderValue = p.fader;
    faderRampRemaining = 0;
    faderGain = faderLaw (p.hamster ? 1.0f - p.fader : p.fader, p.faderCurve);
    noteGain = noteGainTarget = 1.0f;
    lastChopNote = -1;
    lastBend = 0.0;
    lastScratchCC = -2.0;
    platterAngle = 0.0;
    platterSpeed = speed = platterTarget (p);
    paramTouch = holdTouch = ccTouch = noteTouch = bendTouch = handStopTouch = false;
    autoTouchSamples = 0;
    noteBrake = noteCut = false;
    cutNotesHeld = 0;
    updateTouch();
}

// ---------------------------------------------------------------------------
// MIDI
// ---------------------------------------------------------------------------
void TurntableEngine::handleMidi (const MidiMessage& m, const EngineParams& p)
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        if (note == p.touchNote)
        {
            noteTouch = true;
            updateTouch();
            return;
        }
        if (note == p.cutNote)
        {
            ++cutNotesHeld;
            return;
        }
        if (p.baseNote >= 128 || note < p.baseNote || numChops == 0)
            return;

        const int index = (note - p.baseNote) % numChops;
        seekSeconds (chops[(size_t) index]);
        lastChopNote = note;
        noteBrake = noteCut = false;
        if (handStopTouch) { handStopTouch = false; updateTouch(); }
        noteGainTarget = p.velocity ? jmap (m.getFloatVelocity(), 0.15f, 1.0f) : 1.0f;

        // "Instant" = the platter's already turning under the slipmat and the
        // record is let go perfectly; "Release" = held still, then let go.
        speed = p.noteStart == 0 ? platterSpeed : 0.0;
        return;
    }

    if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        if (note == p.touchNote)
        {
            noteTouch = false;
            updateTouch();
        }
        else if (note == p.cutNote)
        {
            cutNotesHeld = jmax (0, cutNotesHeld - 1);
        }
        else if (note == lastChopNote)
        {
            switch (p.noteOff)
            {
                case 1: noteCut = true; break;
                case 2: handStopTouch = true; updateTouch(); break;
                case 3: noteBrake = true; break;
                default: break;
            }
        }
        return;
    }

    if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const int value = m.getControllerValue();

        if (cc == p.touchCC)
        {
            ccTouch = value >= 64;
            updateTouch();
        }
        if (cc == p.faderCC)
        {
            faderValue = (float) value / 127.0f;
            faderRampRemaining = 0;
        }
        if (cc == p.scratchCC)
        {
            const double v = value / 127.0 * 2.0 - 1.0;
            if (lastScratchCC > -1.5)
                moveHand ((v - lastScratchCC) * p.rangeRot * secondsPerRotation * p.sensitivity, ccAutoTouchSeconds);
            lastScratchCC = v;
        }
        if (cc == p.jogCC)
        {
            int ticks = 0;
            switch (p.jogEncoding)
            {
                case 1:  ticks = value < 64 ? value : value - 128; break;   // two's complement
                case 2:  ticks = (value & 64) ? -(value & 63) : (value & 63); break; // sign bit
                default: ticks = value - 64; break;                          // offset 64
            }
            if (ticks != 0)
                jogRotations ((double) ticks / jmax (1.0f, p.jogTicks));
        }
        if (cc == 120 || cc == 123) // all sound off / all notes off
        {
            noteTouch = handStopTouch = false;
            cutNotesHeld = 0;
            updateTouch();
        }
        return;
    }

    if (m.isPitchWheel() && p.bendScratch)
    {
        const double bend = (m.getPitchWheelValue() - 8192) / 8192.0;
        const bool offCentre = std::abs (bend) > 0.004;

        if (offCentre && ! bendTouch)
        {
            bendTouch = true;
            updateTouch();
        }
        if (touching)
            handRaw += (bend - lastBend) * p.rangeRot * secondsPerRotation * p.sensitivity;
        lastBend = bend;

        if (! offCentre && bendTouch)
        {
            bendTouch = false;
            updateTouch();
        }
    }
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------
float TurntableEngine::faderLaw (float x, float curve) const noexcept
{
    // Scratch-mixer fader: a small dead zone ("cut lag") then a very short cut-in.
    const float lag = 0.015f * curve;
    const float cutIn = 1.0f - 0.97f * curve;
    const float t = jlimit (0.0f, 1.0f, (x - lag) / jmax (0.001f, cutIn - lag));
    return std::sin (t * MathConstants<float>::halfPi);
}

void TurntableEngine::process (AudioBuffer<float>& output, const MidiBuffer& midi,
                               const EngineParams& p, const HostState& host)
{
    const int numSamples = output.getNumSamples();
    output.clear();
    if (numSamples == 0) return;

    refreshChops (p);
    currentHandTau = (double) jmax (0.0005f, p.handTau);
    currentSensitivity = (double) p.sensitivity;

    // Host transport sync: deterministic restart on play, motor follows transport.
    const bool hostPlaying = host.valid && host.playing;
    if (p.hostSync && hostPlaying && ! wasHostPlaying)
        restartForHostPlayback (p);
    wasHostPlaying = hostPlaying;

    if (firstBlock)
    {
        lastParamScratch = p.scratch;
        lastParamFader = p.fader;
        faderValue = p.fader;
        platterSpeed = speed = p.motor ? platterTarget (p) : 0.0;
        firstBlock = false;
    }

    // Automatable touch and hold
    const bool wasTouching = touching;
    if (p.touch != paramTouch || p.hold != holdTouch)
    {
        paramTouch = p.touch;
        holdTouch = p.hold;
        updateTouch();
    }
    const bool touchPressed = touching && ! wasTouching;

    // Automatable hand position: ramp linearly across the block. The hand is
    // relative to where the record was grabbed, so a jump in the Scratch value
    // that arrives together with the touch-down must not move the record.
    scratchRampStep = touchPressed ? 0.0
                                   : (double) (p.scratch - lastParamScratch) * p.rangeRot * secondsPerRotation / numSamples;
    lastParamScratch = p.scratch;

    // Automatable crossfader: ramp too, but only when the parameter actually moved
    // (so the fader CC can own it in between).
    if (! exactlyEqual (p.fader, lastParamFader))
    {
        faderRampStep = (p.fader - faderValue) / (float) numSamples;
        faderRampRemaining = numSamples;
        lastParamFader = p.fader;
    }

    // Render in segments split at MIDI events (sample accurate).
    float* left = output.getWritePointer (0);
    float* right = output.getNumChannels() > 1 ? output.getWritePointer (1) : nullptr;

    int cursor = 0;
    for (const auto meta : midi)
    {
        const int at = jlimit (0, numSamples, meta.samplePosition);
        if (at > cursor)
        {
            renderSegment (left, right, cursor, at - cursor, p);
            cursor = at;
        }
        handleMidi (meta.getMessage(), p);
    }
    if (cursor < numSamples)
        renderSegment (left, right, cursor, numSamples - cursor, p);

    // Telemetry
    const double len = record != nullptr ? record->getDurationSeconds() : 0.0;
    double wrapped = position;
    if (len > 0.0 && p.loop)
    {
        wrapped = std::fmod (position, len);
        if (wrapped < 0.0) wrapped += len;
    }
    uiPosition.store (wrapped);
    uiRecordAngle.store (position / secondsPerRotation);
    uiPlatterAngle.store (platterAngle);
    uiSpeed.store ((float) speed);
    uiFaderGain.store (faderGain);
    uiTouching.store (touching);
    uiLevel.store (levelFollower);
}

void TurntableEngine::renderSegment (float* left, float* right, int start, int num, const EngineParams& p)
{
    const bool motorOn = p.hostSync ? wasHostPlaying : p.motor;
    const double target = motorOn ? platterTarget (p) : 0.0;
    const double motorAccel = (motorOn ? 1.0 / jmax (0.005f, p.startTime) : 1.0 / jmax (0.005f, p.brakeTime)) * dt;
    const double brakeAccel = 1.0 / jmax (0.005f, p.brakeTime) * dt;
    const double slipAccel = 1.0 / jmax (0.002f, p.slipTime) * dt;
    const double w0 = 1.0 / jmax (0.0005f, p.handTau);
    // two cascaded stages of tau/2: a 2nd-order glide, so control steps become continuous motion
    const double inputCoeff = p.inputTau > 0.0001f ? 1.0 - std::exp (-dt / (0.5 * (double) p.inputTau)) : 1.0;
    const double fast = (double) p.fastSmooth;
    // Smooth 4: widen the anti-alias kernel further as the record goes faster,
    // taking the fizz off very fast cuts.
    auto kernelSpeed = [fast] (double v) { return v * (1.0 + 1.5 * fast * jmax (0.0, std::abs (v) - 1.0)); };

    const float noiseGain = p.noise * p.noise * noiseScale;
    const float faderCoeff = 1.0f - std::exp (-1.0f / (0.0006f * (float) sampleRate));
    const float smoothCoeff = 1.0f - std::exp (-1.0f / (0.004f * (float) sampleRate));
    const float vol = Decibels::decibelsToGain (p.volumeDb, -60.0f);
    const float targetL = vol * (p.pan > 0.0f ? 1.0f - p.pan : 1.0f);
    const float targetR = vol * (p.pan < 0.0f ? 1.0f + p.pan : 1.0f);

    SincReader::Source src;
    double srcRate = 44100.0;
    if (record != nullptr)
    {
        const auto& buf = p.phono ? record->groove : record->clean;
        src.left = buf.getReadPointer (0);
        src.right = buf.getReadPointer (1);
        src.noise = record->surface.data();
        src.length = record->getLength();
        src.loop = p.loop;
        srcRate = record->sampleRate;
    }
    const double rateRatio = srcRate / sampleRate;

    for (int i = start; i < start + num; ++i)
    {
        // --- auto-touch timeout (jog wheels / CC scratching) -----------------
        if (autoTouchSamples > 0 && --autoTouchSamples == 0)
            updateTouch();

        // --- platter motor ------------------------------------------------------
        platterSpeed += jlimit (-motorAccel, motorAccel, target - platterSpeed);
        platterAngle += platterSpeed * dt / secondsPerRotation;

        // --- record: hand, brake or slipmat friction ----------------------------
        if (touching)
        {
            // Smooth 2: interpolate between control points (mouse, MIDI, automation steps)
            handRaw += scratchRampStep;
            const double previousHand = hand;
            handMid += (handRaw - handMid) * inputCoeff;
            hand += (handMid - hand) * inputCoeff;
            handVelocity = inputCoeff >= 1.0 ? scratchRampStep / dt : (hand - previousHand) / dt;
            // critically damped spring: hand + fingertip compliance + record inertia
            const double accel = w0 * w0 * (hand - position) + 2.0 * w0 * (handVelocity - speed);
            speed += accel * dt;
        }
        else if (noteBrake)
        {
            speed += jlimit (-brakeAccel, brakeAccel, -speed);
        }
        else
        {
            // Coulomb friction: the slipmat drags the record towards platter speed
            // with constant force, so release ramps up linearly like the real thing.
            speed += jlimit (-slipAccel, slipAccel, platterSpeed - speed);
        }
        speed = jlimit (-maxSpeed, maxSpeed, speed);
        position += speed * dt;

        // --- needle -------------------------------------------------------------
        float sL = 0.0f, sR = 0.0f, sN = 0.0f;
        if (record != nullptr)
        {
            auto f = reader.read (src, position * srcRate, kernelSpeed (speed) * rateRatio);
            const float velocityGain = p.phono ? (float) speed : 1.0f; // cartridge output ~ stylus velocity
            sL = f.left * velocityGain * mainGain;
            sR = f.right * velocityGain * mainGain;
            sN = f.noise * (float) speed * mainGain;

            if (ghost.gain > 0.0f)
            {
                ghost.position += ghost.speed * dt;
                auto g = reader.read (src, ghost.position * srcRate, kernelSpeed (ghost.speed) * rateRatio);
                const float gv = p.phono ? (float) ghost.speed : 1.0f;
                sL += g.left * gv * ghost.gain;
                sR += g.right * gv * ghost.gain;
                sN += g.noise * (float) ghost.speed * ghost.gain;
            }
        }

        if (fadeStep > 0.0f)
        {
            mainGain = jmin (1.0f, mainGain + fadeStep);
            ghost.gain = jmax (0.0f, ghost.gain - fadeStep);
            if (mainGain >= 1.0f && ghost.gain <= 0.0f) fadeStep = 0.0f;
        }

        // --- phono preamp -------------------------------------------------------
        double oL = sL, oR = sR;
        if (p.phono)
        {
            oL = deEmph[0].process (oL);
            oR = deEmph[1].process (oR);
        }
        oL = subsonic[0].process (oL);
        oR = subsonic[1].process (oR);
        const double oN = subsonic[2].process (deEmph[2].process (sN));

        // --- mixer --------------------------------------------------------------
        if (faderRampRemaining > 0)
        {
            faderValue = jlimit (0.0f, 1.0f, faderValue + faderRampStep);
            --faderRampRemaining;
        }
        const float x = p.hamster ? 1.0f - faderValue : faderValue;
        const bool gated = p.mute || noteCut || cutNotesHeld > 0;
        const float faderTarget = gated ? 0.0f : faderLaw (x, p.faderCurve);
        faderGain += (faderTarget - faderGain) * faderCoeff;
        noteGain += (noteGainTarget - noteGain) * smoothCoeff;
        volL += (targetL - volL) * smoothCoeff;
        volR += (targetR - volR) * smoothCoeff;

        const float g = faderGain * noteGain;
        const float outL = (float) (oL + noiseGain * oN) * g * volL;
        const float outR = (float) (oR + noiseGain * oN) * g * volR;

        if (right != nullptr)
        {
            left[i] = outL;
            right[i] = outR;
        }
        else
        {
            left[i] = 0.5f * (outL + outR);
        }

        const float a = std::abs (outL) + std::abs (outR);
        levelFollower = a > levelFollower ? a : levelFollower * 0.9995f;
    }
}
