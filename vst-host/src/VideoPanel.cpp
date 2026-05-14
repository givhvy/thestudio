#include "VideoPanel.h"
#include "Theme.h"

VideoPanel::VideoPanel()
{
    setOpaque(true);
    setInterceptsMouseClicks(true, true);

    addAndMakeVisible(video_);

    // ── Title bar ────────────────────────────────────────────────
    titleLabel_.setText("VIDEO", juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
    titleLabel_.setColour(juce::Label::textColourId, Theme::orange1);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    closeBtn_.setColour(juce::TextButton::buttonColourId, Theme::zinc800);
    closeBtn_.setColour(juce::TextButton::textColourOffId, Theme::zinc300);
    closeBtn_.onClick = [this]() { if (onClose) onClose(); };
    addAndMakeVisible(closeBtn_);

    // ── Transport ────────────────────────────────────────────────
    openBtn_.setColour(juce::TextButton::buttonColourId, Theme::zinc800);
    openBtn_.setColour(juce::TextButton::textColourOffId, Theme::zinc200);
    openBtn_.onClick = [this]() { showOpenDialog(); };
    addAndMakeVisible(openBtn_);

    playBtn_.setColour(juce::TextButton::buttonColourId, Theme::orange3);
    playBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    playBtn_.onClick = [this]() { togglePlay(); };
    addAndMakeVisible(playBtn_);

    scrub_.setColour(juce::Slider::backgroundColourId, Theme::zinc800);
    scrub_.setColour(juce::Slider::trackColourId,       Theme::orange2);
    scrub_.setColour(juce::Slider::thumbColourId,       Theme::orange1);
    scrub_.setRange(0.0, 1.0, 0.0001);
    scrub_.setValue(0.0, juce::dontSendNotification);
    scrub_.onDragStart = [this]() { scrubbing_ = true; };
    scrub_.onDragEnd   = [this]()
    {
        if (video_.isVideoOpen())
            video_.setPlayPosition(scrub_.getValue() * juce::jmax(0.001, video_.getVideoDuration()));
        scrubbing_ = false;
    };
    addAndMakeVisible(scrub_);

    volume_.setColour(juce::Slider::backgroundColourId, Theme::zinc800);
    volume_.setColour(juce::Slider::trackColourId,       Theme::zinc500);
    volume_.setColour(juce::Slider::thumbColourId,       Theme::zinc200);
    volume_.setRange(0.0, 1.0, 0.01);
    volume_.setValue(0.85, juce::dontSendNotification);
    volume_.onValueChange = [this]() { video_.setAudioVolume((float)volume_.getValue()); };
    addAndMakeVisible(volume_);

    timeLabel_.setText("--:-- / --:--", juce::dontSendNotification);
    timeLabel_.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
    timeLabel_.setColour(juce::Label::textColourId, Theme::zinc300);
    timeLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(timeLabel_);

    startTimerHz(15);
}

VideoPanel::~VideoPanel()
{
    stopTimer();
    video_.closeVideo();
}

void VideoPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Drop shadow rim
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(bounds, 10.0f);

    // Chassis (zinc-850 → zinc-900 gradient)
    juce::ColourGradient body(Theme::zinc850, 0.0f, 0.0f,
                              Theme::zinc900, 0.0f, bounds.getHeight(), false);
    g.setGradientFill(body);
    g.fillRoundedRectangle(bounds.reduced(1.0f), 9.0f);

    // Border
    g.setColour(Theme::zinc700);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

    // Title bar separator
    g.setColour(Theme::zinc800);
    g.drawHorizontalLine(28, 6.0f, bounds.getWidth() - 6.0f);

    // Helpful message when no video is loaded
    if (!video_.isVideoOpen())
    {
        auto msg = juce::Rectangle<float>(0.0f, 36.0f, bounds.getWidth(), bounds.getHeight() - 80.0f);
        g.setColour(Theme::zinc600);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f));
        g.drawFittedText(
            "Drop a video here or click \"Open...\"\n(.mp4, .mov, .mkv, .avi, .webm)",
            msg.toNearestInt(), juce::Justification::centred, 2);
    }
}

void VideoPanel::resized()
{
    auto b = getLocalBounds();

    // Header strip
    auto header = b.removeFromTop(28);
    titleLabel_.setBounds(header.reduced(10, 0));
    closeBtn_.setBounds(header.removeFromRight(28).reduced(4));

    // Transport strip (bottom)
    auto transport = b.removeFromBottom(44).reduced(8, 6);
    openBtn_.setBounds(transport.removeFromLeft(70));
    transport.removeFromLeft(6);
    playBtn_.setBounds(transport.removeFromLeft(70));
    transport.removeFromLeft(8);

    // Time label on the right
    auto timeR = transport.removeFromRight(110);
    timeLabel_.setBounds(timeR);

    // Volume on the right (before the time label)
    transport.removeFromRight(6);
    volume_.setBounds(transport.removeFromRight(110));

    // Scrub takes the remaining middle
    transport.removeFromLeft(4);
    transport.removeFromRight(8);
    scrub_.setBounds(transport);

    // Video fills the rest
    video_.setBounds(b.reduced(8, 4));
}

void VideoPanel::mouseDown(const juce::MouseEvent& e)
{
    // Click on the empty area (when no video) → open dialog
    if (!video_.isVideoOpen() && e.y > 36 && e.y < getHeight() - 50)
        showOpenDialog();
}

bool VideoPanel::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".mp4" || ext == ".mov" || ext == ".mkv"
         || ext == ".avi" || ext == ".webm" || ext == ".m4v"
         || ext == ".wmv" || ext == ".flv")
            return true;
    }
    return false;
}

void VideoPanel::filesDropped(const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.existsAsFile())
        {
            if (loadVideoFile(file)) return;
        }
    }
}

bool VideoPanel::loadVideoFile(const juce::File& f)
{
    if (!f.existsAsFile()) return false;

    auto result = video_.load(f);
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Couldn't load video",
            f.getFileName() + "\n\n" + result.getErrorMessage());
        return false;
    }

    currentFile_ = f;
    titleLabel_.setText("VIDEO  " + f.getFileName().toUpperCase(),
                        juce::dontSendNotification);
    video_.setAudioVolume((float)volume_.getValue());
    video_.play();
    playBtn_.setButtonText("Pause");
    repaint();
    return true;
}

void VideoPanel::showOpenDialog()
{
    fileChooser_.reset(new juce::FileChooser(
        "Open Video",
        juce::File::getSpecialLocation(juce::File::userMoviesDirectory),
        "*.mp4;*.mov;*.mkv;*.avi;*.webm;*.m4v;*.wmv;*.flv"));

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile()) loadVideoFile(f);
        });
}

void VideoPanel::togglePlay()
{
    if (!video_.isVideoOpen())
    {
        showOpenDialog();
        return;
    }
    if (video_.isPlaying())
    {
        video_.stop();
        playBtn_.setButtonText("Play");
    }
    else
    {
        video_.play();
        playBtn_.setButtonText("Pause");
    }
}

void VideoPanel::timerCallback()
{
    updateTransportUi();
}

void VideoPanel::updateTransportUi()
{
    if (!video_.isVideoOpen())
    {
        timeLabel_.setText("--:-- / --:--", juce::dontSendNotification);
        scrub_.setValue(0.0, juce::dontSendNotification);
        return;
    }

    const double pos = video_.getPlayPosition();
    const double len = juce::jmax(0.001, video_.getVideoDuration());

    if (!scrubbing_)
        scrub_.setValue(juce::jlimit(0.0, 1.0, pos / len), juce::dontSendNotification);

    timeLabel_.setText(formatTime(pos) + " / " + formatTime(len),
                       juce::dontSendNotification);

    // Keep play button label in sync if playback ended naturally.
    if (!video_.isPlaying() && playBtn_.getButtonText() == "Pause")
        playBtn_.setButtonText("Play");
}

juce::String VideoPanel::formatTime(double seconds)
{
    if (seconds <= 0.0 || std::isnan(seconds)) return "00:00";
    int s   = (int)seconds;
    int m   = s / 60;
    s      %= 60;
    int h   = m / 60;
    m      %= 60;

    juce::String mm = juce::String(m).paddedLeft('0', 2);
    juce::String ss = juce::String(s).paddedLeft('0', 2);
    if (h > 0)
        return juce::String(h) + ":" + mm + ":" + ss;
    return mm + ":" + ss;
}
