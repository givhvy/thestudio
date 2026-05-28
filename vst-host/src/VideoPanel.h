#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

// Embedded video player. Floating overlay panel (toggled via the
// VIDEO button in BottomDock). Lets the user pick or drop a video
// file and watch it while making music — visual reference like
// FL Studio's video player.
//
// Uses a WebView2 HTML5 <video> element under the hood (instead of
// JUCE's MediaFoundation VideoComponent) so it handles far more
// codecs (H.264, H.265/HEVC, VP9, AV1, etc.) and gives us reliable
// native transport controls.
class VideoPanel : public juce::Component,
                   public juce::FileDragAndDropTarget
{
public:
    VideoPanel();
    ~VideoPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void moved() override;
    void mouseDown(const juce::MouseEvent& e) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    static bool canAcceptVideoFiles(const juce::StringArray& files);

    // Open a video file (returns true on success).
    bool loadVideoFile(const juce::File& f);
    void handleFileDrop(const juce::StringArray& files);
    juce::File getCurrentFile() const { return currentFile_; }
    bool hasVideoLoaded() const { return currentFile_.existsAsFile(); }
    bool isEmbeddedInSession() const { return embeddedInSession_; }

    void embedPlayerInSession(juce::Component* host);
    void unembedPlayerFromSession();
    void syncWebPlayerBounds();
    void scheduleWebLayoutSync();

    void saveWindowState() const;
    juce::Rectangle<int> getSavedOrDefaultBounds(juce::Rectangle<int> parentBounds,
                                                 juce::Rectangle<int> defaultBounds) const;

    // Called when the user clicks the close X.
    std::function<void()> onClose;
    std::function<void()> onOpenInSessionTab;

private:
    void paintEmbeddedPlaceholder(juce::Graphics& g, juce::Rectangle<int> area);
    static bool isVideoFile(const juce::File& f);
    void paintEmptyState(juce::Graphics& g, juce::Rectangle<int> area);
    void setWebVisible(bool visible);
    void reloadWebPlayer();
    static juce::String buildPlayerHtml(const juce::File& videoFile);
    void showOpenDialog();
    void showEmptyPage();
    void setFileDragHighlight(bool on);
    static juce::File stateFile();
    juce::Rectangle<int> constrainToParent(juce::Rectangle<int> bounds,
                                           juce::Rectangle<int> parentBounds) const;

    void mouseDrag(const juce::MouseEvent& e) override;

    std::unique_ptr<juce::WebBrowserComponent> web_;
    std::unique_ptr<juce::Component>            dropOverlay_;
    juce::File           currentFile_;
    bool                 fileDragOver_ = false;
    bool                 webVisible_ = false;
    bool                 embeddedInSession_ = false;
    juce::Component*     embeddedHost_ = nullptr;
    juce::File           tempHtmlFile_;        // generated HTML wrapper

    juce::TextButton sessionBtn_ { "Open in session tab" };
    juce::TextButton openBtn_   { "Open Video..." };
    juce::TextButton closeBtn_  { "X" };
    juce::Label      titleLabel_;

    juce::ComponentDragger              dragger_;
    juce::ComponentBoundsConstrainer    constrainer_;
    juce::ResizableCornerComponent      resizer_ { this, &constrainer_ };
    bool                                draggingPanel_ = false;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoPanel)
};
