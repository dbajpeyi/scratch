#include "Parameters.h"

using namespace juce;

namespace
{
    ParameterID pid (const char* id) { return ParameterID { id, 1 }; }

    String midiNoteName (int v, int)
    {
        if (v >= kMidiOff) return "Off";
        return MidiMessage::getMidiNoteName (v, true, true, 3) + " (" + String (v) + ")";
    }

    String ccName (int v, int)
    {
        if (v >= kMidiOff) return "Off";
        return "CC " + String (v);
    }

    std::unique_ptr<AudioParameterInt> midiParam (const char* id, const String& name, int def, bool isNote)
    {
        AudioParameterIntAttributes attrs;
        attrs = attrs.withAutomatable (false)
                     .withStringFromValueFunction (isNote ? midiNoteName : ccName);
        return std::make_unique<AudioParameterInt> (pid (id), name, 0, kMidiOff, def, attrs);
    }

    std::unique_ptr<AudioParameterFloat> floatParam (const char* id, const String& name,
                                                     NormalisableRange<float> range, float def,
                                                     const String& label, int decimals = 2)
    {
        AudioParameterFloatAttributes attrs;
        attrs = attrs.withLabel (label)
                     .withStringFromValueFunction ([decimals] (float v, int) { return String (v, decimals); });
        return std::make_unique<AudioParameterFloat> (pid (id), name, range, def, attrs);
    }

    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }
}

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    // --- Performance -------------------------------------------------------
    layout.add (std::make_unique<AudioParameterBool> (pid (ParamID::touch), "Touch", false));
    layout.add (floatParam (ParamID::scratch, "Scratch", { -1.0f, 1.0f }, 0.0f, "", 3));
    layout.add (floatParam (ParamID::fader, "Crossfader", { 0.0f, 1.0f }, 1.0f, "", 3));
    layout.add (std::make_unique<AudioParameterBool> (pid (ParamID::motor), "Start/Stop", true));
    layout.add (floatParam (ParamID::pitch, "Pitch", { -1.0f, 1.0f }, 0.0f, "", 3));

    // --- Deck feel ----------------------------------------------------------
    layout.add (floatParam (ParamID::range, "Scratch Range", skewed (0.05f, 2.0f, 0.5f), 0.5f, "rot"));
    layout.add (floatParam (ParamID::handSmooth, "Hand Smoothing", skewed (1.0f, 60.0f, 10.0f), 10.0f, "ms", 1));
    layout.add (floatParam (ParamID::slipmat, "Slipmat Grip", skewed (5.0f, 400.0f, 50.0f), 45.0f, "ms", 0));
    layout.add (floatParam (ParamID::startTime, "Start Time", skewed (0.02f, 4.0f, 0.7f), 0.7f, "s"));
    layout.add (floatParam (ParamID::brakeTime, "Brake Time", skewed (0.02f, 6.0f, 1.0f), 1.0f, "s"));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamID::rpm), "Speed", Choices::rpm, 0));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamID::pitchRange), "Pitch Range", Choices::pitchRange, 0));

    // --- Mixer --------------------------------------------------------------
    layout.add (floatParam (ParamID::faderCurve, "Fader Curve", { 0.0f, 1.0f }, 0.85f, ""));
    layout.add (std::make_unique<AudioParameterBool> (pid (ParamID::hamster), "Hamster", false));
    layout.add (floatParam (ParamID::volume, "Volume", { -60.0f, 12.0f }, 0.0f, "dB", 1));
    layout.add (floatParam (ParamID::pan, "Pan", { -1.0f, 1.0f }, 0.0f, ""));

    // --- Sound --------------------------------------------------------------
    layout.add (std::make_unique<AudioParameterBool> (pid (ParamID::phono), "Phono Model", true));
    layout.add (floatParam (ParamID::noise, "Vinyl Noise", { 0.0f, 1.0f }, 0.2f, ""));
    layout.add (std::make_unique<AudioParameterBool> (pid (ParamID::loop), "Loop Record", true));

    // --- Notes / chops ------------------------------------------------------
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamID::chopMode), "Chop Mode", Choices::chopMode, 0));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamID::slices), "Slices", Choices::slices, 3));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamID::noteOff), "Note Off", Choices::noteOff, 0));
    layout.add (std::make_unique<AudioParameterChoice> (pid (ParamID::noteStart), "Note Start", Choices::noteStart, 0));
    layout.add (std::make_unique<AudioParameterBool> (pid (ParamID::velocity), "Velocity Sens", false));
    layout.add (std::make_unique<AudioParameterBool> (pid (ParamID::hostSync), "Host Sync", false));

    // --- MIDI mapping -------------------------------------------------------
    layout.add (midiParam (ParamID::baseNote,  "First Chop Note", 60, true));
    layout.add (midiParam (ParamID::touchNote, "Touch Note", 48, true));
    layout.add (midiParam (ParamID::cutNote,   "Cut Note", 50, true));
    layout.add (midiParam (ParamID::touchCC,   "Touch CC", 64, false));
    layout.add (midiParam (ParamID::scratchCC, "Scratch CC", 1, false));
    layout.add (midiParam (ParamID::faderCC,   "Fader CC", 11, false));
    layout.add (midiParam (ParamID::jogCC,     "Jog CC", 16, false));
    {
        AudioParameterChoiceAttributes attrs;
        layout.add (std::make_unique<AudioParameterChoice> (pid (ParamID::jogEncoding), "Jog Encoding",
                                                            Choices::jogEncoding, 0, attrs.withAutomatable (false)));
    }
    {
        AudioParameterFloatAttributes attrs;
        layout.add (std::make_unique<AudioParameterFloat> (pid (ParamID::jogTicks), "Jog Ticks/Rev",
                                                           skewed (16.0f, 4096.0f, 256.0f), 256.0f,
                                                           attrs.withAutomatable (false)
                                                                .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); })));
    }
    {
        AudioParameterBoolAttributes attrs;
        layout.add (std::make_unique<AudioParameterBool> (pid (ParamID::bendScratch), "Pitch Bend Scratch", true,
                                                          attrs.withAutomatable (false)));
    }

    return layout;
}
