#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/CrateLookAndFeel.h"
#include "ui/PlatterView.h"
#include "ui/WaveformView.h"
#include "ui/TransportButton.h"
#include "ui/HostMenu.h"

class CrateCutEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       private juce::Timer
{
public:
    static constexpr int baseWidth = 800;
    static constexpr int baseHeight = 612;

    explicit CrateCutEditor (CrateCutProcessor&);
    ~CrateCutEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; content.repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragHover = false; content.repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    struct Combo
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    /** Everything is laid out at base size inside this, then scaled as one. */
    struct Content : public juce::Component
    {
        explicit Content (CrateCutEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        void paintOverChildren (juce::Graphics& g) override { owner.paintOverlay (g); }
        CrateCutEditor& owner;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void paintOverlay (juce::Graphics&);
    void setupKnob (Knob&, const char* paramId, const juce::String& name, const juce::String& tip, bool bipolar = false);
    void setupToggle (Toggle&, const char* paramId, const juce::String& name, const juce::String& tip);
    void setupCombo (Combo&, const char* paramId, const juce::String& name, const juce::String& tip);
    void placeKnob (Knob&, juce::Rectangle<int>);
    void placeCombo (Combo&, juce::Rectangle<int>);
    void openFileChooser();
    void loadFile (const juce::File&);
    void showStatus (const juce::String&, bool isError);
    void setRpm (int index);
    void syncButtons();
    void setTransport (bool motorOn, bool reverse);
    void setParam (const char* id, float value);

    CrateCutProcessor& proc;
    CrateLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    Content content { *this };

    PlatterView platter;
    WaveformView waveform;

    Knob smoothHand, smoothInput, smoothSlip, smoothFast;
    Knob range, noise, speed, accel, sensitivity, brakeTime, faderCurve, volume, pan;
    Toggle phono, loop, hostSync, hamster, velocity, hold, mute;
    Combo chopMode, slices, noteOff, noteStart, pitchRange;

    juce::Slider pitch { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Slider crossfader { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<APVTS::SliderAttachment> pitchAttachment, faderAttachment;

    TransportButton playBackwards { TransportButton::Icon::backwards, "Play backwards" };
    TransportButton pauseButton   { TransportButton::Icon::pause, "Pause" };
    TransportButton playForwards  { TransportButton::Icon::forwards, "Play forwards" };
    juce::TextButton rpmButtons[3];
    std::vector<std::unique_ptr<HostMenuOnRightClick>> hostMenus;
    juce::TextButton loadButton { "LOAD" }, sizeButton { "1x" };

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status;
    bool statusIsError = false;
    juce::uint32 statusTime = 0;
    bool dragHover = false;
    int lastRecordVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrateCutEditor)
};
