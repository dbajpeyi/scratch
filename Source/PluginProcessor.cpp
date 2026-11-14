#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/DefaultSample.h"

using namespace juce;

namespace
{
    constexpr double maxEmbedSeconds = 90.0;
}

CrateCutProcessor::CrateCutProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CrateCut", createParameterLayout())
{
    formats.registerBasicFormats();

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    raw = { get (ParamID::touch), get (ParamID::scratch), get (ParamID::fader), get (ParamID::motor),
            get (ParamID::pitch), get (ParamID::range), get (ParamID::handSmooth), get (ParamID::slipmat),
            get (ParamID::startTime), get (ParamID::brakeTime), get (ParamID::rpm), get (ParamID::pitchRange),
            get (ParamID::faderCurve), get (ParamID::hamster), get (ParamID::volume), get (ParamID::pan),
            get (ParamID::phono), get (ParamID::noise), get (ParamID::loop), get (ParamID::chopMode),
            get (ParamID::slices), get (ParamID::noteOff), get (ParamID::noteStart), get (ParamID::velocity),
            get (ParamID::hostSync), get (ParamID::baseNote), get (ParamID::touchNote), get (ParamID::cutNote),
            get (ParamID::touchCC), get (ParamID::scratchCC), get (ParamID::faderCC), get (ParamID::jogCC),
            get (ParamID::jogEncoding), get (ParamID::jogTicks), get (ParamID::bendScratch) };

    loadDefaultSample();
    startTimer (500);
}

CrateCutProcessor::~CrateCutProcessor()
{
    stopTimer();
}

//==============================================================================
void CrateCutProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool CrateCutProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

EngineParams CrateCutProcessor::readParams() const
{
    EngineParams p;
    auto b = [] (std::atomic<float>* v) { return v->load() >= 0.5f; };
    auto i = [] (std::atomic<float>* v) { return (int) std::lround (v->load()); };

    p.touch       = b (raw.touch);
    p.scratch     = raw.scratch->load();
    p.rangeRot    = raw.range->load();
    p.fader       = raw.fader->load();
    p.faderCurve  = raw.faderCurve->load();
    p.hamster     = b (raw.hamster);
    p.motor       = b (raw.motor);
    p.rpmFactor   = Choices::rpmFactor[jlimit (0, 2, i (raw.rpm))];
    p.pitch       = raw.pitch->load() * Choices::pitchSpan[jlimit (0, 2, i (raw.pitchRange))];
    p.startTime   = raw.startTime->load();
    p.brakeTime   = raw.brakeTime->load();
    p.slipTime    = raw.slipmat->load() * 0.001f;
    p.handTau     = raw.handSmooth->load() * 0.001f;
    p.phono       = b (raw.phono);
    p.noise       = raw.noise->load();
    p.loop        = b (raw.loop);
    p.chopMode    = i (raw.chopMode);
    p.slices      = Choices::sliceCount[jlimit (0, 7, i (raw.slices))];
    p.noteOff     = i (raw.noteOff);
    p.noteStart   = i (raw.noteStart);
    p.velocity    = b (raw.velocity);
    p.hostSync    = b (raw.hostSync);
    p.volumeDb    = raw.volume->load();
    p.pan         = raw.pan->load();
    p.baseNote    = i (raw.baseNote);
    p.touchNote   = i (raw.touchNote);
    p.cutNote     = i (raw.cutNote);
    p.touchCC     = i (raw.touchCC);
    p.scratchCC   = i (raw.scratchCC);
    p.faderCC     = i (raw.faderCC);
    p.jogCC       = i (raw.jogCC);
    p.jogEncoding = i (raw.jogEncoding);
    p.jogTicks    = raw.jogTicks->load();
    p.bendScratch = b (raw.bendScratch);
    return p;
}

void CrateCutProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;

    {
        const SpinLock::ScopedTryLockType lock (recordLock);
        if (lock.isLocked() && pendingRecord != nullptr)
        {
            activeRecord = pendingRecord; // old record stays alive in keepAlive
            pendingRecord = nullptr;
        }
    }
    engine.setRecord (activeRecord.get());

    const double seek = pendingSeek.exchange (std::numeric_limits<double>::quiet_NaN());
    if (! std::isnan (seek))
        engine.seekSeconds (seek);

    const float jog = pendingJog.exchange (0.0f);
    if (std::abs (jog) > 0.0f)
        engine.jogRotations (jog);

    HostState host;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            host.valid = true;
            host.playing = pos->getIsPlaying();
        }

    engine.process (buffer, midi, readParams(), host);
    midi.clear();
}

void CrateCutProcessor::addJog (float rotations)
{
    float expected = pendingJog.load();
    while (! pendingJog.compare_exchange_weak (expected, expected + rotations)) {}
}

//==============================================================================
void CrateCutProcessor::setRecord (RecordData::Ptr newRecord, MemoryBlock embedded)
{
    {
        const ScopedLock sl (stateLock);
        currentRecord = newRecord;
        currentEmbedded = std::move (embedded);
        keepAlive.add (newRecord);
    }
    {
        const SpinLock::ScopedLockType lock (recordLock);
        pendingRecord = newRecord;
    }
    ++recordVersion;
}

RecordData::Ptr CrateCutProcessor::getRecord() const
{
    const ScopedLock sl (stateLock);
    return currentRecord;
}

String CrateCutProcessor::loadSample (const File& file)
{
    String error;
    auto rec = RecordData::loadFromFile (file, formats, error);
    if (rec == nullptr)
        return error;

    MemoryBlock embedded;
    if (rec->getDurationSeconds() <= maxEmbedSeconds)
        rec->encodeToFlac (embedded);

    setRecord (rec, std::move (embedded));
    return {};
}

void CrateCutProcessor::loadDefaultSample()
{
    setRecord (createDefaultRecord(), {});
}

void CrateCutProcessor::timerCallback()
{
    // Free records the audio thread no longer uses (never on the audio thread).
    const ScopedLock sl (stateLock);
    for (int i = keepAlive.size(); --i >= 0;)
    {
        auto* r = keepAlive.getObjectPointerUnchecked (i);
        if (r != currentRecord.get() && r->getReferenceCount() == 1)
            keepAlive.remove (i);
    }
}

//==============================================================================
void CrateCutProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());

    auto* sample = xml->createNewChildElement ("Sample");
    {
        const ScopedLock sl (stateLock);
        if (currentRecord != nullptr && currentRecord->sourceFile != File())
        {
            sample->setAttribute ("path", currentRecord->sourceFile.getFullPathName());
            sample->setAttribute ("name", currentRecord->name);
            if (currentEmbedded.getSize() > 0)
                sample->setAttribute ("flac", currentEmbedded.toBase64Encoding());
        }
    }
    xml->setAttribute ("uiScale", uiScale);

    copyXmlToBinary (*xml, destData);
}

void CrateCutProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    uiScale = (float) xml->getDoubleAttribute ("uiScale", 1.0);

    std::unique_ptr<XmlElement> sample;
    if (auto* s = xml->getChildByName ("Sample"))
    {
        sample = std::make_unique<XmlElement> (*s);
        xml->removeChildElement (s, true);
    }

    apvts.replaceState (ValueTree::fromXml (*xml));

    if (sample == nullptr || ! sample->hasAttribute ("path"))
    {
        loadDefaultSample();
        return;
    }

    const File file (sample->getStringAttribute ("path"));
    if (file.existsAsFile() && loadSample (file).isEmpty())
        return;

    // File moved or missing: fall back to the copy embedded in the project.
    MemoryBlock flac;
    if (flac.fromBase64Encoding (sample->getStringAttribute ("flac")) && flac.getSize() > 0)
    {
        if (auto rec = RecordData::loadFromMemory (flac, formats, sample->getStringAttribute ("name", "Embedded"), file))
        {
            setRecord (rec, std::move (flac));
            return;
        }
    }

    loadDefaultSample();
}

//==============================================================================
AudioProcessorEditor* CrateCutProcessor::createEditor()
{
    return new CrateCutEditor (*this);
}

#ifndef CRATECUT_TEST_BUILD
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrateCutProcessor();
}
#endif
