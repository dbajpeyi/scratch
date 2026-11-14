#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/RecordData.h"

class CrateCutProcessor;

/** Sample overview with playhead and chop markers. Click to drop the needle. */
class WaveformView : public juce::Component
{
public:
    explicit WaveformView (CrateCutProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override { waveCache = {}; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    void refresh();

private:
    void seekTo (float x);

    CrateCutProcessor& processor;
    RecordData::Ptr record;
    juce::Image waveCache;
    int cachedChopMode = -1, cachedSlices = -1;
    std::array<double, 32> chops {};
    int numChops = 0;
};
