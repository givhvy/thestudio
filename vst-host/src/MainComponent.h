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
#include "PatternsPanel.h"
#include "VideoPanel.h"
#include "ConsistencyPanel.h"
#include "OrgChartPanel.h"
#include "ChordAnalysisEngine.h"
#include "ChordifyAutomationEngine.h"
#include "ChordifyMidiImporter.h"
#include <atomic>
#include "PluginWindow.h"
#include "NativeEffectEditor.h"
#include "Theme.h"
#include <unordered_map>

class ProjectOpenOverlay;
class ProjectSaveOverlay;
class CloudUploadOverlay;
class Midi808SettingsOverlay;
class ChangelogOverlay;

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      public juce::FileDragAndDropTarget,
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

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void deliverVideoFileDrop(const juce::StringArray& files, juce::Point<int> localPos);
    void notifyVideoFileDragEnter(const juce::StringArray& files);
    void notifyVideoFileDragExit();
    void toggleMaximize();
    bool isWindowMaximized() const { return isMaximized_; }
    // Debug/perf harness: build a busy beat (full drum preset + pattern clips +
    // open mixer & piano roll) and start playback. Drivable via JSON-RPC so
    // performance can be measured programmatically. mode: 0=stop, 1=build+play.
    void runPerfStress(int mode);
    // True borderless full-screen (covers the whole monitor incl. taskbar).
    // Alt+Enter, FL-Studio style.
    void toggleFullScreen();

    // Project file I/O (.stratum)
    static constexpr const char* kProjectExt = ".stratum";
    void saveProjectAs();        // shows file chooser
    void openProjectFile();      // shows file chooser
    void exportAudioAs();        // shows file chooser
    void newProject();
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
    std::unique_ptr<PatternsPanel> patternsPanel_;
    std::unique_ptr<VideoPanel> videoPanel_;
    std::unique_ptr<ConsistencyPanel> consistencyPanel_;
    std::unique_ptr<OrgChartPanel> orgChartPanel_;
    ChordAnalysisEngine chordAnalysisEngine_;
    ChordifyAutomationEngine chordifyAutomationEngine_;
    bool bassAnalysisBusy_ = false;
    
    enum class CenterView { Playlist, Mixer, PianoRoll, Consistency, OrgChart };
    CenterView centerView_ = CenterView::Playlist;
    void setCenterView(CenterView v);

    // Index (into channelRack channels) of whichever channel is currently
    // loaded in the piano roll. -1 = nothing loaded yet.
    int pianoRollChannelIndex_ = -1;

    enum class AiPanelMode { Floating, SidePanel };
    AiPanelMode aiPanelMode_ = AiPanelMode::Floating;
    void showAiOpenModeMenu();
    void openAiPanel(AiPanelMode mode);
    void closeAiPanel();
    juce::Rectangle<int> getAiFloatingBounds() const;
    juce::Rectangle<int> getAiSidePanelBounds() const;

    TransportBar::PlaybackMode playbackMode_ = TransportBar::PlaybackMode::Rack;
    bool pianoRealFeel_ = false;
    void applyPlaybackMode(TransportBar::PlaybackMode mode);
    bool exportAudioToFile(const juce::File& wavFile, int soloChannel = -1, bool includeRack = true, bool includePlaylistLoops = true);
    bool exportStemsToFolder(const juce::File& folder, const juce::String& beatName);
    void showThemeMenu();
    void showPinterestMenu();
    void showExportAudioModal(bool defaultStems = false);
    void showCloudUploadModal();
    void showMidi808SettingsModal();
    void showChangelogModal();
    void backupCurrentProject();
    void checkForUpdates(bool manual);
    void openUpdateDownload();
    // Export the current beat to a WAV and ask Beats Studio (via the 9003 TCP
    // bridge) to render a video for it on its Create/AutoVid tab.
    void renderVideoInBeatsStudio();
    void openRenderedVideoWindow(const juce::File& videoFile);
    void saveProjectAndRenderWavCopy(const juce::String& cleanName, const juce::File& renderWavFile);
    bool uploadBeatToCloud(const juce::String& name, const juce::File& wavFile, const juce::File& projectFile);
    void openVideoInSessionTab();
    void handleBassExtractionRequest(Playlist::BassExtractionRequest request,
                                     std::function<void(std::vector<Playlist::ExtractedBassNote>)> deliverNotes,
                                     bool autoApply);
    void handleChordifyMidiImport(Playlist::BassExtractionRequest request);
    void handleChordifyRestart();
    void runOrgChartAgent(const juce::String& agentId);
    void runAllEnabledOrgChartAgents();
    void runPinterestDownloadAgent();
    void applyThemePreset(Theme::Preset preset, bool persist);
    void refreshThemeButton();
    void showLoopsInBpmRangePicker(double bpm, juce::Rectangle<int> anchorScreenArea);
    void openLoopPickerCallout(std::vector<juce::File> loops, double bpm, juce::Rectangle<int> anchorScreenArea);
    void syncPlaylistLoopsToChannelRack();
    static juce::File themeStateFile();
    
    // Custom title bar
    juce::Label titleLabel_;
    juce::TextButton minimizeBtn_;
    juce::TextButton maximizeBtn_;
    juce::TextButton closeBtn_;
    juce::TextButton themeBtn_ { "THEME" };
    juce::TextButton pinterestBtn_ { "PINTEREST" };
    juce::TextButton midi808Btn_ { "808 MIDI" };
    juce::TextButton consistencyTitleBtn_ { "CONSISTENCY" };
    juce::TextButton distrokidBtn_ { "DISTROKID" };
    juce::TextButton aeroBtn_ { "AERO" };
    juce::TextButton changelogBtn_ { "CHANGELOG" };
    juce::TextButton backupBtn_ { "BACKUP" };
    juce::TextButton updateBtn_ { "UPDATE" };
    std::atomic<bool> updateCheckInFlight_ { false };
    bool updateAvailable_ = false;
    juce::String latestUpdateVersion_;
    juce::String latestUpdateUrl_;
    std::unique_ptr<std::thread> pinterestThread_;
    juce::ComponentDragger windowDragger_;
    bool isDraggingWindow_ = false;
    juce::Point<int> dragStartPos_;

    bool isMaximized_ = false;
    juce::Rectangle<int> preMaxBounds_;
    bool isFullScreen_ = false;
    juce::Rectangle<int> preFullScreenBounds_;

    juce::File currentProjectFile_;
    std::unique_ptr<juce::FileChooser> fileChooser_;
    std::unique_ptr<juce::FileChooser> instrumentChooser_;
    std::unique_ptr<ProjectOpenOverlay> projectOpenOverlay_;
    std::unique_ptr<ProjectSaveOverlay> projectSaveOverlay_;
    std::unique_ptr<CloudUploadOverlay> cloudUploadOverlay_;
    std::unique_ptr<Midi808SettingsOverlay> midi808SettingsOverlay_;
    std::unique_ptr<ChangelogOverlay> changelogOverlay_;

    // Embedded plugin editor windows (slotId → window). Floating children of
    // MainComponent so the editor lives inside the app, not as an OS window.
    std::unordered_map<int, std::unique_ptr<PluginWindow>> pluginWindows_;
    std::unordered_map<int, std::unique_ptr<NativeEffectWindow>> nativeEffectWindows_;

    // Undo/redo state — store serialized JSON to avoid var lifetime issues.
    std::vector<juce::String> undoStack_;
    std::vector<juce::String> redoStack_;
    juce::String              lastSnapshotJson_;
    bool                      restoringSnapshot_ = false;
    static constexpr size_t   kMaxUndo = 100;
    juce::String              lastExternalBrowserAudioPath_;
    double                    lastExternalBrowserAudioMs_ = 0.0;

    bool shouldDropFilesWhenDraggedExternally(const juce::DragAndDropTarget::SourceDetails& sourceDetails,
                                              juce::StringArray& files,
                                              bool& canMoveFiles) override;

    juce::String captureSnapshotJson() const;
    void         applySnapshotJson(const juce::String& json);
    void         timerCallback() override; // polls for state changes → undo push

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
