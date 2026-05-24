#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "MainComponent.h"

class AppWindow : public juce::DocumentWindow,
                  public juce::FileDragAndDropTarget
{
public:
    AppWindow (PluginHost& pluginHost, AudioEngine& audioEngine);

    void closeButtonPressed() override;
    void minimiseButtonPressed() override;
    void maximiseButtonPressed() override;
    void moved()    override { if (canPersist_) saveWindowState(); }
    void resized()  override { juce::DocumentWindow::resized(); if (canPersist_) saveWindowState(); }

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    MainComponent* getMainComponent() { return mainComponent.get(); }

private:
    std::unique_ptr<MainComponent> mainComponent;

    static juce::File windowStateFile();
    void saveWindowState();
    void restoreWindowState();

    bool canPersist_ = false;
};
