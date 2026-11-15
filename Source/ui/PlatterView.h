#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class CrateCutProcessor;

/**
    The deck: platter, record, label and tone arm.

    Grab the record with the mouse to scratch. The drag drives the "Touch" and
    "Scratch" parameters through proper begin/end gestures, so enabling
    automation recording in the host captures the performance as editable
    automation. Trackpad / mouse-wheel swipes act like a jog wheel.
*/
class PlatterView : public juce::Component
{
public:
    explicit PlatterView (CrateCutProcessor&);
    ~PlatterView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void renderVinylCache();
    void drawPlatter (juce::Graphics&, float platterAngle);
    void drawLabel (juce::Graphics&, float recordAngle);
    void drawToneArm (juce::Graphics&, float progress);
    bool hitsRecord (juce::Point<float>) const;
    float angleAt (juce::Point<float>) const;

    CrateCutProcessor& processor;
    juce::RangedAudioParameter* touchParam = nullptr;
    juce::RangedAudioParameter* scratchParam = nullptr;
    juce::RangedAudioParameter* rangeParam = nullptr;
    juce::RangedAudioParameter* sensitivityParam = nullptr;

    juce::Point<float> centre;
    float platterRadius = 0, vinylRadius = 0, labelRadius = 0;
    juce::Point<float> armPivot;
    float armLength = 0;

    juce::Image vinylCache;
    float cacheScale = 0.0f;

    bool dragging = false;
    float lastAngle = 0.0f;
    float scratchValue = 0.0f;
};
