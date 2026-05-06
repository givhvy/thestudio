#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginHost.h"
#include "AudioEngine.h"
#include "TransportBar.h"
#include "ChannelRack.h"
#include "Mixer.h"
#include "Browser.h"
#include "Playlist.h"
#include "BottomDock.h"
#include "PianoRoll.h"

class MainComponent : public juce::Component, public juce::DragAndDropContainer
{
public:
    MainComponent(PluginHost& pluginHost, AudioEngine& audioEngine);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    PluginHost& pluginHost_;
    AudioEngine& audioEngine_;
    
    std::unique_ptr<TransportBar> transportBar_;
    std::unique_ptr<ChannelRack> channelRack_;
    std::unique_ptr<Mixer> mixer_;
    std::unique_ptr<Browser> browser_;
    std::unique_ptr<Playlist> playlist_;
    std::unique_ptr<BottomDock> bottomDock_;
    std::unique_ptr<PianoRoll> pianoRoll_;
    
    enum class CenterView { Playlist, Mixer, PianoRoll };
    CenterView centerView_ = CenterView::Playlist;
    void setCenterView(CenterView v);
    
    // Custom title bar
    juce::Label titleLabel_;
    juce::TextButton minimizeBtn_;
    juce::TextButton maximizeBtn_;
    juce::TextButton closeBtn_;
    
    juce::ComponentDragger windowDragger_;
    juce::Point<int> dragStartPos_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
