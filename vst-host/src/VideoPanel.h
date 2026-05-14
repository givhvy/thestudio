#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_video/juce_video.h>

// Embedded video player. Floating overlay panel (toggled via the
// VIDEO button in BottomDock). Lets the user pick or drop a video
// file and watch it while making music — useful as visual reference
// (think FL Studio's video player).
//
// Hosts a native `juce::VideoComponent` (MediaFoundation on Windows,
// AVFoundation on macOS) with a custom transport strip on the bottom:
// Play/Pause, scrub slider, time readout, volume.
class VideoPanel : public juce::Component,
                   public juce::FileDragAndDropTarget,
                   private juce::Timer
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
    void timerCallback() override;     // updates scrub slider + time label
    void showOpenDialog();
    void togglePlay();
    void updateTransportUi();
    static juce::String formatTime(double seconds);

    juce::VideoComponent video_{ false };   // false = no native controls
    juce::File           currentFile_;

    // Transport
    juce::TextButton playBtn_   { "Play" };
    juce::TextButton openBtn_   { "Open..." };
    juce::TextButton closeBtn_  { "X" };
    juce::Slider     scrub_     { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider     volume_    { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label      timeLabel_;
    juce::Label      titleLabel_;

    bool             scrubbing_ = false;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoPanel)
};
