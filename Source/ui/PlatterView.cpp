#include "PlatterView.h"
#include "CrateLookAndFeel.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace
{
    constexpr float twoPi = MathConstants<float>::twoPi;
}

PlatterView::PlatterView (CrateCutProcessor& p) : processor (p)
{
    touchParam = processor.apvts.getParameter (ParamID::touch);
    scratchParam = processor.apvts.getParameter (ParamID::scratch);
    rangeParam = processor.apvts.getParameter (ParamID::range);
    setOpaque (false);
    setRepaintsOnMouseActivity (false);
}

PlatterView::~PlatterView()
{
    if (dragging)
    {
        touchParam->setValueNotifyingHost (0.0f);
        touchParam->endChangeGesture();
        scratchParam->endChangeGesture();
    }
}

void PlatterView::resized()
{
    const auto b = getLocalBounds().toFloat();
    platterRadius = jmin (b.getHeight() * 0.5f - 6.0f, b.getWidth() * 0.42f);
    centre = { 6.0f + platterRadius, b.getCentreY() };
    vinylRadius = platterRadius * 0.92f;
    labelRadius = vinylRadius * 0.34f;

    armPivot = { jmin (b.getRight() - 34.0f, centre.x + platterRadius + 52.0f), centre.y - platterRadius * 0.62f };
    const float rMid = vinylRadius * 0.68f;
    const auto mid = centre + Point<float> (std::cos (0.30f), std::sin (0.30f)) * rMid;
    armLength = mid.getDistanceFrom (armPivot);

    cacheScale = 0.0f; // force re-render of the vinyl
}

bool PlatterView::hitsRecord (Point<float> p) const
{
    return p.getDistanceFrom (centre) <= platterRadius;
}

float PlatterView::angleAt (Point<float> p) const
{
    return std::atan2 (p.y - centre.y, p.x - centre.x);
}

//==============================================================================
void PlatterView::renderVinylCache()
{
    const int size = jmax (8, (int) std::ceil (vinylRadius * 2.0f * cacheScale));
    vinylCache = Image (Image::ARGB, size, size, true);
    Graphics g (vinylCache);
    const float r = size * 0.5f;
    const Point<float> c (r, r);
    const float lr = labelRadius / vinylRadius * r;

    g.setColour (Palette::vinyl);
    g.fillEllipse (0.0f, 0.0f, (float) size, (float) size);

    // grooves
    Random rng (42);
    const float step = jmax (0.9f, 1.15f * cacheScale);
    for (float rr = lr * 1.06f; rr < r * 0.965f; rr += step)
    {
        float alpha = 0.025f + 0.05f * rng.nextFloat();
        if (std::fmod (rr / r * 9.0f, 1.0f) < 0.035f) alpha = 0.005f; // gaps between tracks
        g.setColour (Colours::white.withAlpha (alpha));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 0.6f * cacheScale);
    }

    // run-out and lead-in bands
    g.setColour (Colour (0xff1a1a1d));
    g.drawEllipse (c.x - lr * 1.06f, c.y - lr * 1.06f, lr * 2.12f, lr * 2.12f, 3.0f * cacheScale);
    g.setColour (Colour (0xff202024));
    g.drawEllipse (c.x - r * 0.975f, c.y - r * 0.975f, r * 1.95f, r * 1.95f, 2.0f * cacheScale);

    // light sheen: two opposing reflections, fixed to the light, not the record
    for (int i = 0; i < 360; ++i)
    {
        const float a0 = twoPi * i / 360.0f, a1 = twoPi * (i + 1) / 360.0f;
        const float k = std::pow (jmax (0.0f, std::cos (2.0f * (a0 + 0.75f))), 14.0f);
        if (k < 0.01f) continue;
        Path seg;
        seg.addPieSegment (0.0f, 0.0f, (float) size, (float) size, a0, a1 + 0.004f, lr / r * 1.06f);
        g.setColour (Colours::white.withAlpha (0.13f * k));
        g.fillPath (seg);
    }

    // edge bevel
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawEllipse (1.0f, 1.0f, size - 2.0f, size - 2.0f, 1.5f * cacheScale);
}

void PlatterView::drawPlatter (Graphics& g, float platterAngle)
{
    // shadow + rim
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (platterRadius * 2.0f + 10.0f, platterRadius * 2.0f + 10.0f).withCentre (centre.translated (0.0f, 5.0f)));

    const auto rim = Rectangle<float> (platterRadius * 2.0f, platterRadius * 2.0f).withCentre (centre);
    g.setGradientFill (ColourGradient (Colour (0xffd6d9de), rim.getX(), rim.getY(),
                                       Colour (0xff6c7077), rim.getRight(), rim.getBottom(), false));
    g.fillEllipse (rim);

    // strobe dots: three rows around the rim, turning with the platter
    const float a = platterAngle * twoPi;
    const int counts[] = { 90, 92, 96 };
    for (int row = 0; row < 3; ++row)
    {
        const float rr = platterRadius - 2.5f - row * 3.2f;
        g.setColour (Colour (0xff3a3d42).withAlpha (0.85f));
        for (int i = 0; i < counts[row]; ++i)
        {
            const float t = a + twoPi * i / counts[row];
            g.fillRect (centre.x + std::cos (t) * rr - 0.9f, centre.y + std::sin (t) * rr - 0.9f, 1.8f, 1.8f);
        }
    }

    // slipmat edge (felt) peeking out under the record
    const float matR = vinylRadius + (platterRadius - vinylRadius) * 0.35f;
    g.setColour (Colour (0xff1c1c1f));
    g.fillEllipse (Rectangle<float> (matR * 2.0f, matR * 2.0f).withCentre (centre));
}

void PlatterView::drawLabel (Graphics& g, float recordAngle)
{
    const float a = recordAngle * twoPi;
    const auto t = AffineTransform::rotation (a, centre.x, centre.y);

    const auto label = Rectangle<float> (labelRadius * 2.0f, labelRadius * 2.0f).withCentre (centre);
    g.setGradientFill (ColourGradient (Palette::red.brighter (0.2f), label.getX(), label.getY(),
                                       Palette::red.darker (0.45f), label.getRight(), label.getBottom(), false));
    g.fillEllipse (label);
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawEllipse (label.reduced (labelRadius * 0.12f), 1.0f);

    // label artwork (rotates)
    {
        Graphics::ScopedSaveState save (g);
        g.addTransform (t);

        g.setColour (Palette::accent);
        g.setFont (CrateLookAndFeel::uiFont (labelRadius * 0.27f, true));
        g.drawText ("CRATECUT", Rectangle<float> (labelRadius * 1.6f, labelRadius * 0.34f)
                                    .withCentre (centre.translated (0.0f, -labelRadius * 0.48f)), Justification::centred);

        g.setColour (Palette::text.withAlpha (0.85f));
        g.setFont (CrateLookAndFeel::uiFont (labelRadius * 0.15f));
        g.drawText (String::fromUTF8 ("SIDE A  \xc2\xb7  33 1/3"), Rectangle<float> (labelRadius * 1.6f, labelRadius * 0.24f)
                                    .withCentre (centre.translated (0.0f, labelRadius * 0.45f)), Justification::centred);

        g.setColour (Palette::accent.withAlpha (0.8f));
        g.fillRect (Rectangle<float> (labelRadius * 1.1f, 1.2f).withCentre (centre.translated (0.0f, -labelRadius * 0.24f)));

        // cue sticker across the label edge onto the vinyl, like on a DJ's record
        const auto sticker = Rectangle<float> (labelRadius * 0.11f, labelRadius * 0.62f)
                                 .withCentre (centre.translated (0.0f, -(labelRadius * 1.05f)));
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (sticker.translated (1.0f, 1.0f), 1.5f);
        g.setColour (Colour (0xfff3eee4));
        g.fillRoundedRectangle (sticker, 1.5f);
    }

    // spindle
    const auto spindle = Rectangle<float> (labelRadius * 0.14f, labelRadius * 0.14f).withCentre (centre);
    g.setGradientFill (ColourGradient (Colours::white, spindle.getX(), spindle.getY(),
                                       Colour (0xff70747a), spindle.getRight(), spindle.getBottom(), false));
    g.fillEllipse (spindle);
}

void PlatterView::drawToneArm (Graphics& g, float progress)
{
    // Find where the needle sits for this position on the record: intersection of
    // the groove circle and the circle swept by the arm.
    const float rNeedle = vinylRadius * (0.93f - 0.5f * jlimit (0.0f, 1.0f, progress));
    const auto d = armPivot - centre;
    const float dist = d.getDistanceFromOrigin();
    const float aa = (rNeedle * rNeedle - armLength * armLength + dist * dist) / (2.0f * dist);
    const float h = std::sqrt (jmax (0.0f, rNeedle * rNeedle - aa * aa));
    const auto base = centre + d * (aa / dist);
    const Point<float> perp (-d.y / dist, d.x / dist);
    auto p1 = base + perp * h, p2 = base - perp * h;
    const auto needle = p1.y > p2.y ? p1 : p2;

    const auto dir = (needle - armPivot) / armPivot.getDistanceFrom (needle);

    // base
    const float baseR = platterRadius * 0.16f;
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (Rectangle<float> (baseR * 2.0f, baseR * 2.0f).withCentre (armPivot.translated (2.0f, 4.0f)));
    g.setGradientFill (ColourGradient (Colour (0xff5b5f66), armPivot.x - baseR, armPivot.y - baseR,
                                       Colour (0xff2b2d31), armPivot.x + baseR, armPivot.y + baseR, false));
    g.fillEllipse (Rectangle<float> (baseR * 2.0f, baseR * 2.0f).withCentre (armPivot));
    g.setColour (Colour (0xff6d7178));
    g.drawEllipse (Rectangle<float> (baseR * 2.0f, baseR * 2.0f).withCentre (armPivot), 1.0f);

    // arm rest post
    const auto rest = armPivot.translated (-baseR * 0.3f, platterRadius * 0.95f);
    g.setColour (Colour (0xff2c2e33));
    g.fillEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (rest));

    // counterweight
    const auto cw = armPivot - dir * (baseR * 1.05f);
    Path weight;
    weight.addRoundedRectangle (-baseR * 0.55f, -baseR * 0.42f, baseR * 1.1f, baseR * 0.84f, 3.0f);
    const auto rot = AffineTransform::rotation (std::atan2 (dir.y, dir.x)).translated (cw);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillPath (weight, rot.translated (2.0f, 4.0f));
    g.setGradientFill (ColourGradient (Colour (0xffe0e2e6), cw.x, cw.y - baseR * 0.4f, Colour (0xff6a6e75), cw.x, cw.y + baseR * 0.4f, false));
    g.fillPath (weight, rot);

    // tube (with the classic S-bend approximated by a gentle kink near the head)
    const auto kink = armPivot + (needle - armPivot) * 0.72f + Point<float> (dir.y, -dir.x) * (platterRadius * 0.05f);
    Path tube;
    tube.startNewSubPath (armPivot);
    tube.quadraticTo (kink, needle - dir * (platterRadius * 0.10f));
    g.setColour (Colours::black.withAlpha (0.35f));
    g.strokePath (tube, PathStrokeType (6.5f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (3.0f, 6.0f));
    g.setColour (Palette::metal);
    g.strokePath (tube, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white.withAlpha (0.6f));
    g.strokePath (tube, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));

    // headshell + cartridge, offset ~22 degrees like a real S-arm
    const float headAngle = std::atan2 (dir.y, dir.x) + 0.38f;
    Path head;
    head.addRoundedRectangle (-platterRadius * 0.11f, -platterRadius * 0.035f, platterRadius * 0.15f, platterRadius * 0.07f, 2.0f);
    const auto headT = AffineTransform::rotation (headAngle).translated (needle);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillPath (head, headT.translated (2.0f, 4.0f));
    g.setColour (Colour (0xff2a2b2f));
    g.fillPath (head, headT);
    g.setColour (Palette::accent);
    Path stripe;
    stripe.addRectangle (-platterRadius * 0.09f, -1.0f, platterRadius * 0.08f, 2.0f);
    g.fillPath (stripe, headT);

    // pivot cap
    g.setGradientFill (ColourGradient (Colours::white, armPivot.x - 6, armPivot.y - 6, Colour (0xff7a7e85), armPivot.x + 6, armPivot.y + 6, false));
    g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (armPivot));
}

void PlatterView::paint (Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (std::abs (scale - cacheScale) > 0.01f || ! vinylCache.isValid())
    {
        cacheScale = jmax (1.0f, scale);
        renderVinylCache();
    }

    auto& e = processor.engine;
    const float platterAngle = (float) std::fmod (e.uiPlatterAngle.load(), 1.0);
    const float recordAngle = (float) std::fmod (e.uiRecordAngle.load(), 1.0);
    const bool touching = e.uiTouching.load();

    float progress = 0.0f;
    if (auto rec = processor.getRecord())
        if (rec->getDurationSeconds() > 0.0)
            progress = (float) (e.uiPosition.load() / rec->getDurationSeconds());

    drawPlatter (g, platterAngle);

    const auto vinyl = Rectangle<float> (vinylRadius * 2.0f, vinylRadius * 2.0f).withCentre (centre);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (vinyl.translated (0.0f, 2.0f));
    g.drawImage (vinylCache, vinyl);

    // a few glinting grooves that turn with the record, so motion reads on the vinyl too
    {
        const float a = recordAngle * twoPi;
        const float radii[] = { 0.52f, 0.61f, 0.64f, 0.79f, 0.86f };
        const float spans[] = { 0.5f, 0.9f, 0.35f, 0.7f, 0.45f };
        for (int i = 0; i < 5; ++i)
        {
            const float rr = vinylRadius * radii[i];
            const float start = a + 0.9f + i * 1.7f;
            Path glint;
            glint.addCentredArc (centre.x, centre.y, rr, rr, 0.0f, start, start + spans[i], true);
            g.setColour (Colours::white.withAlpha (0.10f));
            g.strokePath (glint, PathStrokeType (1.0f));
        }
    }

    drawLabel (g, recordAngle);

    if (touching)
    {
        g.setColour (Palette::accent.withAlpha (0.55f));
        g.drawEllipse (vinyl.expanded (2.0f), 2.5f);
        g.setColour (Palette::accent.withAlpha (0.15f));
        g.drawEllipse (vinyl.expanded (5.0f), 4.0f);
    }

    drawToneArm (g, progress);

    // speed readout
    const float speed = e.uiSpeed.load();
    g.setColour (touching ? Palette::accent : Palette::dim);
    g.setFont (CrateLookAndFeel::uiFont (11.0f, true));
    g.drawText (touching ? "HAND" : "MOTOR", Rectangle<float> (60.0f, 14.0f).withPosition (getWidth() - 66.0f, (float) getHeight() - 36.0f), Justification::centredRight);
    g.setColour (Palette::text);
    g.setFont (CrateLookAndFeel::uiFont (13.0f, true));
    g.drawText (String (speed, 2) + "x", Rectangle<float> (70.0f, 16.0f).withPosition (getWidth() - 76.0f, (float) getHeight() - 22.0f), Justification::centredRight);
}

//==============================================================================
void PlatterView::mouseMove (const MouseEvent& e)
{
    setMouseCursor (hitsRecord (e.position) ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
}

void PlatterView::mouseDown (const MouseEvent& e)
{
    if (! hitsRecord (e.position)) return;

    dragging = true;
    lastAngle = angleAt (e.position);
    scratchValue = 0.0f;

    scratchParam->beginChangeGesture();
    touchParam->beginChangeGesture();
    scratchParam->setValueNotifyingHost (scratchParam->convertTo0to1 (0.0f));
    touchParam->setValueNotifyingHost (1.0f);
}

void PlatterView::mouseDrag (const MouseEvent& e)
{
    if (! dragging) return;

    const float angle = angleAt (e.position);
    float delta = angle - lastAngle;
    if (delta > MathConstants<float>::pi) delta -= twoPi;
    if (delta < -MathConstants<float>::pi) delta += twoPi;
    lastAngle = angle;

    // screen y points down, so a clockwise drag (forward play) is a positive angle
    const float range = jmax (0.01f, rangeParam->convertFrom0to1 (rangeParam->getValue()));
    scratchValue = jlimit (-1.0f, 1.0f, scratchValue + delta / twoPi / range);
    scratchParam->setValueNotifyingHost (scratchParam->convertTo0to1 (scratchValue));
}

void PlatterView::mouseUp (const MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    touchParam->setValueNotifyingHost (0.0f);
    touchParam->endChangeGesture();
    scratchParam->endChangeGesture();
}

void PlatterView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (! hitsRecord (e.position)) return;
    const float sign = w.isReversed ? -1.0f : 1.0f;
    processor.addJog (sign * (w.deltaY - w.deltaX) * 0.3f);
}
