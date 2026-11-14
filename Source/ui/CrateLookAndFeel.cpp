#include "CrateLookAndFeel.h"

using namespace juce;

CrateLookAndFeel::CrateLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, Palette::background);
    setColour (Label::textColourId, Palette::text);
    setColour (Slider::textBoxTextColourId, Palette::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (ComboBox::backgroundColourId, Palette::well);
    setColour (ComboBox::textColourId, Palette::text);
    setColour (ComboBox::outlineColourId, Palette::plateHi);
    setColour (ComboBox::arrowColourId, Palette::accent);
    setColour (PopupMenu::backgroundColourId, Palette::panel);
    setColour (PopupMenu::textColourId, Palette::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::accent.withAlpha (0.85f));
    setColour (PopupMenu::highlightedTextColourId, Palette::background);
    setColour (TextButton::buttonColourId, Palette::plateHi);
    setColour (TextButton::buttonOnColourId, Palette::accent);
    setColour (TextButton::textColourOffId, Palette::text);
    setColour (TextButton::textColourOnId, Palette::background);
    setColour (TooltipWindow::backgroundColourId, Palette::panel);
    setColour (TooltipWindow::textColourId, Palette::text);
    setColour (TooltipWindow::outlineColourId, Palette::accent.withAlpha (0.5f));
}

Font CrateLookAndFeel::uiFont (float height, bool bold)
{
    return Font (FontOptions (height, bold ? Font::bold : Font::plain));
}

void CrateLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = s.getProperties()["bipolar"];

    // value arc
    const float arcR = radius - 1.5f;
    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::well);
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
    g.setColour (Palette::accent.withAlpha (s.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (value, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // knob body
    const float bodyR = radius - 7.0f;
    const auto body = Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0.0f, 2.0f));
    g.setGradientFill (ColourGradient (Colour (0xff4a4d54), body.getX(), body.getY(),
                                       Colour (0xff1f2125), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Colour (0xff5a5e66));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    // knurled ring
    g.setColour (Colours::black.withAlpha (0.25f));
    for (int i = 0; i < 24; ++i)
    {
        const float a = MathConstants<float>::twoPi * i / 24.0f;
        g.drawLine (centre.x + std::sin (a) * bodyR * 0.82f, centre.y - std::cos (a) * bodyR * 0.82f,
                    centre.x + std::sin (a) * bodyR * 0.98f, centre.y - std::cos (a) * bodyR * 0.98f, 1.0f);
    }

    // pointer
    Path pointer;
    pointer.addRoundedRectangle (-1.6f, -bodyR + 3.0f, 3.2f, bodyR * 0.55f, 1.6f);
    g.setColour (Palette::text);
    g.fillPath (pointer, AffineTransform::rotation (angle).translated (centre));
}

void CrateLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos,
                                         float, float, Slider::SliderStyle style, Slider& s)
{
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const bool vertical = style == Slider::LinearVertical;

    if (vertical)
    {
        // Pitch fader: long slot with a centre detent and tick marks.
        const float cx = area.getCentreX();
        g.setColour (Palette::dim.withAlpha (0.6f));
        for (int i = 0; i <= 16; ++i)
        {
            const float ty = area.getY() + area.getHeight() * i / 16.0f;
            const float len = (i % 4 == 0) ? 9.0f : 5.0f;
            g.fillRect (cx - 16.0f - len, ty - 0.5f, len, 1.0f);
            g.fillRect (cx + 16.0f, ty - 0.5f, len, 1.0f);
        }
        g.setColour (Palette::accent.withAlpha (0.8f));
        g.fillRect (cx + 16.0f, area.getCentreY() - 1.0f, 12.0f, 2.0f);

        g.setColour (Palette::well);
        g.fillRoundedRectangle (cx - 3.0f, area.getY(), 6.0f, area.getHeight(), 3.0f);

        const auto cap = Rectangle<float> (30.0f, 18.0f).withCentre ({ cx, sliderPos });
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);
        g.setGradientFill (ColourGradient (Colour (0xff5d6068), cap.getX(), cap.getY(),
                                           Colour (0xff2a2c31), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (Palette::text);
        g.fillRect (cap.getX() + 4.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 8.0f, 2.0f);
        return;
    }

    // Crossfader: short throw slot and a wide cap.
    const float cy = area.getCentreY();
    g.setColour (Palette::well);
    g.fillRoundedRectangle (area.getX(), cy - 3.5f, area.getWidth(), 7.0f, 3.5f);
    g.setColour (Palette::dim.withAlpha (0.5f));
    for (int i = 0; i <= 10; ++i)
    {
        const float tx = area.getX() + area.getWidth() * i / 10.0f;
        g.fillRect (tx - 0.5f, cy + 12.0f, 1.0f, (i % 5 == 0) ? 6.0f : 3.0f);
    }

    const auto cap = Rectangle<float> (20.0f, 30.0f).withCentre ({ sliderPos, cy });
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);
    g.setGradientFill (ColourGradient (Colour (0xff60636b), cap.getX(), cap.getY(),
                                       Colour (0xff2a2c31), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (s.isMouseButtonDown() ? Palette::accent : Palette::text);
    g.fillRect (cap.getCentreX() - 1.0f, cap.getY() + 4.0f, 2.0f, cap.getHeight() - 8.0f);
}

void CrateLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    auto base = on ? Palette::accent : Palette::plateHi;
    if (highlighted) base = base.brighter (0.08f);
    if (down) base = base.darker (0.15f);

    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 4.0f);
    g.setGradientFill (ColourGradient (base.brighter (0.1f), r.getX(), r.getY(), base.darker (0.2f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::white.withAlpha (on ? 0.25f : 0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

void CrateLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? Palette::background : Palette::text);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), Justification::centred, 1);
}

void CrateLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    g.setColour (highlighted ? Palette::plateHi.brighter (0.05f) : Palette::plateHi);
    g.fillRoundedRectangle (r, 4.0f);

    const auto led = Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 10.0f, r.getCentreY() });
    if (on)
    {
        g.setColour (Palette::accent.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (on ? Palette::accent : Palette::well);
    g.fillEllipse (led);

    g.setColour (on ? Palette::text : Palette::dim);
    g.setFont (uiFont (12.0f, true));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (19.0f).toNearestInt(), Justification::centredLeft, 1);
}

void CrateLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (Palette::well);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (false) ? Palette::accent : Palette::plateHi);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Palette::accent);
    g.fillPath (arrow);
}

Font CrateLookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (12.5f); }

void CrateLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

Font CrateLookAndFeel::getLabelFont (Label& l)
{
    return uiFont (l.getFont().getHeight() > 0 ? l.getFont().getHeight() : 12.0f, l.getFont().isBold());
}

Font CrateLookAndFeel::getTextButtonFont (TextButton&, int buttonHeight)
{
    return uiFont (jmin (14.0f, buttonHeight * 0.5f), true);
}
