#include "WaveformView.h"
#include "CrateLookAndFeel.h"
#include "../PluginProcessor.h"

using namespace juce;

WaveformView::WaveformView (CrateCutProcessor& p) : processor (p)
{
    setMouseCursor (MouseCursor::IBeamCursor);
    refresh();
}

void WaveformView::refresh()
{
    auto rec = processor.getRecord();
    const auto params = processor.readParams();
    if (rec != record || params.chopMode != cachedChopMode || params.slices != cachedSlices)
    {
        record = rec;
        cachedChopMode = params.chopMode;
        cachedSlices = params.slices;
        numChops = record != nullptr ? computeChopPoints (*record, params.chopMode, params.slices, chops.data(), (int) chops.size()) : 0;
        waveCache = {};
    }
}

void WaveformView::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::well);
    g.fillRoundedRectangle (bounds, 5.0f);

    if (record == nullptr || record->getLength() == 0)
        return;

    const auto area = bounds.reduced (6.0f, 14.0f).withTrimmedBottom (-8.0f);
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();

    if (! waveCache.isValid())
    {
        const int w = jmax (1, (int) (area.getWidth() * scale)), h = jmax (1, (int) (area.getHeight() * scale));
        waveCache = Image (Image::ARGB, w, h, true);
        Graphics wg (waveCache);
        const float mid = h * 0.5f;
        const auto& peaks = record->overview;
        float peakMax = 0.001f;
        for (auto& p : peaks) peakMax = jmax (peakMax, -p.min, p.max);

        wg.setColour (Palette::accent.withAlpha (0.75f));
        for (int x = 0; x < w; ++x)
        {
            const size_t a = (size_t) ((int64) x * (int64) peaks.size() / w);
            const size_t b = jmax (a + 1, (size_t) ((int64) (x + 1) * (int64) peaks.size() / w));
            float lo = 0.0f, hi = 0.0f;
            for (size_t i = a; i < b && i < peaks.size(); ++i)
            {
                lo = jmin (lo, peaks[i].min);
                hi = jmax (hi, peaks[i].max);
            }
            wg.fillRect ((float) x, mid - hi / peakMax * mid * 0.95f, 1.0f, jmax (1.0f, (hi - lo) / peakMax * mid * 0.95f));
        }
    }
    g.drawImage (waveCache, area);

    const double duration = record->getDurationSeconds();
    auto xFor = [&] (double t) { return area.getX() + (float) (t / duration) * area.getWidth(); };

    // chop markers with the note that triggers them
    const auto params = processor.readParams();
    g.setFont (CrateLookAndFeel::uiFont (10.0f, true));
    for (int i = 0; i < numChops; ++i)
    {
        const float x = xFor (chops[(size_t) i]);
        g.setColour (Palette::text.withAlpha (0.35f));
        g.fillRect (x, area.getY(), 1.0f, area.getHeight());
        if (params.baseNote < 128)
        {
            const auto name = MidiMessage::getMidiNoteName (params.baseNote + i, true, true, 3);
            g.setColour (Palette::text.withAlpha (0.7f));
            g.drawText (name, Rectangle<float> (40.0f, 12.0f).withPosition (x + 2.0f, bounds.getY() + 1.0f), Justification::centredLeft);
        }
    }

    // playhead
    const float px = xFor (processor.engine.uiPosition.load());
    const bool touching = processor.engine.uiTouching.load();
    g.setColour (touching ? Palette::accent : Palette::text);
    g.fillRect (px - 1.0f, bounds.getY() + 2.0f, 2.0f, bounds.getHeight() - 4.0f);

    g.setColour (Palette::dim);
    g.setFont (CrateLookAndFeel::uiFont (10.0f));
    g.drawText (String (processor.engine.uiPosition.load(), 2) + " / " + String (duration, 2) + " s",
                bounds.reduced (6.0f, 1.0f), Justification::topRight);
}

void WaveformView::seekTo (float x)
{
    if (record == nullptr) return;
    const auto area = getLocalBounds().toFloat().reduced (6.0f, 14.0f);
    const double t = jlimit (0.0, 1.0, (double) ((x - area.getX()) / area.getWidth())) * record->getDurationSeconds();
    processor.requestSeek (t);
}

void WaveformView::mouseDown (const MouseEvent& e) { seekTo (e.position.x); }
void WaveformView::mouseDrag (const MouseEvent& e) { seekTo (e.position.x); }
