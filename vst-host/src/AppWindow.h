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
    void moved()    override { if (canPersist_) saveWindowState(); }
    void resized()  override { juce::DocumentWindow::resized(); if (canPersist_) saveWindowState(); }

    MainComponent* getMainComponent() { return mainComponent.get(); }

private:
    std::unique_ptr<MainComponent> mainComponent;

    static juce::File windowStateFile();
    void saveWindowState();
    void restoreWindowState();

    bool canPersist_ = false;
};
