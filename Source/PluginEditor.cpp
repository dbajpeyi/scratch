#include "PluginEditor.h"

using namespace juce;

namespace
{
    const String audioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf";

    void drawSectionLabel (Graphics& g, const String& text, int x, int y)
    {
        g.setColour (Palette::dim);
        g.setFont (CrateLookAndFeel::uiFont (10.5f, true));
        g.drawText (text, x, y, 200, 12, Justification::centredLeft);
    }
}

CrateCutEditor::CrateCutEditor (CrateCutProcessor& p)
    : AudioProcessorEditor (p), proc (p), platter (p), waveform (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (platter);
    content.addAndMakeVisible (waveform);

    setupKnob (smoothHand,  ParamID::handSmooth,  "1 HAND",  "Smooth 1 - Hand: the give between your hand and the record. Low = tight and twitchy, high = soft, rounded strokes.");
    setupKnob (smoothInput, ParamID::inputSmooth, "2 INPUT", "Smooth 2 - Input: interpolates between control points (mouse moves, MIDI steps, automation). Raise it for a coarse mouse or 7-bit CC, keep it near 0 for a high-resolution mouse or tablet.");
    setupKnob (smoothSlip,  ParamID::slipmat,     "3 SLIP",  "Smooth 3 - Slipmat: how fast the spinning platter drags the record back up to speed when you let go.");
    setupKnob (smoothFast,  ParamID::fastSmooth,  "4 FAST",  "Smooth 4 - Fast cuts: softens the top end on very fast movements, taking the fizz off hard flicks.");
    setupKnob (range,       ParamID::range,       "RANGE",   "Scratch range: how far the record travels for the full Scratch parameter throw (in rotations).");
    setupKnob (noise,       ParamID::noise,       "NOISE",   "Surface noise and crackle. It lives in the groove, so it scratches with the record.");
    setupKnob (speed,       ParamID::speed,       "SPEED",   "Playback speed (1x = normal). Works on top of the 33/45/78 buttons and the pitch fader.");
    setupKnob (accel,       ParamID::accel,       "ACC",     "Acceleration: how quickly the motor brings the platter up to speed.");
    setupKnob (sensitivity, ParamID::sensitivity, "SEN",     "Sensitivity: how far the record moves for a given mouse, jog wheel, pitch bend or CC movement.");
    setupKnob (brakeTime,   ParamID::brakeTime,   "BRAKE",   "Brake time when the deck is paused.");
    setupKnob (faderCurve,  ParamID::faderCurve,  "CURVE",   "Crossfader curve: right = sharp scratch-mixer cut, left = smooth.");
    setupKnob (volume,      ParamID::volume,      "VOL",     "Output level.");
    setupKnob (pan,         ParamID::pan,         "PAN",     "Balance.", true);

    setupToggle (phono,    ParamID::phono,    "PHONO",   "Phono model: magnetic cartridge + RIAA physics. Level follows hand speed - slow drags fade, fast cuts punch - like real vinyl.");
    setupToggle (loop,     ParamID::loop,     "LOOP",    "Loop the record (locked groove). Off = silence past the ends.");
    setupToggle (hostSync, ParamID::hostSync, "SYNC",    "Motor follows the host transport, and the record resets on play so renders are repeatable.");
    setupToggle (hamster,  ParamID::hamster,  "HAMSTER", "Reverse the crossfader direction.");
    setupToggle (velocity, ParamID::velocity, "VELOCITY","Note velocity sets the level of each chop.");
    setupToggle (hold,     ParamID::hold,     "HOLD",    "Hold the record still. Turn it on when linking the platter (Scratch) to a controller: the controller then moves the record directly.");
    setupToggle (mute,     ParamID::mute,     "MUTE",    "Silence the deck (click-free). Handy for automation.");

    setupCombo (chopMode,   ParamID::chopMode,   "CHOP",       "Where the note chops land: detected transients or even slices.");
    setupCombo (slices,     ParamID::slices,     "SLICES",     "Number of chops mapped up the keyboard from the First Chop Note.");
    setupCombo (noteOff,    ParamID::noteOff,    "NOTE OFF",   "What releasing a chop key does: keep playing, cut the fader, stop it by hand, or brake.");
    setupCombo (noteStart,  ParamID::noteStart,  "NOTE START", "Instant: record already moving. Release: held at the cue and let go, so the slipmat pulls it up to speed.");
    setupCombo (pitchRange, ParamID::pitchRange, "",           "Pitch fader range.");

    pitch.getProperties().set ("bipolar", true);
    pitch.setDoubleClickReturnValue (true, 0.0);
    pitch.setTooltip ("Pitch fader (double-click to reset). Up is faster, like the deck slider pulled towards you.");
    content.addAndMakeVisible (pitch);
    pitchAttachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, ParamID::pitch, pitch);

    crossfader.setTooltip ("Crossfader. Automate or map a CC to it for transformers, chirps and flares.");
    content.addAndMakeVisible (crossfader);
    faderAttachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, ParamID::fader, crossfader);

    playBackwards.setTooltip ("Play backwards (motor in reverse).");
    pauseButton.setTooltip ("Pause: the motor stops and the platter brakes to a halt.");
    playForwards.setTooltip ("Play forwards.");
    playBackwards.onClick = [this] { setTransport (true, true); };
    pauseButton.onClick   = [this] { setTransport (false, proc.apvts.getRawParameterValue (ParamID::reverse)->load() >= 0.5f); };
    playForwards.onClick  = [this] { setTransport (true, false); };
    for (auto* b : { &playBackwards, &pauseButton, &playForwards })
        content.addAndMakeVisible (b);

    // Right-click any of these for the host's parameter menu (FL: "Link to controller").
    hostMenus.push_back (std::make_unique<HostMenuOnRightClick> (pitch, proc.apvts.getParameter (ParamID::pitch)));
    hostMenus.push_back (std::make_unique<HostMenuOnRightClick> (crossfader, proc.apvts.getParameter (ParamID::fader)));

    const char* rpmNames[] = { "33", "45", "78" };
    for (int i = 0; i < 3; ++i)
    {
        rpmButtons[i].setButtonText (rpmNames[i]);
        rpmButtons[i].onClick = [this, i] { setRpm (i); };
        content.addAndMakeVisible (rpmButtons[i]);
    }

    loadButton.setTooltip ("Load a sample (or drag an audio file onto the plugin).");
    loadButton.onClick = [this] { openFileChooser(); };
    content.addAndMakeVisible (loadButton);

    sizeButton.setTooltip ("Window size");
    sizeButton.onClick = [this]
    {
        const float scales[] = { 1.0f, 1.5f, 2.0f };
        const float current = (float) getWidth() / baseWidth;
        float next = scales[0];
        for (float s : scales)
            if (s > current + 0.05f) { next = s; break; }
        setSize (roundToInt (baseWidth * next), roundToInt (baseHeight * next));
    };
    content.addAndMakeVisible (sizeButton);

    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 5 / 2, baseHeight * 5 / 2);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / baseHeight);
    const float scale = jlimit (0.75f, 2.5f, proc.uiScale);
    setSize (roundToInt (baseWidth * scale), roundToInt (baseHeight * scale));

    syncButtons();
    startTimerHz (60);
}

CrateCutEditor::~CrateCutEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void CrateCutEditor::setupKnob (Knob& k, const char* id, const String& name, const String& tip, bool bipolar)
{
    k.slider.getProperties().set ("bipolar", bipolar);
    k.slider.setPopupDisplayEnabled (true, true, this);
    k.slider.setTooltip (tip);
    k.label.setText (name, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setFont (CrateLookAndFeel::uiFont (11.0f, true));
    k.label.setColour (Label::textColourId, Palette::dim);
    k.label.setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (k.slider);
    content.addAndMakeVisible (k.label);
    k.attachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, k.slider);
    hostMenus.push_back (std::make_unique<HostMenuOnRightClick> (k.slider, proc.apvts.getParameter (id)));
}

void CrateCutEditor::setupToggle (Toggle& t, const char* id, const String& name, const String& tip)
{
    t.button.setButtonText (name);
    t.button.setTooltip (tip);
    content.addAndMakeVisible (t.button);
    t.attachment = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, t.button);
}

void CrateCutEditor::setupCombo (Combo& c, const char* id, const String& name, const String& tip)
{
    if (auto* choice = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c.box.addItemList (choice->choices, 1);
    c.box.setTooltip (tip);
    c.label.setText (name, dontSendNotification);
    c.label.setFont (CrateLookAndFeel::uiFont (10.5f, true));
    c.label.setColour (Label::textColourId, Palette::dim);
    content.addAndMakeVisible (c.box);
    content.addAndMakeVisible (c.label);
    c.attachment = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, c.box);
}

void CrateCutEditor::placeKnob (Knob& k, Rectangle<int> r)
{
    k.label.setBounds (r.removeFromBottom (14));
    k.slider.setBounds (r);
}

void CrateCutEditor::placeCombo (Combo& c, Rectangle<int> r)
{
    c.label.setBounds (r.removeFromTop (14).withTrimmedLeft (-2));
    c.box.setBounds (r);
}

//==============================================================================
void CrateCutEditor::resized()
{
    const float scale = (float) getWidth() / baseWidth;
    content.setBounds (0, 0, baseWidth, baseHeight);
    content.setTransform (AffineTransform::scale (scale));
    proc.uiScale = scale;
    sizeButton.setButtonText (scale > 1.75f ? "2x" : scale > 1.25f ? "1.5x" : "1x");

    // top bar
    loadButton.setBounds (196, 11, 64, 26);
    placeCombo (chopMode, { 556, 4, 104, 38 });
    placeCombo (slices,   { 666, 4, 60, 38 });
    sizeButton.setBounds (740, 11, 46, 26);

    // left column: smoothing, pan/vol, hold/mute
    placeKnob (smoothHand,  { 14, 74, 54, 62 });
    placeKnob (smoothInput, { 68, 74, 54, 62 });
    placeKnob (smoothSlip,  { 14, 140, 54, 62 });
    placeKnob (smoothFast,  { 68, 140, 54, 62 });
    placeKnob (pan,    { 18, 210, 100, 80 });
    placeKnob (volume, { 18, 294, 100, 80 });
    hold.button.setBounds (22, 384, 92, 25);
    mute.button.setBounds (22, 414, 92, 25);

    platter.setBounds (124, 58, 470, 392);

    // right column: speed, pitch, motor
    for (int i = 0; i < 3; ++i)
        rpmButtons[i].setBounds (606 + i * 58, 68, 52, 26);
    pitch.setBounds (608, 112, 56, 214);
    pitchRange.box.setBounds (602, 340, 70, 22);
    placeKnob (speed,       { 682, 104, 96, 66 });
    placeKnob (accel,       { 682, 170, 96, 66 });
    placeKnob (sensitivity, { 682, 236, 96, 66 });
    placeKnob (brakeTime,   { 682, 302, 96, 66 });
    placeCombo (noteOff,   { 602, 376, 86, 36 });
    placeCombo (noteStart, { 694, 376, 86, 36 });
    velocity.button.setBounds (602, 418, 104, 24);

    // bottom strip: transport, switches, crossfader
    playBackwards.setBounds (20, 472, 30, 40);
    pauseButton.setBounds   (53, 472, 30, 40);
    playForwards.setBounds  (86, 472, 30, 40);
    phono.button.setBounds (124, 470, 84, 25);
    loop.button.setBounds (124, 501, 84, 25);
    hostSync.button.setBounds (214, 470, 88, 25);
    hamster.button.setBounds (214, 501, 88, 25);
    crossfader.setBounds (314, 468, 236, 44);
    placeKnob (faderCurve, { 556, 464, 62, 66 });
    placeKnob (range,      { 622, 464, 76, 66 });
    placeKnob (noise,      { 702, 464, 76, 66 });

    waveform.setBounds (10, 542, 780, 62);
}

void CrateCutEditor::paint (Graphics& g)
{
    g.fillAll (Palette::background);
}

void CrateCutEditor::paintContent (Graphics& g)
{
    g.fillAll (Palette::background);

    // top bar
    g.setColour (Palette::accent);
    g.setFont (CrateLookAndFeel::uiFont (22.0f, true));
    g.drawText ("CRATECUT", 16, 8, 140, 24, Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (CrateLookAndFeel::uiFont (10.5f, true));
    g.drawText ("SCRATCH DECK", 17, 30, 140, 12, Justification::centredLeft);

    g.setFont (CrateLookAndFeel::uiFont (13.0f));
    if (status.isNotEmpty() && Time::getMillisecondCounter() - statusTime < 4000)
    {
        g.setColour (statusIsError ? Palette::red.brighter (0.4f) : Palette::accent);
        g.drawFittedText (status, 270, 8, 280, 32, Justification::centredLeft, 2);
    }
    else if (auto rec = proc.getRecord())
    {
        g.setColour (Palette::text);
        g.drawFittedText (rec->name, 270, 8, 280, 18, Justification::centredLeft, 1);
        g.setColour (Palette::dim);
        g.setFont (CrateLookAndFeel::uiFont (10.5f));
        g.drawText (String (rec->getDurationSeconds(), 2) + " s  \xc2\xb7  " + String (rec->sampleRate / 1000.0, 1) + " kHz",
                    270, 26, 280, 14, Justification::centredLeft);
    }

    // deck plate
    const auto plate = Rectangle<float> (10.0f, 52.0f, 780.0f, 402.0f);
    g.setGradientFill (ColourGradient (Palette::plateHi, plate.getX(), plate.getY(), Palette::plate, plate.getX(), plate.getBottom(), false));
    g.fillRoundedRectangle (plate, 8.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (plate.reduced (0.5f), 8.0f, 1.0f);

    // right column divider + labels
    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillRect (598.0f, 62.0f, 1.0f, 382.0f);
    drawSectionLabel (g, "SMOOTH", 22, 62);
    drawSectionLabel (g, "PITCH", 610, 98);

    // bottom strip
    const auto strip = Rectangle<float> (10.0f, 460.0f, 780.0f, 76.0f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (strip, 8.0f);
    g.setColour (Palette::dim);
    g.setFont (CrateLookAndFeel::uiFont (10.5f, true));
    g.drawText ("CROSSFADER", 314, 516, 236, 14, Justification::centred);
    g.drawText ("TRANSPORT", 16, 516, 104, 14, Justification::centred);
}

void CrateCutEditor::paintOverlay (Graphics& g)
{
    if (dragHover)
    {
        g.setColour (Palette::accent.withAlpha (0.12f));
        g.fillRect (content.getLocalBounds());
        g.setColour (Palette::accent);
        g.drawRect (content.getLocalBounds().reduced (2), 3);
        g.setFont (CrateLookAndFeel::uiFont (24.0f, true));
        g.drawText ("Drop to press a new record", content.getLocalBounds(), Justification::centred);
    }
}

//==============================================================================
void CrateCutEditor::timerCallback()
{
    platter.repaint();
    waveform.refresh();
    waveform.repaint();

    syncButtons();

    if (proc.getRecordVersion() != lastRecordVersion)
    {
        lastRecordVersion = proc.getRecordVersion();
        content.repaint (0, 0, baseWidth, 50);
    }
    if (status.isNotEmpty() && Time::getMillisecondCounter() - statusTime > 4000)
    {
        status.clear();
        content.repaint (0, 0, baseWidth, 50);
    }
}

void CrateCutEditor::syncButtons()
{
    const int rpmIndex = (int) proc.apvts.getRawParameterValue (ParamID::rpm)->load();
    for (int i = 0; i < 3; ++i)
        rpmButtons[i].setToggleState (i == rpmIndex, dontSendNotification);

    const bool motorOn = proc.apvts.getRawParameterValue (ParamID::motor)->load() >= 0.5f;
    const bool reverse = proc.apvts.getRawParameterValue (ParamID::reverse)->load() >= 0.5f;
    playBackwards.setToggleState (motorOn && reverse, dontSendNotification);
    pauseButton.setToggleState (! motorOn, dontSendNotification);
    playForwards.setToggleState (motorOn && ! reverse, dontSendNotification);
}

void CrateCutEditor::setParam (const char* id, float value)
{
    if (auto* param = proc.apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
}

void CrateCutEditor::setRpm (int index)
{
    setParam (ParamID::rpm, (float) index);
}

void CrateCutEditor::setTransport (bool motorOn, bool reverse)
{
    setParam (ParamID::reverse, reverse ? 1.0f : 0.0f);
    setParam (ParamID::motor, motorOn ? 1.0f : 0.0f);
}

void CrateCutEditor::openFileChooser()
{
    chooser = std::make_unique<FileChooser> ("Load a sample to press", File(), audioWildcard);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile())
                                  loadFile (file);
                          });
}

void CrateCutEditor::loadFile (const File& file)
{
    const auto error = proc.loadSample (file);
    if (error.isNotEmpty())
        showStatus (error, true);
    else
        showStatus ("Pressed " + file.getFileName(), false);
}

void CrateCutEditor::showStatus (const String& text, bool isError)
{
    status = text;
    statusIsError = isError;
    statusTime = Time::getMillisecondCounter();
    content.repaint (0, 0, baseWidth, 50);
}

bool CrateCutEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension (audioWildcard))
            return true;
    return false;
}

void CrateCutEditor::filesDropped (const StringArray& files, int, int)
{
    dragHover = false;
    for (auto& f : files)
        if (File (f).hasFileExtension (audioWildcard))
        {
            loadFile (File (f));
            break;
        }
    content.repaint();
}
