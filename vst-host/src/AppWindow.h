#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "MainComponent.h"

class AppWindow : public juce::DocumentWindow
{
public:
    AppWindow (PluginHost& pluginHost, AudioEngine& audioEngine);

    void closeButtonPressed() override;
    void minimiseButtonPressed() override;
    void maximiseButtonPressed() override;

    MainComponent* getMainComponent() { return mainComponent.get(); }

private:
    std::unique_ptr<MainComponent> mainComponent;
};
