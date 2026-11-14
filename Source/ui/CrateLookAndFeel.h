#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Palette
{
    inline const juce::Colour background { 0xff141518 };
    inline const juce::Colour plate      { 0xff23252a };
    inline const juce::Colour plateHi    { 0xff2e3137 };
    inline const juce::Colour panel      { 0xff1b1d21 };
    inline const juce::Colour well       { 0xff0f1012 };
    inline const juce::Colour text       { 0xffe8e2d6 };
    inline const juce::Colour dim        { 0xff8c8880 };
    inline const juce::Colour accent     { 0xfff0a830 };
    inline const juce::Colour red        { 0xffc8412c };
    inline const juce::Colour vinyl      { 0xff0c0c0e };
    inline const juce::Colour metal      { 0xffb9bcc2 };
}

class CrateLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CrateLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    static juce::Font uiFont (float height, bool bold = false);
};
