#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WebBrowserHost.h"

class AppWindow : public juce::DocumentWindow
{
public:
    AppWindow (PluginHost& pluginHost, AudioEngine& audioEngine);

    void closeButtonPressed() override;
    void minimiseButtonPressed() override;
    void maximiseButtonPressed() override;

    WebBrowserHost* getWebHost() { return webHost.get(); }

private:
    std::unique_ptr<WebBrowserHost> webHost;
};
