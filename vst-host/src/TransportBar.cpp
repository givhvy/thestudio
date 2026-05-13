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
            juce::Path p;
            p.addTriangle(cx - 5, cy - 6, cx - 5, cy + 6, cx + 6, cy);
            g.setColour(isActive ? juce::Colour(0xff10b981) : juce::Colour(0xff10b981).withAlpha(0.75f));
            g.fillPath(p);
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
    auto patBounds = juce::Rectangle<float>((float)x, (float)cy - 14, 48, 28);
    drawPanel(patBounds, 14.0f);
    auto patDot = juce::Rectangle<float>(patBounds.getX() + 7, patBounds.getCentreY() - 3, 6.0f, 6.0f);
    Theme::drawGlowLED(g, patDot, Theme::orange2, true);
    g.setColour(juce::Colour(0xffa1a1aa));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f));
    g.drawText("PAT", (int)patBounds.getX() + 16, (int)patBounds.getY(), 30, 28, juce::Justification::centredLeft);
    
    x = (int)patBounds.getRight() + 6;
    
    // ═══════════════════════════════════
    // Transport buttons panel
    // ═══════════════════════════════════
    auto tpBounds = juce::Rectangle<float>((float)x, (float)cy - 15, 100, 30);
    drawPanel(tpBounds, 15.0f);
    
    x = (int)tpBounds.getRight() + 6;
    
    // ═══════════════════════════════════
    // BPM panel
    // ═══════════════════════════════════
    bpmBounds_ = juce::Rectangle<float>((float)x, (float)cy - 14, 110, 28);
    drawPanel(bpmBounds_, 8.0f);
    
    auto bpmLCD = juce::Rectangle<float>(bpmBounds_.getX() + 5, bpmBounds_.getY() + 4, 66, 20);
    drawInset(bpmLCD, 4.0f);
    if (isDraggingBPM_)
    {
        g.setColour(Theme::orange1.withAlpha(0.15f));
        g.fillRoundedRectangle(bpmLCD, 4.0f);
    }
    
    g.setColour(juce::Colour(0xff52525b));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(7.5f));
    g.drawText("BPM", (int)bpmLCD.getX() + 3, (int)bpmLCD.getY() + 1, 20, 8, juce::Justification::centredLeft);
    
    g.setColour(isDraggingBPM_ ? Theme::orange2 : Theme::orange1);
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(14.0f).withStyle("Bold"));
    g.drawText(juce::String((int)bpm_), (int)bpmLCD.getX(), (int)bpmLCD.getY() + 5, (int)bpmLCD.getWidth(), 15, juce::Justification::centred);
    
    float btnX = bpmBounds_.getRight() - 22;
    auto bpmPlusBtn = juce::Rectangle<float>(btnX, bpmBounds_.getY() + 3, 17, 10);
    auto bpmMinusBtn = juce::Rectangle<float>(btnX, bpmBounds_.getY() + 15, 17, 10);
    for (auto& btn : { bpmPlusBtn, bpmMinusBtn })
    {
        juce::ColourGradient bg(juce::Colour(0xff222226), 0.0f, btn.getY(),
                                juce::Colour(0xff18181b), 0.0f, btn.getBottom(), false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(btn, 3.0f);
        g.setColour(juce::Colour(0xff3f3f46));
        g.drawRoundedRectangle(btn.reduced(0.5f), 3.0f, 0.5f);
    }
    g.setColour(Theme::orange1);
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(10.0f).withStyle("Bold"));
    g.drawText("+", bpmPlusBtn.toNearestInt(), juce::Justification::centred);
    g.drawText("-", bpmMinusBtn.toNearestInt(), juce::Justification::centred);
    
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
    g.drawText("0:00:00", timeLCD.toNearestInt(), juce::Justification::centred);
    
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
    
    // ═══════════════════════════════════
    // PIANO / MIXER toggle group
    // ═══════════════════════════════════
    pianoBtnRect_    = juce::Rectangle<float>((float)x,                             (float)cy - 14, 58, 28);
    mixerBtnRect_    = juce::Rectangle<float>(pianoBtnRect_.getRight(),               pianoBtnRect_.getY(), 58, 28);
    playlistBtnRect_ = juce::Rectangle<float>(mixerBtnRect_.getRight(),               pianoBtnRect_.getY(), 64, 28);
    
    auto toggleGroup = juce::Rectangle<float>(pianoBtnRect_.getX(), pianoBtnRect_.getY(),
                                               pianoBtnRect_.getWidth() + mixerBtnRect_.getWidth() + playlistBtnRect_.getWidth(),
                                               pianoBtnRect_.getHeight());
    drawPanel(toggleGroup, 8.0f);
    
    g.setColour(juce::Colour(0xff3f3f46));
    g.drawVerticalLine((int)pianoBtnRect_.getRight(), toggleGroup.getY() + 4, toggleGroup.getBottom() - 4);
    g.drawVerticalLine((int)mixerBtnRect_.getRight(), toggleGroup.getY() + 4, toggleGroup.getBottom() - 4);
    
    auto lerpColour = [](juce::Colour a, juce::Colour b, float t) {
        return juce::Colour::fromRGBA(
            (juce::uint8)(a.getRed()   + (b.getRed()   - a.getRed())   * t),
            (juce::uint8)(a.getGreen() + (b.getGreen() - a.getGreen()) * t),
            (juce::uint8)(a.getBlue()  + (b.getBlue()  - a.getBlue())  * t),
            (juce::uint8)(a.getAlpha() + (b.getAlpha() - a.getAlpha()) * t)
        );
    };
    
    float pianoAlpha    = (selectedView_ == 0) ? animationProgress_ : (1.0f - animationProgress_);
    pianoAlpha          = juce::jlimit(0.0f, 1.0f, pianoAlpha);
    float mixerAlpha    = (selectedView_ == 1) ? animationProgress_ : (1.0f - animationProgress_);
    mixerAlpha          = juce::jlimit(0.0f, 1.0f, mixerAlpha);
    float playlistAlpha = (selectedView_ == 2) ? animationProgress_ : (1.0f - animationProgress_);
    playlistAlpha       = juce::jlimit(0.0f, 1.0f, playlistAlpha);
    
    // ── PIANO (left, left-rounded) ──
    if (pianoAlpha > 0.0f)
    {
        g.setColour(Theme::orange1.withAlpha(pianoAlpha * 0.22f));
        juce::Path pp;
        pp.addRoundedRectangle(pianoBtnRect_.getX() + 1, pianoBtnRect_.getY() + 1,
                               pianoBtnRect_.getWidth() - 1, pianoBtnRect_.getHeight() - 2,
                               7.0f, 7.0f, true, false, true, false);
        g.fillPath(pp);
        if (pianoAlpha > 0.5f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f * pianoAlpha));
            g.drawHorizontalLine((int)pianoBtnRect_.getY() + 2, pianoBtnRect_.getX() + 6, pianoBtnRect_.getRight() - 4);
        }
    }
    g.setColour(lerpColour(juce::Colour(0xff71717a), juce::Colours::white, pianoAlpha));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f));
    g.drawText("PIANO", pianoBtnRect_.toNearestInt(), juce::Justification::centred);
    
    // ── MIXER (middle, no rounded corners) ──
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
    
    // ── PLAYLIST (right, right-rounded) ──
    if (playlistAlpha > 0.0f)
    {
        g.setColour(Theme::orange1.withAlpha(playlistAlpha * 0.22f));
        juce::Path lp;
        lp.addRoundedRectangle(playlistBtnRect_.getX(), playlistBtnRect_.getY() + 1,
                               playlistBtnRect_.getWidth() - 1, playlistBtnRect_.getHeight() - 2,
                               7.0f, 7.0f, false, true, false, true);
        g.fillPath(lp);
        if (playlistAlpha > 0.5f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f * playlistAlpha));
            g.drawHorizontalLine((int)playlistBtnRect_.getY() + 2, playlistBtnRect_.getX() + 4, playlistBtnRect_.getRight() - 6);
        }
    }
    g.setColour(lerpColour(juce::Colour(0xff71717a), juce::Colours::white, playlistAlpha));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f));
    g.drawText("PLAYLIST", playlistBtnRect_.toNearestInt(), juce::Justification::centred);
    
    // ═══════════════════════════════════
    // Right side: SAVE OPEN EXPORT LOG
    // ═══════════════════════════════════
    int rx = w - 10;
    
    auto drawRightBtn = [&](juce::Rectangle<float> r, const juce::String& label, bool accent) {
        drawPanel(r, 6.0f);
        if (accent)
        {
            g.setColour(Theme::orange2.withAlpha(0.2f));
            g.fillRoundedRectangle(r.reduced(1.0f), 6.0f);
            g.setColour(Theme::orange1);
        }
        else
        {
            g.setColour(juce::Colour(0xffa1a1aa));
        }
        g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f));
        g.drawText(label, r.toNearestInt(), juce::Justification::centred);
    };
    
    auto logBtn  = juce::Rectangle<float>((float)rx - 52, (float)cy - 13, 52, 26);
    auto expBtn  = juce::Rectangle<float>(logBtn.getX() - 6 - 64, (float)cy - 13, 64, 26);
    auto openBtn = juce::Rectangle<float>(expBtn.getX() - 6 - 50, (float)cy - 13, 50, 26);
    auto saveBtn = juce::Rectangle<float>(openBtn.getX() - 6 - 50, (float)cy - 13, 50, 26);
    
    drawRightBtn(logBtn,  "LOG",    true);
    drawRightBtn(expBtn,  "EXPORT", true);
    drawRightBtn(openBtn, "OPEN",   false);
    drawRightBtn(saveBtn, "SAVE",   false);
}

void TransportBar::resized()
{
    int h = getHeight();
    int cy = h / 2;
    int btnSize = 22;
    int y = cy - btnSize / 2;
    
    // Transport buttons inside pill: PAT(48) + gap(6) = 54, pill w=100
    // Center 3×22 + 2×6 = 78 inside 100 → start at 54 + 11 = 65
    int tpPillX = 10 + 48 + 6; // = 64
    int btnTotal = 3 * btnSize + 2 * 6; // = 78
    int bx = tpPillX + (100 - btnTotal) / 2;
    playBtn_->setBounds(bx, y, btnSize, btnSize); bx += btnSize + 6;
    stopBtn_->setBounds(bx, y, btnSize, btnSize); bx += btnSize + 6;
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
    int h = getHeight();
    int cy = h / 2;
    
    // Match paint() layout: PAT(48)+6 + TP(100)+6 + BPM(110)+6 + TIME(134)+6 + PAT_SEL(100)+4 + PAT_PLUS(28)+10 = x
    int x = 10 + 48 + 6 + 100 + 6 + 110 + 6 + 134 + 6 + 100 + 4 + 28 + 10;
    pianoBtnRect_    = juce::Rectangle<float>((float)x, (float)cy - 14, 58, 28);
    mixerBtnRect_    = juce::Rectangle<float>(pianoBtnRect_.getRight(),  pianoBtnRect_.getY(), 58, 28);
    playlistBtnRect_ = juce::Rectangle<float>(mixerBtnRect_.getRight(),  pianoBtnRect_.getY(), 64, 28);
}

void TransportBar::mouseDown(const juce::MouseEvent& e)
{
    int w = getWidth();
    int h = getHeight();
    int cy = h / 2;
    
    // Check if clicking on BPM +/- buttons or display
    if (bpmBounds_.contains((float)e.x, (float)e.y))
    {
        // Match paint() layout: LCD at (bpmBounds_.x+5, bpmBounds_.y+4, 66, 20)
        // +/- buttons at (bpmBounds_.right-22, bpmBounds_.y+3/15, 17, 10)
        auto bpmDisplay = juce::Rectangle<float>(bpmBounds_.getX() + 5, bpmBounds_.getY() + 4, 66, 20);
        float btnX = bpmBounds_.getRight() - 22;
        auto bpmPlusBtn  = juce::Rectangle<float>(btnX, bpmBounds_.getY() + 3,  17, 10);
        auto bpmMinusBtn = juce::Rectangle<float>(btnX, bpmBounds_.getY() + 15, 17, 10);
        
        if (bpmPlusBtn.contains((float)e.x, (float)e.y))
        {
            // Increase BPM by 1
            double newBPM = juce::jlimit(20.0, 999.0, bpm_ + 1.0);
            if (newBPM != bpm_)
            {
                bpm_ = newBPM;
                bpmSlider_.setValue(bpm_, juce::dontSendNotification);
                if (onBPMChanged) onBPMChanged(bpm_);
                repaint();
            }
            return;
        }
        else if (bpmMinusBtn.contains((float)e.x, (float)e.y))
        {
            // Decrease BPM by 1
            double newBPM = juce::jlimit(20.0, 999.0, bpm_ - 1.0);
            if (newBPM != bpm_)
            {
                bpm_ = newBPM;
                bpmSlider_.setValue(bpm_, juce::dontSendNotification);
                if (onBPMChanged) onBPMChanged(bpm_);
                repaint();
            }
            return;
        }
        else if (bpmDisplay.contains((float)e.x, (float)e.y))
        {
            // Start dragging
            isDraggingBPM_ = true;
            dragStartY_ = e.y;
            dragStartBPM_ = bpm_;
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            repaint();
            return;
        }
    }
    
    
    // PIANO button
    if (pianoBtnRect_.contains(e.getPosition().toFloat()))
    {
        if (selectedView_ != 0)
        {
            previousView_ = selectedView_;
            selectedView_ = 0;
            animationProgress_ = 0.0f;
            startTimer(16); // 60 FPS
        }
        if (onPianoToggle) onPianoToggle();
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
    
    // Right side buttons: LOG (rightmost), EXPORT, OPEN, SAVE
    int rx = w - 12;
    auto logR  = juce::Rectangle<float>((float)rx - 56, (float)cy - 13, 56, 26);
    auto expR  = juce::Rectangle<float>((float)logR.getX() - 6 - 64, (float)cy - 13, 64, 26);
    auto openR = juce::Rectangle<float>((float)expR.getX() - 6 - 50, (float)cy - 13, 50, 26);
    auto saveR = juce::Rectangle<float>((float)openR.getX() - 6 - 50, (float)cy - 13, 50, 26);
    
    if (logR.contains((float)e.x, (float)e.y))  { if (onLog) onLog(); return; }
    if (expR.contains((float)e.x, (float)e.y))  { if (onExport) onExport(); return; }
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

void TransportBar::togglePlay()
{
    isPlaying_ = !isPlaying_;
    if (auto* btn = dynamic_cast<TransportButton*>(playBtn_.get()))
        btn->setActive(isPlaying_);
    if (onPlayStateChanged) onPlayStateChanged(isPlaying_);
}

void TransportBar::stop()
{
    isPlaying_ = false;
    isRecording_ = false;
    if (auto* btn = dynamic_cast<TransportButton*>(playBtn_.get()))
        btn->setActive(false);
    if (auto* btn = dynamic_cast<TransportButton*>(recordBtn_.get()))
        btn->setActive(false);
    if (onPlayStateChanged) onPlayStateChanged(false);
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
    animationProgress_ += 0.05f; // Animation speed
    
    if (animationProgress_ >= 1.0f)
    {
        animationProgress_ = 1.0f;
        stopTimer();
    }
    
    repaint();
}
