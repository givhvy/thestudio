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
#include "AIPanel.h"
#include "VideoPanel.h"

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      private juce::Timer
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
    void toggleMaximize();

    // Project file I/O (.stratum)
    static constexpr const char* kProjectExt = ".stratum";
    void saveProjectAs();        // shows file chooser
    void openProjectFile();      // shows file chooser
    bool saveProject(const juce::File& f);
    bool loadProject(const juce::File& f);

    // Undo / Redo (Ctrl+Z, Ctrl+Alt+Z)
    void undo();
    void redo();

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
    std::unique_ptr<AIPanel> aiPanel_;
    std::unique_ptr<VideoPanel> videoPanel_;
    
    enum class CenterView { Playlist, Mixer, PianoRoll };
    CenterView centerView_ = CenterView::Playlist;
    void setCenterView(CenterView v);
    
    // Custom title bar
    juce::Label titleLabel_;
    juce::TextButton minimizeBtn_;
    juce::TextButton maximizeBtn_;
    juce::TextButton closeBtn_;
    juce::ComponentDragger windowDragger_;
    bool isDraggingWindow_ = false;
    juce::Point<int> dragStartPos_;

    bool isMaximized_ = false;
    juce::Rectangle<int> preMaxBounds_;

    juce::File currentProjectFile_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    // Undo/redo state — store serialized JSON to avoid var lifetime issues.
    std::vector<juce::String> undoStack_;
    std::vector<juce::String> redoStack_;
    juce::String              lastSnapshotJson_;
    bool                      restoringSnapshot_ = false;
    static constexpr size_t   kMaxUndo = 100;

    juce::String captureSnapshotJson() const;
    void         applySnapshotJson(const juce::String& json);
    void         timerCallback() override; // polls for state changes → undo push

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
