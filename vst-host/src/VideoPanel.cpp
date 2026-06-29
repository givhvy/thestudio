#include "VideoPanel.h"
#include "Theme.h"

#if JUCE_WINDOWS
using HWND_t = void*;
extern "C" __declspec(dllimport) int __stdcall DragAcceptFiles(HWND_t, int);

static void disableDragAcceptOnHwnd(HWND_t hwnd)
{
    if (hwnd != nullptr)
        DragAcceptFiles(hwnd, 0);
}

static void disableDragAcceptOnWebViewChildren(juce::Component& browser)
{
    if (auto* peer = browser.getPeer())
        disableDragAcceptOnHwnd((HWND_t) peer->getNativeHandle());

    auto walk = [](juce::Component& c, auto& recurse) -> void
    {
        if (auto* p = c.getPeer())
            disableDragAcceptOnHwnd((HWND_t) p->getNativeHandle());
        for (int i = 0; i < c.getNumChildComponents(); ++i)
            recurse(*c.getChildComponent(i), recurse);
    };
    walk(browser, walk);
}
#endif

namespace
{
class VideoDropOverlay : public juce::Component,
                         public juce::FileDragAndDropTarget
{
public:
    std::function<bool(const juce::StringArray&)> acceptsFiles;
    std::function<void(const juce::File&)>          onFileDropped;
    std::function<void(bool)>                       onDragHighlight;

    void paint(juce::Graphics& g) override
    {
        if (!highlight_)
            return;

        g.setColour(Theme::orange1.withAlpha(0.14f));
        g.fillAll();
        g.setColour(Theme::orange1);
        g.drawRect(getLocalBounds(), 2);

        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
        g.drawText("Drop video to play", getLocalBounds(), juce::Justification::centred);
    }

    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        return acceptsFiles ? acceptsFiles(files) : false;
    }

    void fileDragEnter(const juce::StringArray& files, int, int) override
    {
        if (isInterestedInFileDrag(files))
            setHighlight(true);
    }

    void fileDragExit(const juce::StringArray&) override
    {
        setHighlight(false);
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        setHighlight(false);
        for (const auto& path : files)
        {
            juce::File f(path);
            if (f.existsAsFile() && onFileDropped)
            {
                onFileDropped(f);
                return;
            }
        }
    }

private:
    void setHighlight(bool on)
    {
        if (highlight_ == on)
            return;
        highlight_ = on;
        if (onDragHighlight)
            onDragHighlight(on);
        repaint();
    }

    bool highlight_ = false;
};
} // namespace

VideoPanel::VideoPanel()
{
    setOpaque(true);
    setInterceptsMouseClicks(true, true);

    web_ = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
           #if JUCE_WINDOWS
            .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
           #else
            .withBackend(juce::WebBrowserComponent::Options::Backend::defaultBackend)
           #endif
    );
    addChildComponent(*web_);

    dropOverlay_ = std::make_unique<VideoDropOverlay>();
    auto* overlay = static_cast<VideoDropOverlay*>(dropOverlay_.get());
    overlay->setInterceptsMouseClicks(false, false);
    overlay->acceptsFiles = [](const juce::StringArray& files) {
        for (const auto& f : files)
            if (VideoPanel::isVideoFile(juce::File(f)))
                return true;
        return false;
    };
    overlay->onFileDropped = [this](const juce::File& f) { handleFileDrop({ f.getFullPathName() }); };
    overlay->onDragHighlight = [this](bool on) { setFileDragHighlight(on); };
    addAndMakeVisible(*dropOverlay_);

    titleLabel_.setText("VIDEO", juce::dontSendNotification);
    titleLabel_.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
    titleLabel_.setColour(juce::Label::textColourId, Theme::orange1);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel_);

    closeBtn_.setColour(juce::TextButton::buttonColourId, Theme::zinc800);
    closeBtn_.setColour(juce::TextButton::textColourOffId, Theme::zinc300);
    closeBtn_.onClick = [this]() {
        stopPlayback();
        if (onClose) onClose();
    };
    addAndMakeVisible(closeBtn_);

    openBtn_.setColour(juce::TextButton::buttonColourId, Theme::orange3);
    openBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    openBtn_.onClick = [this]() { showOpenDialog(); };
    addAndMakeVisible(openBtn_);

    sessionBtn_.setColour(juce::TextButton::buttonColourId, Theme::zinc800);
    sessionBtn_.setColour(juce::TextButton::textColourOffId, Theme::zinc200);
    sessionBtn_.onClick = [this]() {
        if (onOpenInSessionTab && hasVideoLoaded())
            onOpenInSessionTab();
    };
    sessionBtn_.setEnabled(false);
    addAndMakeVisible(sessionBtn_);

    constrainer_.setMinimumSize(360, 220);
    constrainer_.setMaximumSize(4096, 4096);
    addAndMakeVisible(resizer_);
    resizer_.setAlwaysOnTop(true);

    showEmptyPage();
}

VideoPanel::~VideoPanel()
{
    saveWindowState();
    unembedPlayerFromSession();
    web_.reset();
    dropOverlay_.reset();
    if (tempHtmlFile_.existsAsFile()) tempHtmlFile_.deleteFile();
}

void VideoPanel::syncWebPlayerBounds()
{
    if (!web_)
        return;

    juce::Rectangle<int> area;
    if (embeddedInSession_ && embeddedHost_ != nullptr)
    {
        if (auto* host = embeddedHost_)
            host->resized();
        area = embeddedHost_->getLocalBounds();
    }
    else
    {
        area = cleanPlayerMode_ ? getLocalBounds()
                                : getLocalBounds().withTrimmedTop(34).reduced(6, 4);
    }

    if (area.getWidth() < 2 || area.getHeight() < 2)
        return;

    web_->setBounds(area);
    if (dropOverlay_)
        dropOverlay_->setBounds(area);

    web_->toFront(false);
    if (dropOverlay_)
        dropOverlay_->toBack();
}

void VideoPanel::scheduleWebLayoutSync()
{
    for (int delayMs : { 0, 40, 120, 280 })
    {
        juce::Timer::callAfterDelay(delayMs, [safe = juce::Component::SafePointer<VideoPanel>(this)]()
        {
            if (safe != nullptr)
                safe->syncWebPlayerBounds();
        });
    }
}

void VideoPanel::reloadWebPlayer()
{
    if (!web_ || !tempHtmlFile_.existsAsFile())
        return;

    web_->goToURL(juce::URL(tempHtmlFile_).toString(false));
}

void VideoPanel::embedPlayerInSession(juce::Component* host)
{
    if (host == nullptr || !hasVideoLoaded() || web_ == nullptr)
        return;

    if (embeddedHost_ != host)
        unembedPlayerFromSession();

    embeddedHost_ = host;
    embeddedInSession_ = true;

    host->addAndMakeVisible(*web_);
    if (dropOverlay_)
        host->addAndMakeVisible(*dropOverlay_);

    setWebVisible(true);
    syncWebPlayerBounds();
    reloadWebPlayer();
    scheduleWebLayoutSync();

    sessionBtn_.setButtonText("In SESSION tab");
    sessionBtn_.setEnabled(false);
    repaint();
}

void VideoPanel::unembedPlayerFromSession()
{
    if (!embeddedInSession_ || web_ == nullptr)
        return;

    addAndMakeVisible(*web_);
    if (dropOverlay_)
        addAndMakeVisible(*dropOverlay_);

    embeddedInSession_ = false;
    embeddedHost_ = nullptr;

    sessionBtn_.setButtonText("Open in session tab");
    sessionBtn_.setEnabled(hasVideoLoaded());

    syncWebPlayerBounds();
    scheduleWebLayoutSync();
    repaint();
}

void VideoPanel::paintEmbeddedPlaceholder(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(0xff09090b));
    g.fillRect(area);

    g.setColour(Theme::zinc600);
    g.drawRoundedRectangle(area.toFloat().reduced(2.0f), 6.0f, 1.0f);

    g.setColour(Theme::orange1);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
    g.drawText("Playing in SESSION tab", area.withTrimmedBottom(28), juce::Justification::centred);

    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f));
    g.drawText("Use SESSION on the bottom dock to show project info again.",
               area.withTrimmedTop(area.getHeight() / 2), juce::Justification::centredTop);
}

bool VideoPanel::canAcceptVideoFiles(const juce::StringArray& files)
{
    for (const auto& path : files)
        if (isVideoFile(juce::File(path)))
            return true;
    return false;
}

bool VideoPanel::isVideoFile(const juce::File& f)
{
    if (!f.existsAsFile())
        return false;

    const auto ext = f.getFileExtension().toLowerCase();
    return ext == ".mp4" || ext == ".mov" || ext == ".mkv"
        || ext == ".avi" || ext == ".webm" || ext == ".m4v"
        || ext == ".wmv" || ext == ".flv" || ext == ".ogv"
        || ext == ".ts"  || ext == ".mpg" || ext == ".mpeg";
}

juce::String VideoPanel::buildPlayerHtml(const juce::File& videoFile, bool cleanPlayer, const juce::String& sourceUrl)
{
    const auto mime    = [&]() {
        const auto ext = videoFile.getFileExtension().toLowerCase();
        if (ext == ".mp4" || ext == ".m4v") return "video/mp4";
        if (ext == ".webm") return "video/webm";
        if (ext == ".ogv" || ext == ".ogg") return "video/ogg";
        if (ext == ".mov") return "video/quicktime";
        if (ext == ".mkv") return "video/x-matroska";
        if (ext == ".avi") return "video/x-msvideo";
        if (ext == ".wmv") return "video/x-ms-wmv";
        if (ext == ".flv") return "video/x-flv";
        if (ext == ".ts")  return "video/mp2t";
        if (ext == ".mpg" || ext == ".mpeg") return "video/mpeg";
        return "video/mp4";
    }();

    if (cleanPlayer)
    {
        return juce::String(R"HTML(<!doctype html>
<html><head><meta charset="utf-8"/>
<style>
  * { box-sizing: border-box; }
  html, body {
    margin: 0; padding: 0; width: 100%; height: 100%;
    overflow: hidden; background: #000; user-select: none;
  }
  video {
    display: block; width: 100vw; height: 100vh;
    object-fit: contain; background: #000; outline: none; cursor: none;
  }
</style></head>
<body>
<video id="v" playsinline muted autoplay loop>
  <source src=")HTML")
            + sourceUrl + R"HTML(" type=")HTML" + mime + R"HTML("/>
</video>
<script>
(function () {
  const v = document.getElementById('v');
  const tryPlay = () => v.play().catch(() => {});
  v.addEventListener('loadedmetadata', tryPlay);
  v.addEventListener('loadeddata', tryPlay);
  window.addEventListener('load', tryPlay);
  setTimeout(tryPlay, 80);
  document.addEventListener('click', () => {
    if (v.paused) tryPlay(); else v.pause();
  });
  v.addEventListener('error', () => {
    document.body.innerHTML =
      '<div style="display:flex;height:100%;align-items:center;justify-content:center;'
      + 'color:#f97316;background:#000;font:13px Segoe UI, sans-serif;">Could not decode video</div>';
  });
})();
</script>
</body></html>)HTML";
    }

    return juce::String(R"HTML(<!doctype html>
<html><head><meta charset="utf-8"/>
<style>
  :root {
    --bg: #09090b;
    --panel: #18181b;
    --line: #27272a;
    --text: #e4e4e7;
    --muted: #71717a;
    --accent: #f97316;
    --accent2: #ea580c;
  }
  * { box-sizing: border-box; }
  html, body {
    margin: 0; padding: 0; height: 100%; overflow: hidden;
    background: var(--bg); color: var(--text);
    font-family: 'Segoe UI', system-ui, sans-serif;
    user-select: none;
  }
  .stage {
    position: relative;
    height: 100%;
    width: 100%;
    background: #000;
    overflow: hidden;
  }
  .viewport {
    position: absolute;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    background: #000;
  }
  video {
    max-width: 100%;
    max-height: 100%;
    width: 100%;
    height: 100%;
    object-fit: contain;
    background: #000;
    outline: none;
    cursor: none;
  }
  .stage.show-ui video { cursor: default; }
  .controls {
    position: absolute;
    left: 0;
    right: 0;
    bottom: 0;
    z-index: 5;
    padding: 28px 10px 10px;
    display: flex;
    flex-direction: column;
    gap: 6px;
    background: linear-gradient(180deg, transparent 0%, rgba(0,0,0,0.55) 35%, rgba(9,9,11,0.92) 100%);
    border-top: none;
    opacity: 1;
    transform: translateY(0);
    transition: opacity 0.3s ease, transform 0.3s ease;
    pointer-events: auto;
  }
  .controls.hidden {
    opacity: 0;
    transform: translateY(10px);
    pointer-events: none;
  }
  .row { display: flex; align-items: center; gap: 8px; }
  button.icon {
    width: 34px; height: 34px; border: 1px solid var(--line);
    border-radius: 6px; background: #27272a; color: var(--text);
    cursor: pointer; font-size: 14px; line-height: 1;
  }
  button.icon:hover { border-color: var(--accent); color: #fff; }
  button.icon.active {
    background: linear-gradient(180deg, var(--accent) 0%, var(--accent2) 100%);
    border-color: #fdba74; color: #111;
  }
  button.text-btn {
    height: 34px; padding: 0 12px; border-radius: 6px;
    border: 1px solid var(--line); background: #27272a;
    color: var(--text); cursor: pointer; font-size: 11px; font-weight: 700;
  }
  button.text-btn:hover { border-color: var(--accent); }
  button.text-btn.unmuted {
    background: linear-gradient(180deg, var(--accent) 0%, var(--accent2) 100%);
    border-color: #fdba74; color: #111;
  }
  .time { font-size: 11px; color: var(--muted); min-width: 88px; text-align: center; }
  input[type=range] {
    flex: 1; height: 6px; appearance: none; border-radius: 999px;
    background: #3f3f46; outline: none;
  }
  input[type=range]::-webkit-slider-thumb {
    appearance: none; width: 12px; height: 12px; border-radius: 50%;
    background: var(--accent); border: 2px solid #fff;
  }
</style></head>
<body>
<div class="stage" id="stage">
  <div class="viewport">
    <video id="v" playsinline muted autoplay>
      <source src=")HTML")
        + sourceUrl + R"HTML(" type=")HTML" + mime + R"HTML("/>
    </video>
    <div class="controls" id="controls">
      <div class="row">
        <button class="icon" id="playBtn" title="Play / Pause">&#9654;</button>
        <input type="range" id="seek" min="0" max="1000" value="0"/>
        <div class="time" id="time">0:00 / 0:00</div>
        <button class="text-btn" id="muteBtn" title="Sound off by default">Unmute</button>
        <button class="icon" id="fsBtn" title="Fullscreen">&#9974;</button>
      </div>
    </div>
  </div>
</div>
<script>
(function () {
  const v = document.getElementById('v');
  const stage = document.getElementById('stage');
  const controls = document.getElementById('controls');
  const playBtn = document.getElementById('playBtn');
  const seek = document.getElementById('seek');
  const timeEl = document.getElementById('time');
  const muteBtn = document.getElementById('muteBtn');
  const fsBtn = document.getElementById('fsBtn');

  let muted = true;
  v.muted = true;

  const IDLE_MS = 2200;
  let hideTimer = null;
  let scrubbing = false;

  const setUiVisible = (on) => {
    controls.classList.toggle('hidden', !on);
    stage.classList.toggle('show-ui', on);
  };

  const scheduleHide = () => {
    clearTimeout(hideTimer);
    hideTimer = setTimeout(() => {
      if (!scrubbing)
        setUiVisible(false);
    }, IDLE_MS);
  };

  const showControls = (autoHide) => {
    setUiVisible(true);
    if (autoHide !== false)
      scheduleHide();
  };

  stage.addEventListener('mousemove', () => showControls());
  stage.addEventListener('mouseenter', () => showControls());
  controls.addEventListener('mouseenter', () => {
    clearTimeout(hideTimer);
    setUiVisible(true);
  });
  controls.addEventListener('mouseleave', () => scheduleHide());

  seek.addEventListener('pointerdown', () => {
    scrubbing = true;
    showControls(false);
  });
  window.addEventListener('pointerup', () => {
    if (!scrubbing) return;
    scrubbing = false;
    scheduleHide();
  });

  const fmt = (sec) => {
    if (!isFinite(sec) || sec < 0) sec = 0;
    const m = Math.floor(sec / 60);
    const s = Math.floor(sec % 60);
    return m + ':' + String(s).padStart(2, '0');
  };

  const updateTime = () => {
    timeEl.textContent = fmt(v.currentTime) + ' / ' + fmt(v.duration || 0);
    if (v.duration > 0)
      seek.value = Math.floor(1000 * v.currentTime / v.duration);
  };

  const updatePlayBtn = () => {
    playBtn.textContent = v.paused ? '\u25B6' : '\u23F8';
    playBtn.classList.toggle('active', !v.paused);
  };

  const updateMuteBtn = () => {
    muteBtn.textContent = muted ? 'Unmute' : 'Mute';
    muteBtn.classList.toggle('unmuted', !muted);
  };

  const tryPlay = () => v.play().then(updatePlayBtn).catch(() => updatePlayBtn());

  playBtn.onclick = () => {
    if (v.paused) tryPlay(); else { v.pause(); updatePlayBtn(); }
  };

  muteBtn.onclick = () => {
    muted = !muted;
    v.muted = muted;
    updateMuteBtn();
  };

  fsBtn.onclick = () => {
    const el = document.documentElement;
    if (!document.fullscreenElement) el.requestFullscreen?.();
    else document.exitFullscreen?.();
  };

  seek.oninput = () => {
    if (v.duration > 0)
      v.currentTime = (seek.value / 1000) * v.duration;
    updateTime();
  };

  v.addEventListener('timeupdate', updateTime);
  v.addEventListener('loadedmetadata', () => { updateTime(); tryPlay(); });
  v.addEventListener('play', updatePlayBtn);
  v.addEventListener('pause', updatePlayBtn);
  v.addEventListener('ended', updatePlayBtn);

  v.addEventListener('click', () => {
    showControls();
    if (v.paused) tryPlay(); else { v.pause(); updatePlayBtn(); }
  });

  document.addEventListener('keydown', (e) => {
    showControls();
    if (e.code === 'Space') { e.preventDefault(); playBtn.click(); }
    if (e.code === 'KeyM') { muteBtn.click(); }
  });

  // Start with controls visible briefly, then hide for maximum video area.
  showControls(false);
  setTimeout(() => setUiVisible(false), 1800);

  v.addEventListener('error', () => {
    document.body.innerHTML =
      '<div style="display:flex;height:100%;align-items:center;justify-content:center;'
      + 'flex-direction:column;color:#a1a1aa;background:#09090b;">'
      + '<div style="font-size:14px;margin-bottom:8px;color:#f97316;">Could not decode video</div>'
      + '<div style="font-size:12px;">Try H.264 / MP4, or another supported format.</div></div>';
  });

  const nudgeLayout = () => {
    document.documentElement.style.height = '100%';
    document.body.style.height = '100%';
    window.dispatchEvent(new Event('resize'));
  };
  v.addEventListener('loadeddata', nudgeLayout);
  window.addEventListener('load', nudgeLayout);
  setTimeout(nudgeLayout, 80);
  setTimeout(nudgeLayout, 300);

  updateMuteBtn();
  updatePlayBtn();
  tryPlay();
})();
</script>
</body></html>)HTML";
}

void VideoPanel::setFileDragHighlight(bool on)
{
    if (fileDragOver_ == on)
        return;
    fileDragOver_ = on;
    repaint();
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

void VideoPanel::paintEmptyState(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(0xff09090b));
    g.fillRect(area);

    if (fileDragOver_)
    {
        g.setColour(Theme::orange1.withAlpha(0.12f));
        g.fillRect(area);
        g.setColour(Theme::orange1);
        g.drawRect(area, 2);
    }

    auto card = area.withSizeKeepingCentre(juce::jmin(area.getWidth() - 24, 360),
                                           juce::jmin(area.getHeight() - 24, 200));

    g.setColour(Theme::zinc700);
    g.drawEllipse(card.getX() + card.getWidth() / 2 - 28, card.getY() + 8, 56.0f, 56.0f, 2.0f);

    juce::Path tri;
    tri.addTriangle((float) card.getCentreX() - 10, (float) card.getY() + 28,
                    (float) card.getCentreX() - 10, (float) card.getY() + 52,
                    (float) card.getCentreX() + 14, (float) card.getY() + 40);
    g.setColour(fileDragOver_ ? Theme::orange1 : Theme::zinc500);
    g.fillPath(tri);

    g.setColour(Theme::zinc100);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(15.0f).withStyle("Bold"));
    g.drawText(fileDragOver_ ? "Release to load video" : "Drop video here",
               card.withTrimmedTop(72), juce::Justification::centredTop);

    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.5f));
    g.drawMultiLineText("MP4, MOV, MKV, WEBM, AVI and more.\n"
                        "Playback starts muted. Use Unmute in the player for sound.",
                        card.getX() + 8, card.getY() + 98, card.getWidth() - 16,
                        juce::Justification::centred);

    auto pill = juce::Rectangle<int>(card.getCentreX() - 90, card.getBottom() - 34, 180, 22);
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(pill.toFloat(), 11.0f);
    g.setColour(Theme::zinc600);
    g.drawRoundedRectangle(pill.toFloat(), 11.0f, 1.0f);
    g.setColour(Theme::zinc400);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("or click Open Video...", pill, juce::Justification::centred);
}

void VideoPanel::setWebVisible(bool visible)
{
    webVisible_ = visible;
    if (!web_)
        return;

    web_->setVisible(visible);
    if (visible)
    {
#if JUCE_WINDOWS
        // WebView2 steals OS file drops unless we turn off DragAcceptFiles on its HWND.
        juce::Timer::callAfterDelay(120, [safe = juce::Component::SafePointer<VideoPanel>(this)]()
        {
            if (safe == nullptr || safe->web_ == nullptr || !safe->web_->isVisible())
                return;
            disableDragAcceptOnWebViewChildren(*safe->web_);
        });
#endif
    }
    repaint();
}

void VideoPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (cleanPlayerMode_)
    {
        g.fillAll(juce::Colours::black);
        return;
    }

    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(bounds, 10.0f);

    juce::ColourGradient body(Theme::zinc850, 0.0f, 0.0f,
                              Theme::zinc900, 0.0f, bounds.getHeight(), false);
    g.setGradientFill(body);
    g.fillRoundedRectangle(bounds.reduced(1.0f), 9.0f);

    g.setColour(fileDragOver_ ? Theme::orange1 : Theme::zinc700);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, fileDragOver_ ? 2.5f : 1.0f);

    g.setColour(Theme::zinc800);
    g.drawHorizontalLine(34, 6.0f, bounds.getWidth() - 6.0f);

    if (embeddedInSession_)
    {
        auto videoArea = getLocalBounds().withTrimmedTop(34).reduced(6, 4);
        paintEmbeddedPlaceholder(g, videoArea);
    }
    else if (!webVisible_)
    {
        auto videoArea = getLocalBounds().withTrimmedTop(34).reduced(6, 4);
        paintEmptyState(g, videoArea);
    }
}

void VideoPanel::resized()
{
    auto b = getLocalBounds();

    if (cleanPlayerMode_)
    {
        closeBtn_.setBounds(b.removeFromTop(34).removeFromRight(34).reduced(4));
        openBtn_.setVisible(false);
        sessionBtn_.setVisible(false);
        titleLabel_.setVisible(false);
        resizer_.setVisible(false);
        syncWebPlayerBounds();
        closeBtn_.toFront(false);
        return;
    }

    openBtn_.setVisible(true);
    sessionBtn_.setVisible(true);
    titleLabel_.setVisible(true);
    resizer_.setVisible(true);

    auto header = b.removeFromTop(34);
    closeBtn_.setBounds(header.removeFromRight(32).reduced(4));
    openBtn_.setBounds(header.removeFromRight(108).reduced(4, 5));
    sessionBtn_.setBounds(header.removeFromRight(132).reduced(4, 6));
    titleLabel_.setBounds(header.reduced(10, 0));

    if (embeddedInSession_)
        syncWebPlayerBounds();
    else
        syncWebPlayerBounds();

    resizer_.setBounds(getWidth() - 16, getHeight() - 16, 16, 16);
    resizer_.toFront(false);
    closeBtn_.toFront(false);
    openBtn_.toFront(false);
    sessionBtn_.toFront(false);
    titleLabel_.toFront(false);

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

    if (cleanPlayerMode_)
        return;

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
        if (isVideoFile(juce::File(f)))
            return true;
    return false;
}

void VideoPanel::fileDragEnter(const juce::StringArray& files, int, int)
{
    if (isInterestedInFileDrag(files))
        setFileDragHighlight(true);
}

void VideoPanel::fileDragExit(const juce::StringArray&)
{
    setFileDragHighlight(false);
}

void VideoPanel::filesDropped(const juce::StringArray& files, int, int)
{
    handleFileDrop(files);
}

void VideoPanel::handleFileDrop(const juce::StringArray& files)
{
    setFileDragHighlight(false);
    for (const auto& path : files)
    {
        juce::File file(path);
        if (file.existsAsFile() && loadVideoFile(file))
            return;
    }
}

bool VideoPanel::loadVideoFile(const juce::File& f, bool cleanPlayer)
{
    if (!isVideoFile(f))
        return false;

    cleanPlayerMode_ = cleanPlayer;

    const auto parentDir = f.getParentDirectory();
    tempHtmlFile_ = parentDir.getChildFile("stratum_video_player.html");
    juce::String sourceUrl = f.getFileName();
    auto html = buildPlayerHtml(f, cleanPlayerMode_, sourceUrl);

    if (!tempHtmlFile_.replaceWithText(html))
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        tempHtmlFile_ = tempDir.getChildFile("stratum_video_player.html");
        sourceUrl = juce::URL(f).toString(true);
        html = buildPlayerHtml(f, cleanPlayerMode_, sourceUrl);
        tempHtmlFile_.replaceWithText(html);
    }

    setWebVisible(true);
    if (web_)
    {
        web_->goToURL(juce::URL(tempHtmlFile_).toString(true));
        web_->setInterceptsMouseClicks(true, true);
    }

    currentFile_ = f;
    titleLabel_.setText("VIDEO  " + f.getFileName(), juce::dontSendNotification);
    sessionBtn_.setEnabled(!embeddedInSession_);
    if (dropOverlay_)
        dropOverlay_->toFront(false);

    syncWebPlayerBounds();
    if (embeddedInSession_)
        scheduleWebLayoutSync();

    return true;
}

void VideoPanel::stopPlayback()
{
    if (embeddedInSession_)
        unembedPlayerFromSession();

    if (web_)
        web_->goToURL("about:blank");

    currentFile_ = juce::File();
    cleanPlayerMode_ = false;
    setWebVisible(false);
    titleLabel_.setText("VIDEO", juce::dontSendNotification);
    sessionBtn_.setEnabled(false);
    sessionBtn_.setButtonText("Open in session tab");
    repaint();
}

void VideoPanel::showEmptyPage()
{
    if (embeddedInSession_)
        unembedPlayerFromSession();

    currentFile_ = juce::File();
    cleanPlayerMode_ = false;
    setWebVisible(false);
    titleLabel_.setText("VIDEO", juce::dontSendNotification);
    sessionBtn_.setEnabled(false);
    sessionBtn_.setButtonText("Open in session tab");
    repaint();
}

void VideoPanel::showOpenDialog()
{
    fileChooser_.reset(new juce::FileChooser(
        "Open Video",
        currentFile_.existsAsFile() ? currentFile_.getParentDirectory()
                                    : juce::File::getSpecialLocation(juce::File::userMoviesDirectory),
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
