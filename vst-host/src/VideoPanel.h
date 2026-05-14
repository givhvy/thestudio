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
    void mouseDown(const juce::MouseEvent& e) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // Open a video file (returns true on success).
    bool loadVideoFile(const juce::File& f);
    juce::File getCurrentFile() const { return currentFile_; }

    // Called when the user clicks the close X.
    std::function<void()> onClose;

private:
    void showOpenDialog();
    void showEmptyPage();

    std::unique_ptr<juce::WebBrowserComponent> web_;
    juce::File           currentFile_;
    juce::File           tempHtmlFile_;        // generated HTML wrapper

    juce::TextButton openBtn_   { "Open Video..." };
    juce::TextButton closeBtn_  { "X" };
    juce::Label      titleLabel_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoPanel)
};
