#include "VideoPanel.h"
#include "Theme.h"

VideoPanel::VideoPanel()
{
    setOpaque(true);
    setInterceptsMouseClicks(true, true);

    web_ = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withBackend(juce::WebBrowserComponent::Options::Backend::webview2));
    addAndMakeVisible(*web_);

    // ── Header ───────────────────────────────────────────────────
    titleLabel_.setText("VIDEO", juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
    titleLabel_.setColour(juce::Label::textColourId, Theme::orange1);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel_);

    closeBtn_.setColour(juce::TextButton::buttonColourId, Theme::zinc800);
    closeBtn_.setColour(juce::TextButton::textColourOffId, Theme::zinc300);
    closeBtn_.onClick = [this]() { if (onClose) onClose(); };
    addAndMakeVisible(closeBtn_);

    openBtn_.setColour(juce::TextButton::buttonColourId, Theme::orange3);
    openBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    openBtn_.onClick = [this]() { showOpenDialog(); };
    addAndMakeVisible(openBtn_);

    // Resizable corner + size limits
    constrainer_.setMinimumSize(360, 220);
    constrainer_.setMaximumSize(4096, 4096);
    addAndMakeVisible(resizer_);
    resizer_.setAlwaysOnTop(true);

    showEmptyPage();
}

VideoPanel::~VideoPanel()
{
    saveWindowState();
    web_.reset();
    if (tempHtmlFile_.existsAsFile()) tempHtmlFile_.deleteFile();
}

juce::File VideoPanel::stateFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stratum");
    dir.createDirectory();
    return dir.getChildFile("video-panel-bounds.json");
}

juce::Rectangle<int> VideoPanel::constrainToParent(juce::Rectangle<int> bounds,
                                                   juce::Rectangle<int> parentBounds) const
{
    const int minW = 360;
    const int minH = 220;
    const int maxW = juce::jmax(minW, parentBounds.getWidth() - 24);
    const int maxH = juce::jmax(minH, parentBounds.getHeight() - 24);

    bounds.setSize(juce::jlimit(minW, maxW, bounds.getWidth()),
                   juce::jlimit(minH, maxH, bounds.getHeight()));

    const int minX = parentBounds.getX() + 8;
    const int minY = parentBounds.getY() + 8;
    const int maxX = parentBounds.getRight() - bounds.getWidth() - 8;
    const int maxY = parentBounds.getBottom() - bounds.getHeight() - 8;
    bounds.setPosition(juce::jlimit(minX, juce::jmax(minX, maxX), bounds.getX()),
                       juce::jlimit(minY, juce::jmax(minY, maxY), bounds.getY()));
    return bounds;
}

void VideoPanel::saveWindowState() const
{
    if (getWidth() < 100 || getHeight() < 100)
        return;

    auto* obj = new juce::DynamicObject();
    auto b = getBounds();
    obj->setProperty("x", b.getX());
    obj->setProperty("y", b.getY());
    obj->setProperty("w", b.getWidth());
    obj->setProperty("h", b.getHeight());
    stateFile().replaceWithText(juce::JSON::toString(juce::var(obj)));
}

juce::Rectangle<int> VideoPanel::getSavedOrDefaultBounds(juce::Rectangle<int> parentBounds,
                                                         juce::Rectangle<int> defaultBounds) const
{
    auto f = stateFile();
    if (!f.existsAsFile())
        return constrainToParent(defaultBounds, parentBounds);

    auto parsed = juce::JSON::parse(f);
    if (!parsed.isObject())
        return constrainToParent(defaultBounds, parentBounds);

    juce::Rectangle<int> saved(
        (int)parsed.getProperty("x", defaultBounds.getX()),
        (int)parsed.getProperty("y", defaultBounds.getY()),
        (int)parsed.getProperty("w", defaultBounds.getWidth()),
        (int)parsed.getProperty("h", defaultBounds.getHeight()));

    return constrainToParent(saved, parentBounds);
}

void VideoPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(bounds, 10.0f);

    juce::ColourGradient body(Theme::zinc850, 0.0f, 0.0f,
                              Theme::zinc900, 0.0f, bounds.getHeight(), false);
    g.setGradientFill(body);
    g.fillRoundedRectangle(bounds.reduced(1.0f), 9.0f);

    g.setColour(Theme::zinc700);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

    g.setColour(Theme::zinc800);
    g.drawHorizontalLine(34, 6.0f, bounds.getWidth() - 6.0f);
}

void VideoPanel::resized()
{
    auto b = getLocalBounds();

    // Header
    auto header = b.removeFromTop(34);
    closeBtn_.setBounds(header.removeFromRight(32).reduced(4));
    openBtn_.setBounds(header.removeFromRight(120).reduced(4, 5));
    titleLabel_.setBounds(header.reduced(10, 0));

    // Video fills the rest
    if (web_) web_->setBounds(b.reduced(6, 4));

    // Resize grip in the bottom-right corner
    resizer_.setBounds(getWidth() - 16, getHeight() - 16, 16, 16);

    if (isVisible() && getAlpha() > 0.95f)
        saveWindowState();
}

void VideoPanel::moved()
{
    if (isVisible() && getAlpha() > 0.95f)
        saveWindowState();
}

void VideoPanel::mouseDown(const juce::MouseEvent& e)
{
    draggingPanel_ = false;

    const bool onResizeGrip = juce::Rectangle<int>(getWidth() - 22, getHeight() - 22, 22, 22).contains(e.x, e.y);
    const bool onHeaderButton = openBtn_.getBounds().contains(e.x, e.y) || closeBtn_.getBounds().contains(e.x, e.y);
    const bool canDragBody = !currentFile_.existsAsFile();

    if (!onResizeGrip && !onHeaderButton && (e.y <= 34 || canDragBody))
    {
        draggingPanel_ = true;
        dragger_.startDraggingComponent(this, e);
    }
}

void VideoPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingPanel_)
    {
        dragger_.dragComponent(this, e, &constrainer_);
        saveWindowState();
    }
}

bool VideoPanel::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".mp4" || ext == ".mov" || ext == ".mkv"
         || ext == ".avi" || ext == ".webm" || ext == ".m4v"
         || ext == ".wmv" || ext == ".flv" || ext == ".ogv"
         || ext == ".ts"  || ext == ".mpg" || ext == ".mpeg")
            return true;
    }
    return false;
}

void VideoPanel::filesDropped(const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.existsAsFile() && loadVideoFile(file)) return;
    }
}

// Pick a MIME type that WebView2 / Chromium understands.
static juce::String mimeForExtension(const juce::String& extLower)
{
    if (extLower == ".mp4" || extLower == ".m4v") return "video/mp4";
    if (extLower == ".webm")                       return "video/webm";
    if (extLower == ".ogv" || extLower == ".ogg")  return "video/ogg";
    if (extLower == ".mov")                        return "video/quicktime";
    if (extLower == ".mkv")                        return "video/x-matroska";
    if (extLower == ".avi")                        return "video/x-msvideo";
    if (extLower == ".wmv")                        return "video/x-ms-wmv";
    if (extLower == ".flv")                        return "video/x-flv";
    if (extLower == ".ts")                         return "video/mp2t";
    if (extLower == ".mpg" || extLower == ".mpeg") return "video/mpeg";
    return "video/mp4";
}

bool VideoPanel::loadVideoFile(const juce::File& f)
{
    if (!f.existsAsFile()) return false;

    // Build a file:// URL with proper escaping (spaces, unicode etc.).
    auto fileUrl = juce::URL(f).toString(false);
    auto mime    = mimeForExtension(f.getFileExtension().toLowerCase());

    juce::String html = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"/>
<style>
  html,body{margin:0;padding:0;background:#0a0a0c;height:100%;overflow:hidden;
            font-family:'Segoe UI',sans-serif;color:#d4d4d8;}
  .wrap{display:flex;align-items:center;justify-content:center;height:100%;}
  video{max-width:100%;max-height:100%;background:#000;outline:none;}
</style></head>
<body><div class="wrap">
  <video id="v" controls autoplay playsinline>
    <source src=")HTML" + fileUrl + R"HTML(" type=")HTML" + mime + R"HTML("/>
    Your browser cannot play this file.
  </video>
</div>
<script>
  // Mute autoplay fallback if browser blocks audio autoplay.
  const v = document.getElementById('v');
  v.addEventListener('error', () => {
    document.body.innerHTML =
      '<div style="display:flex;height:100%;align-items:center;justify-content:center;'
      + 'flex-direction:column;color:#a1a1aa;">'
      + '<div style="font-size:14px;margin-bottom:8px;color:#f97316;">Could not decode video</div>'
      + '<div style="font-size:12px;">Try converting to H.264 / MP4 (AAC audio).</div>'
      + '</div>';
  });
</script>
</body></html>)HTML";

    // Write the HTML wrapper to a temp file and navigate to it.
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    if (!tempHtmlFile_.existsAsFile())
        tempHtmlFile_ = tempDir.getChildFile("stratum_video_player.html");
    tempHtmlFile_.replaceWithText(html);

    if (web_)
        web_->goToURL(juce::URL(tempHtmlFile_).toString(false));
    if (web_)
        web_->setInterceptsMouseClicks(true, true);

    currentFile_ = f;
    titleLabel_.setText("VIDEO  " + f.getFileName().toUpperCase(),
                        juce::dontSendNotification);
    return true;
}

void VideoPanel::showEmptyPage()
{
    juce::String html = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"/><style>
html,body{margin:0;padding:0;background:#0a0a0c;height:100%;
          font-family:'Segoe UI',sans-serif;color:#71717a;
          display:flex;align-items:center;justify-content:center;
          flex-direction:column;}
.box{text-align:center;}
.box .big{font-size:15px;color:#a1a1aa;margin-bottom:10px;}
.box .small{font-size:12px;color:#52525b;}
</style></head>
<body><div class="box">
  <div class="big">No video loaded</div>
  <div class="small">Click <b style="color:#f97316;">Open Video...</b>
    or drag a video file onto this panel.</div>
  <div class="small" style="margin-top:14px;">
    Supports MP4, MOV, MKV, WEBM, AVI, M4V, WMV, FLV, TS, MPEG.
  </div>
</div></body></html>)HTML";

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    if (!tempHtmlFile_.existsAsFile())
        tempHtmlFile_ = tempDir.getChildFile("stratum_video_player.html");
    tempHtmlFile_.replaceWithText(html);
    if (web_) web_->goToURL(juce::URL(tempHtmlFile_).toString(false));
    if (web_) web_->setInterceptsMouseClicks(false, false);
}

void VideoPanel::showOpenDialog()
{
    fileChooser_.reset(new juce::FileChooser(
        "Open Video",
        juce::File::getSpecialLocation(juce::File::userMoviesDirectory),
        "*.mp4;*.mov;*.mkv;*.avi;*.webm;*.m4v;*.wmv;*.flv;*.ts;*.mpg;*.mpeg;*.ogv"));

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile()) loadVideoFile(f);
        });
}
