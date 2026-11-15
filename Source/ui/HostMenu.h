#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
    Right-click on a control shows the host's own menu for its parameter, e.g.
    FL Studio's "Link to controller" / "Create automation clip". Returns false
    if the host doesn't provide one.
*/
inline bool showHostParameterMenu (juce::Component& source, juce::AudioProcessorParameter* param)
{
    if (param == nullptr) return false;
    auto* editor = source.findParentComponentOfClass<juce::AudioProcessorEditor>();
    if (editor == nullptr) return false;

    if (auto* host = editor->getHostContext())
        if (auto menu = host->getContextMenuForParameter (param))
        {
            menu->showNativeMenu (editor->getMouseXYRelative());
            return true;
        }
    return false;
}

/** Attach to any control to give it the host's right-click parameter menu. */
class HostMenuOnRightClick : public juce::MouseListener
{
public:
    HostMenuOnRightClick (juce::Component& c, juce::AudioProcessorParameter* p) : component (c), param (p)
    {
        component.addMouseListener (this, false);
    }
    ~HostMenuOnRightClick() override { component.removeMouseListener (this); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            showHostParameterMenu (component, param);
    }

private:
    juce::Component& component;
    juce::AudioProcessorParameter* param;
};
