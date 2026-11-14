#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Parameters.h"
#include "dsp/RecordData.h"
#include "dsp/TurntableEngine.h"

class CrateCutProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    CrateCutProcessor();
    ~CrateCutProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Message thread. Returns an error message, or empty on success. */
    juce::String loadSample (const juce::File& file);
    void loadDefaultSample();

    /** The record most recently loaded (message thread). */
    RecordData::Ptr getRecord() const;
    int getRecordVersion() const noexcept { return recordVersion.load(); }

    /** UI -> audio requests. */
    void requestSeek (double seconds) { pendingSeek.store (seconds); }
    void addJog (float rotations);

    EngineParams readParams() const;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formats;
    TurntableEngine engine;

    float uiScale = 1.0f; // persisted editor zoom

private:
    void timerCallback() override;
    void setRecord (RecordData::Ptr newRecord, juce::MemoryBlock embedded);

    // Record hand-off: built on the message thread, swapped in on the audio thread.
    juce::SpinLock recordLock;
    RecordData::Ptr pendingRecord, activeRecord;
    juce::ReferenceCountedArray<RecordData> keepAlive; // ensures records are freed off the audio thread
    juce::CriticalSection stateLock;
    RecordData::Ptr currentRecord;
    juce::MemoryBlock currentEmbedded;
    std::atomic<int> recordVersion { 0 };

    std::atomic<double> pendingSeek { std::numeric_limits<double>::quiet_NaN() };
    std::atomic<float> pendingJog { 0.0f };

    struct RawParams
    {
        std::atomic<float> *touch, *scratch, *fader, *motor, *pitch, *range, *handSmooth, *slipmat,
                           *startTime, *brakeTime, *rpm, *pitchRange, *faderCurve, *hamster, *volume, *pan,
                           *phono, *noise, *loop, *chopMode, *slices, *noteOff, *noteStart, *velocity, *hostSync,
                           *baseNote, *touchNote, *cutNote, *touchCC, *scratchCC, *faderCC, *jogCC, *jogEncoding,
                           *jogTicks, *bendScratch;
    } raw {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrateCutProcessor)
};
