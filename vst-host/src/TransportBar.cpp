#include "TransportBar.h"
#include "PluginHost.h"
#include "Theme.h"

class TransportButton : public juce::Component
{
public:
    TransportButton(const juce::String& iconType, juce::Colour color)
        : iconType_(iconType), color_(color) {}
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.5f);
        bool isActive = active_ || pressed_;
        
        // Background: dark panel
        juce::ColourGradient bg(
            juce::Colour(0xff161618), 0.0f, bounds.getY(),
            juce::Colour(0xff0a0a0c), 0.0f, bounds.getBottom(), false
        );
        g.setGradientFill(bg);
        g.fillRoundedRectangle(bounds, 5.0f);
        
        // Active state: color tint overlay
        if (isActive)
        {
            g.setColour(color_.withAlpha(0.18f));
            g.fillRoundedRectangle(bounds, 5.0f);
        }
        
        // Border
        g.setColour(isActive ? color_.withAlpha(0.6f) : juce::Colour(0xff3f3f46));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);
        
        // Top highlight
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawHorizontalLine((int)bounds.getY() + 1, bounds.getX() + 5, bounds.getRight() - 5);
        
        // Icon
        auto cx = bounds.getCentreX();
        auto cy = bounds.getCentreY();
        
        if (iconType_ == "play")
        {
            if (active_)
            {
                g.setColour(juce::Colour(0xff2563eb));
                g.fillRoundedRectangle(cx - 5.5f, cy - 6.0f, 4.0f, 12.0f, 1.0f);
                g.fillRoundedRectangle(cx + 1.5f, cy - 6.0f, 4.0f, 12.0f, 1.0f);
            }
            else
            {
                juce::Path p;
                p.addTriangle(cx - 5, cy - 6, cx - 5, cy + 6, cx + 6, cy);
                g.setColour(juce::Colour(0xff10b981).withAlpha(0.75f));
                g.fillPath(p);
            }
        }
        else if (iconType_ == "stop")
        {
            g.setColour(isActive ? juce::Colour(0xffa1a1aa) : juce::Colour(0xff71717a));
            g.fillRect(cx - 5, cy - 5, 10.0f, 10.0f);
        }
        else if (iconType_ == "rec")
        {
            g.setColour(isActive ? juce::Colour(0xffef4444) : juce::Colour(0xffef4444).withAlpha(0.65f));
            g.fillEllipse(cx - 5, cy - 5, 10.0f, 10.0f);
            if (isActive)
            {
                g.setColour(juce::Colour(0xffef4444).withAlpha(0.3f));
                g.drawEllipse(cx - 7, cy - 7, 14.0f, 14.0f, 1.0f);
            }
        }
    }
    
    void mouseDown(const juce::MouseEvent&) override 
    {
        pressed_ = true;
        repaint();
        if (onClick) onClick();
    }
    
    void mouseUp(const juce::MouseEvent&) override 
    {
        pressed_ = false;
        repaint();
    }
    
    void setActive(bool a) { active_ = a; repaint(); }
    
    std::function<void()> onClick;
    
private:
    juce::String iconType_;
    juce::Colour color_;
    bool active_ = false;
    bool pressed_ = false;
};

TransportBar::TransportBar(PluginHost& pluginHost)
    : pluginHost_(pluginHost)
{
    auto* playBtn = new TransportButton("play", Theme::green);
    playBtn->onClick = [this]() { togglePlay(); };
    playBtn_.reset(playBtn);
    addAndMakeVisible(playBtn);
    
    auto* stopBtn = new TransportButton("stop", Theme::text2);
    stopBtn->onClick = [this]() { stop(); };
    stopBtn_.reset(stopBtn);
    addAndMakeVisible(stopBtn);
    
    auto* recBtn = new TransportButton("rec", Theme::red);
    recBtn->onClick = [this]() { toggleRecord(); };
    recordBtn_.reset(recBtn);
    addAndMakeVisible(recBtn);
    
    bpmSlider_.setSliderStyle(juce::Slider::IncDecButtons);
    bpmSlider_.setRange(20.0, 999.0, 1.0);
    bpmSlider_.setValue(130.0);
    bpmSlider_.setColour(juce::Slider::backgroundColourId, Theme::bg3);
    bpmSlider_.setColour(juce::Slider::textBoxTextColourId, Theme::accentBright);
    bpmSlider_.setColour(juce::Slider::textBoxBackgroundColourId, Theme::bg1);
    bpmSlider_.setColour(juce::Slider::textBoxOutlineColourId, Theme::border2);
    bpmSlider_.setColour(juce::Slider::textBoxHighlightColourId, Theme::accent);
    bpmSlider_.onValueChange = [this]() {
        bpm_ = bpmSlider_.getValue();
        if (onBPMChanged) onBPMChanged(bpm_);
    };
    addAndMakeVisible(bpmSlider_);
    
    bpmLabel_.setText("BPM", juce::dontSendNotification);
    bpmLabel_.setColour(juce::Label::textColourId, Theme::text3);
    bpmLabel_.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    bpmLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmLabel_);
    
    timeLabel_.setText("0:00:00", juce::dontSendNotification);
    timeLabel_.setColour(juce::Label::textColourId, Theme::accentBright);
    timeLabel_.setFont(juce::FontOptions().withName("Consolas").withHeight(28.0f).withStyle("Bold"));
    timeLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(timeLabel_);
    
    beatsChecker_ = std::make_unique<BeatsAppChecker>([this]() { checkBeatsAppStatus(); });
    checkBeatsAppStatus();
}

TransportBar::~TransportBar() = default;

void TransportBar::paint(juce::Graphics& g)
{
    int w = getWidth();
    int h = getHeight();
    int cy = h / 2;

    
    // ── Background: #09090b like cmd panel section ──
    g.fillAll(juce::Colour(0xff09090b));
    g.setColour(juce::Colour(0xff1c1c1f));
    g.drawHorizontalLine(0, 0.0f, (float)w);
    g.setColour(juce::Colour(0xff000000));
    g.drawHorizontalLine(h - 1, 0.0f, (float)w);
    
    // ── CMD-Panel helper ─────────────────────────────────────────────
    auto drawPanel = [&](juce::Rectangle<float> r, float radius = 8.0f) {
        juce::ColourGradient bg(juce::Colour(0xff161618), 0.0f, r.getY(),
                                juce::Colour(0xff0a0a0c), 0.0f, r.getBottom(), false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(r, radius);
        g.setColour(juce::Colour(0x883f3f46));
        g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
        g.setColour(juce::Colour(0x0dffffff));
        g.drawHorizontalLine((int)r.getY() + 1, r.getX() + radius, r.getRight() - radius);
    };
    
    auto drawInset = [&](juce::Rectangle<float> r, float radius = 4.0f) {
        g.setColour(juce::Colour(0xff050507));
        g.fillRoundedRectangle(r, radius);
        g.setColour(juce::Colour(0xff27272a));
        g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.0f);
    };
    
    int x = 10;
    
    // ═══════════════════════════════════
    // PAT pill
    // ═══════════════════════════════════
    playbackModeRect_ = juce::Rectangle<float>((float)x, (float)cy - 14, 68, 28);
    drawPanel(playbackModeRect_, 14.0f);
    auto patDot = juce::Rectangle<float>(playbackModeRect_.getX() + 7, playbackModeRect_.getCentreY() - 3, 6.0f, 6.0f);
    Theme::drawGlowLED(g, patDot, Theme::orange2, true);
    g.setColour(juce::Colour(0xffa1a1aa));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.0f).withStyle("Bold"));
    g.drawText(playbackMode_ == PlaybackMode::Rack ? "RACK" : "PLAYLIST",
               (int)playbackModeRect_.getX() + 17, (int)playbackModeRect_.getY(),
               (int)playbackModeRect_.getWidth() - 22, 28, juce::Justification::centredLeft);
    
    x = (int)playbackModeRect_.getRight() + 6;
    
    // ═══════════════════════════════════
    // Transport buttons panel
    // ═══════════════════════════════════
    auto tpBounds = juce::Rectangle<float>((float)x, (float)cy - 15, 86, 30);
    drawPanel(tpBounds, 15.0f);
    
    x = (int)tpBounds.getRight() + 6;
    
    // ═══════════════════════════════════
    // BPM panel
    // ═══════════════════════════════════
    bpmBounds_ = juce::Rectangle<float>((float)x, (float)cy - 14, 84, 28);
    drawPanel(bpmBounds_, 8.0f);
    
    auto bpmLCD = juce::Rectangle<float>(bpmBounds_.getX() + 5, bpmBounds_.getY() + 4, bpmBounds_.getWidth() - 10, 20);
    drawInset(bpmLCD, 4.0f);
    if (isDraggingBPM_)
    {
        g.setColour(Theme::orange1.withAlpha(0.15f));
        g.fillRoundedRectangle(bpmLCD, 4.0f);
    }
    
    g.setColour(juce::Colour(0xff52525b));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(7.5f));
    g.drawText("BPM", (int)bpmLCD.getX() + 4, (int)bpmLCD.getY() + 1, 22, 8, juce::Justification::centredLeft);
    
    g.setColour(isDraggingBPM_ ? Theme::orange2 : Theme::orange1);
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(14.0f).withStyle("Bold"));
    g.drawText(juce::String((int)bpm_), (int)bpmLCD.getX() + 10, (int)bpmLCD.getY() + 5, (int)bpmLCD.getWidth() - 10, 15, juce::Justification::centred);
    
    // (BPM +/- buttons removed — drag the BPM display or use the scroll wheel
    //  on it to change tempo.)

    x = (int)bpmBounds_.getRight() + 6;
    
    // ═══════════════════════════════════
    // Time display panel (LCD inset)
    // ═══════════════════════════════════
    auto timeBounds = juce::Rectangle<float>((float)x, (float)cy - 14, 134, 28);
    drawPanel(timeBounds, 8.0f);
    auto timeLCD = timeBounds.reduced(4.0f, 4.0f);
    drawInset(timeLCD, 4.0f);
    g.setColour(Theme::orange1);
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(15.0f).withStyle("Bold"));
    g.drawText(formatElapsed(), timeLCD.toNearestInt(), juce::Justification::centred);
    
    x = (int)timeBounds.getRight() + 6;
    
    // ═══════════════════════════════════
    // Pattern selector
    // ═══════════════════════════════════
    patSelRect_ = juce::Rectangle<float>((float)x, (float)cy - 14, 100, 28);
    drawPanel(patSelRect_, 8.0f);
    g.setColour(juce::Colour(0xffe4e4e7));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
    juce::String patName = (currentPattern_ >= 0 && currentPattern_ < patterns_.size())
                            ? patterns_[currentPattern_] : juce::String("Pattern 1");
    g.drawText(patName, (int)patSelRect_.getX() + 8, (int)patSelRect_.getY(),
               70, 28, juce::Justification::centredLeft);
    juce::Path arrow;
    float ax = patSelRect_.getRight() - 13.0f;
    float ay = patSelRect_.getCentreY();
    arrow.addTriangle(ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour(juce::Colour(0xff52525b));
    g.fillPath(arrow);
    
    patPlusRect_ = juce::Rectangle<float>(patSelRect_.getRight() + 4, patSelRect_.getY(), 28, 28);
    drawPanel(patPlusRect_, 8.0f);
    g.setColour(Theme::orange1);
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(16.0f).withStyle("Bold"));
    g.drawText("+", patPlusRect_.toNearestInt(), juce::Justification::centred);
    
    x = (int)patPlusRect_.getRight() + 10;
    
    // ═══════════════════════════════════════════════════════════
    // MIXER / PLAYLIST toggle group  (PIANO moved to PIANO ROLL button in bottom dock)
    // ═══════════════════════════════════════════════════════════
    pianoBtnRect_       = juce::Rectangle<float>((float)x, (float)cy - 14, 0, 28); // removed
    // PLAYLIST first, MIXER second
    playlistBtnRect_    = juce::Rectangle<float>((float)x,                     (float)cy - 14, 64, 28);
    mixerBtnRect_       = juce::Rectangle<float>(playlistBtnRect_.getRight(),   (float)cy - 14, 58, 28);
    orgChartBtnRect_    = juce::Rectangle<float>(mixerBtnRect_.getRight(),      (float)cy - 14, 66, 28);

    auto toggleGroup = juce::Rectangle<float>(playlistBtnRect_.getX(), playlistBtnRect_.getY(),
                                               playlistBtnRect_.getWidth() + mixerBtnRect_.getWidth()
                                               + orgChartBtnRect_.getWidth(),
                                               playlistBtnRect_.getHeight());
    drawPanel(toggleGroup, 8.0f);

    g.setColour(juce::Colour(0xff3f3f46));
    g.drawVerticalLine((int)playlistBtnRect_.getRight(), toggleGroup.getY() + 4, toggleGroup.getBottom() - 4);
    g.drawVerticalLine((int)mixerBtnRect_.getRight(), toggleGroup.getY() + 4, toggleGroup.getBottom() - 4);

    auto lerpColour = [](juce::Colour a, juce::Colour b, float t) {
        return juce::Colour::fromRGBA(
            (juce::uint8)(a.getRed()   + (b.getRed()   - a.getRed())   * t),
            (juce::uint8)(a.getGreen() + (b.getGreen() - a.getGreen()) * t),
            (juce::uint8)(a.getBlue()  + (b.getBlue()  - a.getBlue())  * t),
            (juce::uint8)(a.getAlpha() + (b.getAlpha() - a.getAlpha()) * t)
        );
    };

    float mixerAlpha       = (selectedView_ == 1) ? animationProgress_ : (1.0f - animationProgress_);
    mixerAlpha             = juce::jlimit(0.0f, 1.0f, mixerAlpha);
    float playlistAlpha    = (selectedView_ == 2) ? animationProgress_ : (1.0f - animationProgress_);
    playlistAlpha          = juce::jlimit(0.0f, 1.0f, playlistAlpha);
    float orgChartAlpha    = (selectedView_ == 4) ? animationProgress_ : (1.0f - animationProgress_);
    orgChartAlpha          = juce::jlimit(0.0f, 1.0f, orgChartAlpha);

    // ── PLAYLIST (leftmost — left-rounded corners) ──
    if (playlistAlpha > 0.0f)
    {
        g.setColour(Theme::orange1.withAlpha(playlistAlpha * 0.22f));
        juce::Path pp;
        pp.addRoundedRectangle(playlistBtnRect_.getX() + 1, playlistBtnRect_.getY() + 1,
                               playlistBtnRect_.getWidth() - 1, playlistBtnRect_.getHeight() - 2,
                               7.0f, 7.0f, true, false, true, false);
        g.fillPath(pp);
        if (playlistAlpha > 0.5f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f * playlistAlpha));
            g.drawHorizontalLine((int)playlistBtnRect_.getY() + 2, playlistBtnRect_.getX() + 6, playlistBtnRect_.getRight() - 4);
        }
    }
    g.setColour(lerpColour(juce::Colour(0xff71717a), juce::Colours::white, playlistAlpha));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f));
    g.drawText("PLAYLIST", playlistBtnRect_.toNearestInt(), juce::Justification::centred);

    // ── MIXER (middle — no rounded corners) ──
    if (mixerAlpha > 0.0f)
    {
        g.setColour(Theme::orange1.withAlpha(mixerAlpha * 0.22f));
        g.fillRect(mixerBtnRect_.getX(), mixerBtnRect_.getY() + 1,
                   mixerBtnRect_.getWidth(), mixerBtnRect_.getHeight() - 2);
        if (mixerAlpha > 0.5f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f * mixerAlpha));
            g.drawHorizontalLine((int)mixerBtnRect_.getY() + 2, mixerBtnRect_.getX() + 4, mixerBtnRect_.getRight() - 4);
        }
    }
    g.setColour(lerpColour(juce::Colour(0xff71717a), juce::Colours::white, mixerAlpha));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f));
    g.drawText("MIXER", mixerBtnRect_.toNearestInt(), juce::Justification::centred);

    // ── AGENTS (rightmost — right-rounded corners) ──
    if (orgChartAlpha > 0.0f)
    {
        g.setColour(Theme::orange1.withAlpha(orgChartAlpha * 0.22f));
        juce::Path ap;
        ap.addRoundedRectangle(orgChartBtnRect_.getX(), orgChartBtnRect_.getY() + 1,
                               orgChartBtnRect_.getWidth() - 1, orgChartBtnRect_.getHeight() - 2,
                               7.0f, 7.0f, false, true, false, true);
        g.fillPath(ap);
        if (orgChartAlpha > 0.5f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f * orgChartAlpha));
            g.drawHorizontalLine((int)orgChartBtnRect_.getY() + 2, orgChartBtnRect_.getX() + 4, orgChartBtnRect_.getRight() - 6);
        }
    }
    g.setColour(lerpColour(juce::Colour(0xff71717a), juce::Colours::white, orgChartAlpha));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.0f));
    g.drawText("AGENTS", orgChartBtnRect_.toNearestInt(), juce::Justification::centred);

    // CONSISTENCY button is now in the title bar — nothing to draw here.
    consistencyBtnRect_ = juce::Rectangle<float>(orgChartBtnRect_.getRight(), pianoBtnRect_.getY(), 0, 28);
    
    // ═══════════════════════════════════
    // Right side: icon buttons — SAVE / OPEN / UPLOAD / NEW PROJECT
    // ═══════════════════════════════════
    int rx = w - 10;
    constexpr float BTN_SZ = 28.0f;
    constexpr float BTN_GAP = 6.0f;

    auto drawIconBtn = [&](juce::Rectangle<float> r, juce::Path icon, bool accent) {
        drawPanel(r, 6.0f);
        if (accent)
        {
            g.setColour(Theme::orange2.withAlpha(0.22f));
            g.fillRoundedRectangle(r.reduced(1.0f), 6.0f);
        }
        const juce::Colour stroke = accent ? Theme::orange1 : juce::Colour(0xffd4d4d8);
        g.setColour(stroke);
        icon.applyTransform(icon.getTransformToScaleToFit(r.reduced(7.0f), true));
        g.strokePath(icon, juce::PathStrokeType(1.6f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    // Build the four icons in unit coordinates (0..24).
    auto saveIcon = [] { // floppy disk
        juce::Path p;
        p.startNewSubPath(3, 3); p.lineTo(17, 3); p.lineTo(21, 7); p.lineTo(21, 21);
        p.lineTo(3, 21); p.closeSubPath();
        // Inner label rectangle
        p.startNewSubPath(7, 14); p.lineTo(17, 14); p.lineTo(17, 21); p.lineTo(7, 21); p.closeSubPath();
        // Top metal slider
        p.startNewSubPath(7, 3); p.lineTo(7, 9); p.lineTo(15, 9); p.lineTo(15, 3);
        return p;
    }();

    auto openIcon = [] { // folder
        juce::Path p;
        p.startNewSubPath(3, 7); p.lineTo(9, 7); p.lineTo(11, 9); p.lineTo(21, 9);
        p.lineTo(21, 19); p.lineTo(3, 19); p.closeSubPath();
        return p;
    }();

    auto uploadIcon = [] { // arrow up into tray — clear upload affordance
        juce::Path p;
        p.startNewSubPath(12.0f, 5.0f);  p.lineTo(12.0f, 14.0f);
        p.startNewSubPath(7.0f, 10.0f);  p.lineTo(12.0f, 5.0f); p.lineTo(17.0f, 10.0f);
        p.startNewSubPath(5.0f, 16.0f);  p.lineTo(5.0f, 20.0f); p.lineTo(19.0f, 20.0f); p.lineTo(19.0f, 16.0f);
        return p;
    }();

    auto newProjectIcon = [] { // document with plus
        juce::Path p;
        p.startNewSubPath(5, 3); p.lineTo(15, 3); p.lineTo(20, 8); p.lineTo(20, 21);
        p.lineTo(5, 21); p.closeSubPath();
        p.startNewSubPath(15, 3); p.lineTo(15, 8); p.lineTo(20, 8);
        p.startNewSubPath(12, 11); p.lineTo(12, 17);
        p.startNewSubPath(9, 14);  p.lineTo(15, 14);
        return p;
    }();

    auto newProjectBtn  = juce::Rectangle<float>((float)rx - BTN_SZ,                              (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);
    auto cloudBtn = juce::Rectangle<float>(newProjectBtn.getX() - BTN_GAP - BTN_SZ,               (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);
    auto openBtn  = juce::Rectangle<float>(cloudBtn.getX()  - BTN_GAP - BTN_SZ,               (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);
    auto saveBtn  = juce::Rectangle<float>(openBtn.getX() - BTN_GAP - BTN_SZ,               (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);

    drawIconBtn(newProjectBtn,  newProjectIcon, true);
    drawIconBtn(cloudBtn, uploadIcon, true);
    drawIconBtn(openBtn, openIcon,   false);
    drawIconBtn(saveBtn, saveIcon,   false);

    // ── Beats Studio Button ──
    float beatsStudioBtnW = 104.0f;
    auto beatsStudioBtn = juce::Rectangle<float>(saveBtn.getX() - 12.0f - beatsStudioBtnW, (float)cy - BTN_SZ / 2, beatsStudioBtnW, BTN_SZ);
    
    // Draw the button panel
    drawPanel(beatsStudioBtn, 6.0f);
    
    // Draw LED indicator
    auto ledRect = juce::Rectangle<float>(beatsStudioBtn.getX() + 8.0f, beatsStudioBtn.getCentreY() - 3.0f, 6.0f, 6.0f);
    juce::Colour ledColor = isBeatsAppRunning_ ? juce::Colour(0xff00d4ff) : juce::Colour(0xff52525b);
    Theme::drawGlowLED(g, ledRect, ledColor, isBeatsAppRunning_);
    
    // Draw text "BEATS STUDIO"
    g.setColour(isBeatsAppRunning_ ? juce::Colours::white : juce::Colour(0xffa1a1aa));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f).withStyle("Bold"));
    g.drawText("BEATS STUDIO",
               (int)beatsStudioBtn.getX() + 18, (int)beatsStudioBtn.getY(),
               (int)beatsStudioBtn.getWidth() - 20, (int)beatsStudioBtn.getHeight(),
               juce::Justification::centredLeft);

    // ── Create Video button (left of BEATS STUDIO) ──
    const float createVideoBtnW = 132.0f;
    auto cvBtn = juce::Rectangle<float>(beatsStudioBtn.getX() - 12.0f - createVideoBtnW,
                                        (float)cy - BTN_SZ / 2, createVideoBtnW, BTN_SZ);
    createVideoBtnRect_ = cvBtn;
    drawCreateVideoButton(g, cvBtn);
}

void TransportBar::drawCreateVideoButton(juce::Graphics& g, juce::Rectangle<float> r)
{
    auto bg = r;
    g.setColour(juce::Colour(0xff141416));
    g.fillRoundedRectangle(bg, 6.0f);

    if (videoState_ == VideoRenderState::Rendering)
    {
        // Progress fill across the button.
        const float frac = juce::jlimit(0.0f, 1.0f, videoProgress_ / 100.0f);
        auto fill = bg.withWidth(bg.getWidth() * frac);
        g.setColour(Theme::orange1.withAlpha(0.85f));
        g.fillRoundedRectangle(fill, 6.0f);
        g.setColour(juce::Colour(0xff3a3a3e));
        g.drawRoundedRectangle(bg, 6.0f, 1.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f).withStyle("Bold"));
        g.drawText("RENDERING " + juce::String(videoProgress_) + "%",
                   bg.toNearestInt(), juce::Justification::centred);
    }
    else if (videoState_ == VideoRenderState::Ready)
    {
        g.setColour(juce::Colour(0xff14532d));            // green-ish "ready"
        g.fillRoundedRectangle(bg, 6.0f);
        g.setColour(juce::Colour(0xff22c55e));
        g.drawRoundedRectangle(bg, 6.0f, 1.0f);

        // Play triangle
        auto tri = juce::Rectangle<float>(bg.getX() + 12.0f, bg.getCentreY() - 5.0f, 9.0f, 10.0f);
        juce::Path p;
        p.addTriangle(tri.getX(), tri.getY(), tri.getX(), tri.getBottom(), tri.getRight(), tri.getCentreY());
        g.setColour(juce::Colour(0xff22c55e));
        g.fillPath(p);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f).withStyle("Bold"));
        g.drawText("OPEN VIDEO", (int)bg.getX() + 26, (int)bg.getY(),
                   (int)bg.getWidth() - 28, (int)bg.getHeight(), juce::Justification::centredLeft);
    }
    else // Idle
    {
        g.setColour(juce::Colour(0xff3a3a3e));
        g.drawRoundedRectangle(bg, 6.0f, 1.0f);

        // Film/clapper glyph
        g.setColour(Theme::orange1);
        auto ic = juce::Rectangle<float>(bg.getX() + 11.0f, bg.getCentreY() - 5.0f, 12.0f, 10.0f);
        g.drawRoundedRectangle(ic, 1.5f, 1.2f);
        g.drawLine(ic.getX(), ic.getY() + 3.0f, ic.getRight(), ic.getY() + 3.0f, 1.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f).withStyle("Bold"));
        g.drawText("CREATE VIDEO", (int)bg.getX() + 28, (int)bg.getY(),
                   (int)bg.getWidth() - 30, (int)bg.getHeight(), juce::Justification::centredLeft);
    }
}

void TransportBar::resized()
{
    int h = getHeight();
    int cy = h / 2;
    int btnSize = 20;
    int y = cy - btnSize / 2;
    
    // Transport buttons inside pill: PAT(48) + gap(6) = 54, pill w=100
    // Center 3×22 + 2×6 = 78 inside 100 → start at 54 + 11 = 65
    int tpPillX = 10 + 68 + 6;
    int btnGap = 6;
    int tpPillW = 86;
    int btnTotal = 3 * btnSize + 2 * btnGap;
    int bx = tpPillX + (tpPillW - btnTotal) / 2;
    playBtn_->setBounds(bx, y, btnSize, btnSize); bx += btnSize + btnGap;
    stopBtn_->setBounds(bx, y, btnSize, btnSize); bx += btnSize + btnGap;
    recordBtn_->setBounds(bx, y, btnSize, btnSize);
    
    // BPM slider hidden (interaction via mouseDown)
    bpmLabel_.setBounds(0, 0, 0, 0);
    bpmSlider_.setBounds(0, 0, 0, 0);
    
    // Time label hidden (drawn in paint)
    timeLabel_.setBounds(0, 0, 0, 0);
    
    // Update button rects for hit detection
    updateButtonRects();
}

void TransportBar::updateButtonRects()
{
    int w = getWidth();
    int h = getHeight();
    int cy = h / 2;
    
    playbackModeRect_ = juce::Rectangle<float>(10.0f, (float)cy - 14, 68.0f, 28.0f);

    // Match paint() layout: MODE(74)+6 + TP(100)+6 + BPM(110)+6 + TIME(134)+6 + PAT_SEL(100)+4 + PAT_PLUS(28)+10 = x
    int x = 10 + 68 + 6 + 86 + 6 + 84 + 6 + 134 + 6 + 100 + 4 + 28 + 10;
    // PIANO removed. PLAYLIST first, MIXER second.
    pianoBtnRect_    = juce::Rectangle<float>((float)x, (float)cy - 14, 0, 28);
    playlistBtnRect_ = juce::Rectangle<float>((float)x,                    (float)cy - 14, 64, 28);
    mixerBtnRect_    = juce::Rectangle<float>(playlistBtnRect_.getRight(),  (float)cy - 14, 58, 28);
    orgChartBtnRect_ = juce::Rectangle<float>(mixerBtnRect_.getRight(),     (float)cy - 14, 66, 28);
    consistencyBtnRect_ = juce::Rectangle<float>(orgChartBtnRect_.getRight(), (float)cy - 14, 0, 28);
    settingsBtnRect_ = juce::Rectangle<float>(orgChartBtnRect_.getRight(), (float)cy - 14, 0.0f, 0.0f);

    // Calculate beatsStudioBtnRect_ to match paint()
    int rx = w - 10;
    constexpr float BTN_SZ = 28.0f;
    constexpr float BTN_GAP = 6.0f;
    auto newProjectR = juce::Rectangle<float>((float)rx - BTN_SZ,                          (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);
    auto cloudR      = juce::Rectangle<float>(newProjectR.getX() - BTN_GAP - BTN_SZ,       (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);
    auto openR       = juce::Rectangle<float>(cloudR.getX()  - BTN_GAP - BTN_SZ,           (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);
    auto saveR       = juce::Rectangle<float>(openR.getX() - BTN_GAP - BTN_SZ,             (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);

    float beatsStudioBtnW = 104.0f;
    beatsStudioBtnRect_ = juce::Rectangle<float>(saveR.getX() - 12.0f - beatsStudioBtnW, (float)cy - BTN_SZ / 2, beatsStudioBtnW, BTN_SZ);

    const float createVideoBtnW = 132.0f;
    createVideoBtnRect_ = juce::Rectangle<float>(beatsStudioBtnRect_.getX() - 12.0f - createVideoBtnW,
                                                 (float)cy - BTN_SZ / 2, createVideoBtnW, BTN_SZ);
}

void TransportBar::mouseDown(const juce::MouseEvent& e)
{
    int w = getWidth();
    int h = getHeight();
    int cy = h / 2;
    
    // BPM panel — drag vertically to change tempo, scroll-wheel to nudge.
    if (playbackModeRect_.contains(e.getPosition().toFloat()))
    {
        const bool switchingToPlaylist = playbackMode_ == PlaybackMode::Rack;
        setPlaybackMode(switchingToPlaylist ? PlaybackMode::Playlist : PlaybackMode::Rack);
        if (switchingToPlaylist)
        {
            if (selectedView_ != 2)
            {
                previousView_ = selectedView_;
                selectedView_ = 2;
                animationProgress_ = 0.0f;
                startTimer(16);
            }
            if (onPlaylistToggle) onPlaylistToggle();
        }
        return;
    }

    if (bpmBounds_.contains((float)e.x, (float)e.y))
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem(1, "Find loops in BPM range...");
            const auto anchor = localAreaToGlobal(bpmBounds_.toNearestInt());
            m.showMenuAsync(juce::PopupMenu::Options{}
                                .withTargetScreenArea(anchor)
                                .withMinimumWidth(220)
                                .withStandardItemHeight(28),
                [this, anchor](int result) {
                    if (result == 1 && onFindLoopsInBpmRange)
                        onFindLoopsInBpmRange(bpm_, anchor);
                });
            return;
        }

        isDraggingBPM_ = true;
        dragStartY_    = e.y;
        dragStartBPM_  = bpm_;
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        repaint();
        return;
    }
    
    
    // MIXER button
    if (mixerBtnRect_.contains(e.getPosition().toFloat()))
    {
        if (selectedView_ != 1)
        {
            previousView_ = selectedView_;
            selectedView_ = 1;
            animationProgress_ = 0.0f;
            startTimer(16); // 60 FPS
        }
        if (onMixerToggle) onMixerToggle();
        return;
    }
    if (orgChartBtnRect_.contains(e.getPosition().toFloat()))
    {
        if (selectedView_ != 4)
        {
            previousView_ = selectedView_;
            selectedView_ = 4;
            animationProgress_ = 0.0f;
            startTimer(16);
        }
        if (onOrgChartToggle) onOrgChartToggle();
        return;
    }
    // Pattern selector dropdown → popup of patterns + "Add new pattern"
    if (patSelRect_.contains(e.getPosition().toFloat()))
    {
        juce::PopupMenu m;
        for (int i = 0; i < patterns_.size(); ++i)
            m.addItem(i + 1, patterns_[i], true, i == currentPattern_);
        m.addSeparator();
        m.addItem(9999, "Add new pattern...");

        // Anchor the menu directly under the dropdown rect (in screen coords),
        // not the whole TransportBar.
        auto screenArea = localAreaToGlobal(patSelRect_.toNearestInt());
        m.showMenuAsync(juce::PopupMenu::Options{}
                            .withTargetScreenArea(screenArea)
                            .withMinimumWidth((int)patSelRect_.getWidth() + 28)
                            .withStandardItemHeight(28),
            [this](int r) {
                if (r <= 0) return;
                if (r == 9999) { addPattern(); return; }
                setCurrentPattern(r - 1);
            });
        return;
    }
    // Pattern + button → add new pattern immediately
    if (patPlusRect_.contains(e.getPosition().toFloat()))
    {
        addPattern();
        return;
    }
    // PLAYLIST button
    if (playlistBtnRect_.contains(e.getPosition().toFloat()))
    {
        if (selectedView_ != 2)
        {
            previousView_ = selectedView_;
            selectedView_ = 2;
            animationProgress_ = 0.0f;
            startTimer(16); // 60 FPS
        }
        if (onPlaylistToggle) onPlaylistToggle();
        return;
    }
    // CONSISTENCY button
    if (consistencyBtnRect_.contains(e.getPosition().toFloat()))
    {
        if (selectedView_ != 3)
        {
            previousView_ = selectedView_;
            selectedView_ = 3;
            animationProgress_ = 0.0f;
            startTimer(16); // 60 FPS
        }
        if (onConsistencyToggle) onConsistencyToggle();
        return;
    }
    if (settingsBtnRect_.contains(e.getPosition().toFloat()))
    {
        if (onSettingsClicked) onSettingsClicked();
        return;
    }

    // Right side icon buttons: NEW PROJECT, CLOUD UPLOAD, OPEN, SAVE — all 28x28
    int rx = w - 10;
    constexpr float BTN_SZ = 28.0f;
    constexpr float BTN_GAP = 6.0f;
    auto newProjectR = juce::Rectangle<float>((float)rx - BTN_SZ,                          (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);
    auto cloudR      = juce::Rectangle<float>(newProjectR.getX() - BTN_GAP - BTN_SZ,       (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);
    auto openR       = juce::Rectangle<float>(cloudR.getX()  - BTN_GAP - BTN_SZ,           (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);
    auto saveR       = juce::Rectangle<float>(openR.getX() - BTN_GAP - BTN_SZ,             (float)cy - BTN_SZ / 2, BTN_SZ, BTN_SZ);

    if (createVideoBtnRect_.contains(e.getPosition().toFloat()))
    {
        if (videoState_ == VideoRenderState::Ready)
        {
            // Open / play the finished video.
            if (videoOutputPath_.isNotEmpty())
                juce::File(videoOutputPath_).startAsProcess();
        }
        else if (videoState_ == VideoRenderState::Idle)
        {
            startVideoRender();
        }
        // While Rendering: ignore clicks.
        return;
    }

    if (beatsStudioBtnRect_.contains(e.getPosition().toFloat()))
    {
        // Right-click → "Render Video for this Beat" (drives Beats Studio's
        // Create tab to build a YouTube-ready video from the current beat).
        if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu rm;
            rm.addItem(1, juce::String::fromUTF8("\xF0\x9F\x8E\xAC  Render Video for this Beat"),
                       isBeatsAppRunning_, false);
            if (!isBeatsAppRunning_)
                rm.addItem(2, "(Start Beats Studio first)", false, false);

            auto screenArea = localAreaToGlobal(beatsStudioBtnRect_.toNearestInt());
            rm.showMenuAsync(juce::PopupMenu::Options{}
                                 .withTargetScreenArea(screenArea)
                                 .withMinimumWidth((int)beatsStudioBtnRect_.getWidth() + 60)
                                 .withStandardItemHeight(28),
                [this](int r) {
                    if (r == 1) startVideoRender();
                });
            return;
        }

        if (!isBeatsAppRunning_)
        {
            // Launch the Beats Management Studio app!
            juce::File("F:\\PlaygroundTest\\BeatsManagementVersion2\\start-app.bat").startAsProcess();
            repaint();
        }
        else
        {
            // App is running! Show the dropdown popup menu
            juce::PopupMenu m;
            m.addItem(1, "Focus Beats Studio");
            m.addSeparator();
            m.addItem(9, juce::String::fromUTF8("\xF0\x9F\x8E\xAC  Render Video for this Beat"));
            m.addSeparator();
            m.addItem(2, "Beats Library");
            m.addItem(3, "Sales Tracker (Money)");
            m.addItem(4, "Video Creator (AutoVid)");
            m.addItem(5, "YouTube Uploader");
            m.addItem(6, "Customer Manager");
            m.addItem(7, "Progress Tracker");
            m.addSeparator();
            m.addItem(8, "Sync DAW BPM (" + juce::String((int)bpm_) + ")");

            auto screenArea = localAreaToGlobal(beatsStudioBtnRect_.toNearestInt());
            m.showMenuAsync(juce::PopupMenu::Options{}
                                .withTargetScreenArea(screenArea)
                                .withMinimumWidth((int)beatsStudioBtnRect_.getWidth() + 40)
                                .withStandardItemHeight(28),
                [this](int r) {
                    if (r <= 0) return;
                    if (r == 1) sendBridgeCommand(R"({"action":"focus"})");
                    else if (r == 2) sendBridgeCommand(R"({"action":"navigate","tab":"beats"})");
                    else if (r == 3) sendBridgeCommand(R"({"action":"navigate","tab":"money"})");
                    else if (r == 4) sendBridgeCommand(R"({"action":"navigate","tab":"autovid"})");
                    else if (r == 5) sendBridgeCommand(R"({"action":"navigate","tab":"youtube"})");
                    else if (r == 6) sendBridgeCommand(R"({"action":"navigate","tab":"customers"})");
                    else if (r == 7) sendBridgeCommand(R"({"action":"navigate","tab":"progress"})");
                    else if (r == 8) {
                        juce::String bpmCmd = R"({"action":"syncBpm","bpm":)" + juce::String(bpm_) + "}";
                        sendBridgeCommand(bpmCmd);
                    }
                    else if (r == 9) {
                        startVideoRender();
                    }
                });
        }
        return;
    }

    if (newProjectR.contains((float)e.x, (float)e.y)) { if (onNewProject) onNewProject(); return; }
    if (cloudR.contains((float)e.x, (float)e.y))      { if (onUploadToCloud) onUploadToCloud(); return; }
    if (openR.contains((float)e.x, (float)e.y)) { if (onOpen) onOpen(); return; }
    if (saveR.contains((float)e.x, (float)e.y)) { if (onSave) onSave(); return; }
}

void TransportBar::setCurrentPattern(int idx)
{
    if (idx < 0 || idx >= patterns_.size() || idx == currentPattern_) return;
    currentPattern_ = idx;
    if (onPatternSelected) onPatternSelected(idx);
    repaint();
}

int TransportBar::addPattern(const juce::String& name)
{
    juce::String n = name;
    if (n.isEmpty())
        n = "Pattern " + juce::String(patterns_.size() + 1);
    patterns_.add(n);
    int idx = patterns_.size() - 1;
    currentPattern_ = idx;
    if (onPatternAdded)    onPatternAdded(n);
    if (onPatternSelected) onPatternSelected(idx);
    repaint();
    return idx;
}

void TransportBar::setBPM(double bpm)
{
    double clamped = juce::jlimit(20.0, 999.0, bpm);
    if (clamped == bpm_) return;
    bpm_ = clamped;
    bpmSlider_.setValue(bpm_, juce::dontSendNotification);
    if (onBPMChanged) onBPMChanged(bpm_);
    repaint();
}

void TransportBar::setPlaybackMode(PlaybackMode mode)
{
    if (playbackMode_ == mode) return;
    playbackMode_ = mode;
    if (onPlaybackModeChanged) onPlaybackModeChanged(playbackMode_);
    repaint();
}

// ─── Project I/O ─────────────────────────────────────────────────────
juce::var TransportBar::toJson() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("bpm", bpm_);
    juce::Array<juce::var> arr;
    for (const auto& p : patterns_) arr.add(p);
    obj->setProperty("patterns",       arr);
    obj->setProperty("currentPattern", currentPattern_);
    obj->setProperty("playbackMode",   playbackMode_ == PlaybackMode::Playlist ? "playlist" : "rack");
    return juce::var(obj);
}

void TransportBar::fromJson(const juce::var& v)
{
    if (!v.isObject()) return;
    setBPM((double)v.getProperty("bpm", 130.0));

    patterns_.clear();
    if (auto* arr = v.getProperty("patterns", juce::var()).getArray())
        for (auto& pv : *arr) patterns_.add(pv.toString());
    if (patterns_.isEmpty()) patterns_.add("Pattern 1");

    currentPattern_ = juce::jlimit(0, patterns_.size() - 1,
                                   (int)v.getProperty("currentPattern", 0));
    playbackMode_ = v.getProperty("playbackMode", "rack").toString().equalsIgnoreCase("playlist")
                    ? PlaybackMode::Playlist : PlaybackMode::Rack;
    if (onPlaybackModeChanged) onPlaybackModeChanged(playbackMode_);
    if (onPatternSelected) onPatternSelected(currentPattern_);
    repaint();
}

void TransportBar::setPlaybackStep(int absoluteStep, bool playing)
{
    // Each step is a 16th note → seconds = step * (60/bpm) / 4 = step * 15 / bpm.
    const double bpm = bpm_ > 0.0 ? bpm_ : 130.0;
    playElapsedSeconds_ = juce::jmax(0, absoluteStep) * 15.0 / bpm;
    playStartMs_        = juce::Time::getMillisecondCounterHiRes();

    if (playing && !isPlaying_)
        isPlaying_ = true;

    repaint();
}

juce::String TransportBar::formatElapsed() const
{
    // The clock is anchored to the real playback step (see setPlaybackStep);
    // while playing we interpolate with the wall clock for a smooth readout.
    double secs = playElapsedSeconds_;
    if (isPlaying_)
        secs += (juce::Time::getMillisecondCounterHiRes() - playStartMs_) / 1000.0;

    if (secs < 0.0) secs = 0.0;
    const int totalCs = (int)(secs * 100.0 + 0.5);   // centiseconds
    const int minutes = totalCs / 6000;
    const int seconds = (totalCs / 100) % 60;
    const int centis  = totalCs % 100;
    return juce::String::formatted("%d:%02d:%02d", minutes, seconds, centis);
}

void TransportBar::togglePlay()
{
    isPlaying_ = !isPlaying_;
    if (auto* btn = dynamic_cast<TransportButton*>(playBtn_.get()))
        btn->setActive(isPlaying_);

    if (isPlaying_)
    {
        // Resume the clock from where it was (pause/resume keeps elapsed time).
        playStartMs_ = juce::Time::getMillisecondCounterHiRes();
        startTimer(16); // keep the LCD ticking
    }
    else
    {
        // Pause: bank the elapsed time so the readout holds its value.
        playElapsedSeconds_ += (juce::Time::getMillisecondCounterHiRes() - playStartMs_) / 1000.0;
    }

    if (onPlayStateChanged) onPlayStateChanged(isPlaying_);
    repaint();
}

void TransportBar::stop()
{
    isPlaying_ = false;
    isRecording_ = false;
    // Full stop resets the clock back to 0:00:00.
    playElapsedSeconds_ = 0.0;
    playStartMs_ = juce::Time::getMillisecondCounterHiRes();
    if (auto* btn = dynamic_cast<TransportButton*>(playBtn_.get()))
        btn->setActive(false);
    if (auto* btn = dynamic_cast<TransportButton*>(recordBtn_.get()))
        btn->setActive(false);
    if (onPlayStateChanged) onPlayStateChanged(false);
    repaint();
}

void TransportBar::toggleRecord()
{
    isRecording_ = !isRecording_;
    if (auto* btn = dynamic_cast<TransportButton*>(recordBtn_.get()))
        btn->setActive(isRecording_);
}



void TransportBar::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingBPM_)
    {
        // Drag up = increase BPM, drag down = decrease BPM
        int deltaY = dragStartY_ - e.y; // Inverted: up is positive
        double bpmChange = deltaY * 0.5; // 0.5 BPM per pixel
        
        double newBPM = dragStartBPM_ + bpmChange;
        newBPM = juce::jlimit(20.0, 999.0, newBPM); // Clamp to valid range
        
        if (newBPM != bpm_)
        {
            bpm_ = newBPM;
            bpmSlider_.setValue(bpm_, juce::dontSendNotification);
            if (onBPMChanged) onBPMChanged(bpm_);
            repaint();
        }
    }
}

void TransportBar::mouseUp(const juce::MouseEvent& e)
{
    if (isDraggingBPM_)
    {
        isDraggingBPM_ = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void TransportBar::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Check if mouse is over BPM display
    if (bpmBounds_.contains((float)e.x, (float)e.y))
    {
        double delta = wheel.deltaY * 5.0; // 5 BPM per wheel notch
        double newBPM = bpm_ + delta;
        newBPM = juce::jlimit(20.0, 999.0, newBPM);
        
        if (newBPM != bpm_)
        {
            bpm_ = newBPM;
            bpmSlider_.setValue(bpm_, juce::dontSendNotification);
            if (onBPMChanged) onBPMChanged(bpm_);
            repaint();
        }
    }
}

void TransportBar::timerCallback()
{
    if (animationProgress_ < 1.0f)
        animationProgress_ += 0.05f; // Animation speed

    if (animationProgress_ >= 1.0f)
    {
        animationProgress_ = 1.0f;
        // Keep ticking while playing (the LCD clock needs updates); only stop
        // the timer once both the view animation and playback are idle.
        if (!isPlaying_)
            stopTimer();
    }

    repaint();
}

void TransportBar::checkBeatsAppStatus()
{
    juce::Thread::launch([this]() {
        juce::StreamingSocket socket;
        bool running = socket.connect("127.0.0.1", 9003, 50); // 50ms timeout on localhost
        if (running != isBeatsAppRunning_)
        {
            isBeatsAppRunning_ = running;
            juce::MessageManager::callAsync([this]() { repaint(); });
        }
    });
}

void TransportBar::startVideoRender()
{
    if (videoState_ == VideoRenderState::Rendering)
        return;
    videoState_ = VideoRenderState::Rendering;
    videoProgress_ = 0;
    videoOutputPath_.clear();
    repaint();
    if (onRenderVideoForBeat) onRenderVideoForBeat();
}

void TransportBar::setVideoRenderProgress(int pct)
{
    videoState_ = VideoRenderState::Rendering;
    videoProgress_ = juce::jlimit(0, 100, pct);
    repaint();
}

void TransportBar::setVideoRenderDone(const juce::String& path)
{
    videoState_ = VideoRenderState::Ready;
    videoProgress_ = 100;
    videoOutputPath_ = path;
    repaint();
}

void TransportBar::setVideoRenderIdle()
{
    videoState_ = VideoRenderState::Idle;
    videoProgress_ = 0;
    videoOutputPath_.clear();
    repaint();
}

void TransportBar::sendBridgeCommand(const juce::String& jsonCommand)
{
    juce::Thread::launch([jsonCommand]() {
        juce::StreamingSocket socket;
        if (socket.connect("127.0.0.1", 9003, 500)) // 500ms timeout
        {
            socket.write(jsonCommand.toRawUTF8(), (int)jsonCommand.getNumBytesAsUTF8());
        }
    });
}
