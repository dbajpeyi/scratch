#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamID
{
    // Performance (automatable)
    inline constexpr const char* touch       = "touch";
    inline constexpr const char* scratch     = "scratch";
    inline constexpr const char* fader       = "fader";
    inline constexpr const char* motor       = "motor";
    inline constexpr const char* pitch       = "pitch";

    // Deck feel
    inline constexpr const char* range       = "range";
    inline constexpr const char* handSmooth  = "handSmooth";
    inline constexpr const char* slipmat     = "slipmat";
    inline constexpr const char* startTime   = "startTime";
    inline constexpr const char* brakeTime   = "brakeTime";
    inline constexpr const char* rpm         = "rpm";
    inline constexpr const char* pitchRange  = "pitchRange";

    // Mixer
    inline constexpr const char* faderCurve  = "faderCurve";
    inline constexpr const char* hamster     = "hamster";
    inline constexpr const char* volume      = "volume";
    inline constexpr const char* pan         = "pan";

    // Sound
    inline constexpr const char* phono       = "phono";
    inline constexpr const char* noise       = "noise";
    inline constexpr const char* loop        = "loop";

    // Notes / chops
    inline constexpr const char* chopMode    = "chopMode";
    inline constexpr const char* slices      = "slices";
    inline constexpr const char* noteOff     = "noteOff";
    inline constexpr const char* noteStart   = "noteStart";
    inline constexpr const char* velocity    = "velocity";
    inline constexpr const char* hostSync    = "hostSync";

    // MIDI mapping (not automatable)
    inline constexpr const char* baseNote    = "baseNote";
    inline constexpr const char* touchNote   = "touchNote";
    inline constexpr const char* cutNote     = "cutNote";
    inline constexpr const char* touchCC     = "touchCC";
    inline constexpr const char* scratchCC   = "scratchCC";
    inline constexpr const char* faderCC     = "faderCC";
    inline constexpr const char* jogCC       = "jogCC";
    inline constexpr const char* jogEncoding = "jogEncoding";
    inline constexpr const char* jogTicks    = "jogTicks";
    inline constexpr const char* bendScratch = "bendScratch";
}

namespace Choices
{
    inline const juce::StringArray rpm        { "33 1/3", "45", "78" };
    inline const juce::StringArray pitchRange { "+/-8%", "+/-16%", "+/-50%" };
    inline const juce::StringArray chopMode   { "Transients", "Even" };
    inline const juce::StringArray noteOff    { "Hold", "Cut", "Hand Stop", "Brake" };
    inline const juce::StringArray noteStart  { "Instant", "Release" };
    inline const juce::StringArray jogEncoding{ "Offset 64", "Two's Comp", "Sign Bit" };
    inline const juce::StringArray slices     { "1", "2", "4", "8", "12", "16", "24", "32" };

    inline constexpr float rpmFactor[]  { 1.0f, 45.0f / (100.0f / 3.0f), 78.0f / (100.0f / 3.0f) };
    inline constexpr float pitchSpan[]  { 0.08f, 0.16f, 0.50f };
    inline constexpr int   sliceCount[] { 1, 2, 4, 8, 12, 16, 24, 32 };
}

/** 128 means "off" for the note / CC mapping parameters. */
inline constexpr int kMidiOff = 128;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
