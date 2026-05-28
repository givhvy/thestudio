#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "NativeBridge.h"

class WebBrowserHost : public juce::WebBrowserComponent
{
public:
    WebBrowserHost (PluginHost& ph, AudioEngine& ae);

    void loadFrontend (const juce::File& indexHtml);
    void loadURL (const juce::String& url);
    NativeBridge& getBridge() { return bridge; }

    void pageFinishedLoading (const juce::String& url) override;

private:
    static juce::WebBrowserComponent::Options buildOptions (NativeBridge& bridge);
    NativeBridge bridge;
};
