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
    addAndMakeVisible(titleLabel_);

    closeBtn_.setColour(juce::TextButton::buttonColourId, Theme::zinc800);
    closeBtn_.setColour(juce::TextButton::textColourOffId, Theme::zinc300);
    closeBtn_.onClick = [this]() { if (onClose) onClose(); };
    addAndMakeVisible(closeBtn_);

    openBtn_.setColour(juce::TextButton::buttonColourId, Theme::orange3);
    openBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    openBtn_.onClick = [this]() { showOpenDialog(); };
    addAndMakeVisible(openBtn_);

    showEmptyPage();
}

VideoPanel::~VideoPanel()
{
    web_.reset();
    if (tempHtmlFile_.existsAsFile()) tempHtmlFile_.deleteFile();
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
}

void VideoPanel::mouseDown(const juce::MouseEvent&)
{
    // Header click; ignored (children handle their own clicks)
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
