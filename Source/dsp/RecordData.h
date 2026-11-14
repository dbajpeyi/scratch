#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>

/**
    An immutable "pressed record": everything the audio thread needs to play a
    sample like vinyl. Built on the message thread, handed to the audio thread
    by reference-counted pointer, never modified afterwards.
*/
struct RecordData : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<RecordData>;

    juce::String name;
    juce::File sourceFile;           // empty for the built-in sample
    double sampleRate = 44100.0;

    juce::AudioBuffer<float> clean;  // 2 channels, the audio as loaded
    juce::AudioBuffer<float> groove; // 2 channels, RIAA pre-emphasised ("cut to vinyl")
    std::vector<float> surface;      // mono surface noise in the groove domain (hiss + crackle)

    struct Peak { float min, max; };
    std::vector<Peak> overview;      // for the waveform display

    struct Onset { double time; float strength; };
    std::vector<Onset> onsets;       // sorted by strength, strongest first

    int64_t getLength() const noexcept { return clean.getNumSamples(); }
    double getDurationSeconds() const noexcept { return (double) getLength() / sampleRate; }

    /** Builds a record from raw audio (any channel count; mono is duplicated). */
    static Ptr create (const juce::AudioBuffer<float>& audio, double sampleRate,
                       const juce::String& name, const juce::File& source = {});

    /** Loads an audio file. Returns nullptr and fills error on failure. */
    static Ptr loadFromFile (const juce::File& file, juce::AudioFormatManager& formats, juce::String& error);

    /** Loads audio from an in-memory encoded file (used for samples embedded in the session). */
    static Ptr loadFromMemory (const juce::MemoryBlock& data, juce::AudioFormatManager& formats,
                               const juce::String& name, const juce::File& originalFile);

    /** Encodes the clean audio as FLAC for embedding into the plugin state. */
    bool encodeToFlac (juce::MemoryBlock& dest) const;
};

/** Chop points (seconds) for the given mode: 0 = transients, 1 = even. Writes up to maxCount, returns count. */
int computeChopPoints (const RecordData& record, int mode, int count, double* dest, int maxCount);
