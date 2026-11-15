#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include "RecordData.h"
#include "SincReader.h"
#include "Riaa.h"

/** Parameter snapshot for one block (read from the APVTS by the processor). */
struct EngineParams
{
    bool  touch = false;
    float scratch = 0.0f;        // -1..1 hand position
    float rangeRot = 0.5f;       // rotations for half the scratch travel
    float fader = 1.0f;
    float faderCurve = 0.85f;
    bool  hamster = false;
    bool  motor = true;
    bool  reverse = false;       // platter turns backwards
    bool  hold = false;          // record held still; controllers move it directly
    bool  mute = false;
    float speed = 1.0f;          // playback speed multiplier
    float sensitivity = 1.0f;    // scales jog / bend / CC hand movement
    float inputTau = 0.0f;       // seconds: interpolation between control points (smooth 2)
    float fastSmooth = 0.0f;     // 0..1: extra softening of fast cuts (smooth 4)
    float rpmFactor = 1.0f;
    float pitch = 0.0f;          // as a fraction, e.g. 0.08 = +8 %
    float startTime = 0.7f;      // seconds 0 -> 33 rpm
    float brakeTime = 1.0f;      // seconds 33 rpm -> 0
    float slipTime = 0.045f;     // seconds for the slipmat to drag the record 0 -> 33 rpm (smooth 3)
    float handTau = 0.010f;      // seconds (smooth 1)
    bool  phono = true;
    float noise = 0.2f;
    bool  loop = true;
    int   chopMode = 0;
    int   slices = 8;
    int   noteOff = 0;           // 0 hold, 1 cut, 2 hand stop, 3 brake
    int   noteStart = 0;         // 0 instant, 1 release from hand
    bool  velocity = false;
    bool  hostSync = false;
    float volumeDb = 0.0f;
    float pan = 0.0f;

    int   baseNote = 60, touchNote = 48, cutNote = 50;
    int   touchCC = 64, scratchCC = 1, faderCC = 11, jogCC = 16;
    int   jogEncoding = 0;
    float jogTicks = 256.0f;
    bool  bendScratch = true;
};

struct HostState
{
    bool valid = false;
    bool playing = false;
};

/**
    A physical model of a direct-drive turntable with a slipmat, a hand, a
    magnetic cartridge, a phono preamp and a scratch-mixer crossfader.

    Everything here runs on the audio thread and is allocation free.
*/
class TurntableEngine
{
public:
    /** Groove seconds per revolution: the record is "cut" at 33 1/3 rpm. */
    static constexpr double secondsPerRotation = 1.8;
    static constexpr int maxChops = 32;

    void prepare (double sampleRate);
    void reset();

    /** Audio thread. The pointee must outlive its use (the processor guarantees this). */
    void setRecord (const RecordData* record);

    void process (juce::AudioBuffer<float>& output, const juce::MidiBuffer& midi,
                  const EngineParams& params, const HostState& host);

    /** Audio thread: requests coming from the UI. */
    void seekSeconds (double seconds);
    void jogRotations (double rotations);

    // Telemetry for the UI (written once per block, read from the message thread)
    std::atomic<double> uiPosition { 0.0 };        // wrapped, seconds
    std::atomic<double> uiRecordAngle { 0.0 };     // rotations, unwrapped
    std::atomic<double> uiPlatterAngle { 0.0 };    // rotations
    std::atomic<float>  uiSpeed { 0.0f };
    std::atomic<float>  uiFaderGain { 1.0f };
    std::atomic<bool>   uiTouching { false };
    std::atomic<float>  uiLevel { 0.0f };

private:
    struct Ghost
    {
        double position = 0, speed = 0;
        float gain = 0;
    };

    void renderSegment (float* left, float* right, int start, int num, const EngineParams& p);
    void handleMidi (const juce::MidiMessage& m, const EngineParams& p);
    void moveHand (double grooveSeconds, double autoTouchSeconds);
    void updateTouch();
    void refreshChops (const EngineParams& p);
    void restartForHostPlayback (const EngineParams& p);
    float faderLaw (float x, float curve) const noexcept;
    double platterTarget (const EngineParams& p) const noexcept;

    double sampleRate = 44100.0, dt = 1.0 / 44100.0;

    const RecordData* record = nullptr;
    SincReader reader;

    // Physical state
    double position = 0.0;        // seconds of groove under the needle (unwrapped)
    double speed = 0.0;           // record speed, 1 = 33 rpm at 0 %
    double platterSpeed = 0.0;
    double platterAngle = 0.0;
    double hand = 0.0;            // smoothed hand target position (seconds)
    double handMid = 0.0;         // first input-smoothing stage
    double handRaw = 0.0;         // hand target before input smoothing
    double handVelocity = 0.0;    // feed-forward from smooth automation ramps
    bool   touching = false;
    double currentHandTau = 0.01;
    double currentSensitivity = 1.0;

    // Touch sources
    bool paramTouch = false, holdTouch = false, ccTouch = false, noteTouch = false, bendTouch = false, handStopTouch = false;
    int  autoTouchSamples = 0;

    // Note / gate state
    bool  noteBrake = false, noteCut = false;
    int   cutNotesHeld = 0;
    int   lastChopNote = -1;
    float noteGainTarget = 1.0f, noteGain = 1.0f;

    // Controller state
    float faderValue = 1.0f, faderRampStep = 0.0f;
    int   faderRampRemaining = 0;
    float faderGain = 1.0f;
    float lastParamFader = -1.0f, lastParamScratch = 0.0f;
    bool  firstBlock = true;
    double lastBend = 0.0;
    double lastScratchCC = -2.0;
    double scratchRampStep = 0.0;
    float volL = 1.0f, volR = 1.0f;
    bool  wasHostPlaying = false;

    // Seek crossfade
    Ghost ghost;
    float mainGain = 1.0f;
    float fadeStep = 0.0f;

    // Phono stage, per channel + surface noise
    Biquad deEmph[3], subsonic[3];

    // Chops
    std::array<double, maxChops> chops {};
    int numChops = 0;
    const RecordData* chopsRecord = nullptr;
    int chopsMode = -1, chopsCount = -1;

    float levelFollower = 0.0f;
};
