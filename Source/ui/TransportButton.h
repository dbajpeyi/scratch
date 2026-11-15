#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CrateLookAndFeel.h"

/** Play-backwards / pause / play-forwards button with a drawn icon. */
class TransportButton : public juce::Button
{
public:
    enum class Icon { backwards, pause, forwards };

    TransportButton (Icon i, const juce::String& name) : juce::Button (name), icon (i) {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, {}, highlighted, down);

        const auto r = getLocalBounds().toFloat().reduced (getWidth() * 0.3f, getHeight() * 0.3f);
        g.setColour (getToggleState() ? Palette::background : Palette::text);

        juce::Path p;
        switch (icon)
        {
            case Icon::forwards:  p.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY()); break;
            case Icon::backwards: p.addTriangle (r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getCentreY()); break;
            case Icon::pause:
                p.addRectangle (r.getX() + r.getWidth() * 0.1f, r.getY(), r.getWidth() * 0.28f, r.getHeight());
                p.addRectangle (r.getRight() - r.getWidth() * 0.38f, r.getY(), r.getWidth() * 0.28f, r.getHeight());
                break;
        }
        g.fillPath (p);
    }

private:
    Icon icon;
};
