#include "BottomDock.h"
#include "Theme.h"
#include <cmath>

class BottomDock::SessionVideoHost : public juce::Component
{
public:
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff050507));
    }

    void resized() override
    {
        const auto area = getLocalBounds();
        for (int i = 0; i < getNumChildComponents(); ++i)
            getChildComponent(i)->setBounds(area);
    }

    void visibilityChanged() override
    {
        resized();
    }
};

BottomDock::BottomDock()
{
    visualLevels_.fill(0.0f);
    sessionVideoHost_ = std::make_unique<SessionVideoHost>();
    addChildComponent(*sessionVideoHost_);
    startTimerHz(30);
}

juce::Rectangle<int> BottomDock::getSessionPanelRect() const
{
    const int margin = 8;
    const int gap = 8;
    const int panelH = getHeight() - 2 * margin;
    const int totalW = getWidth() - 2 * margin - 2 * gap;
    const int sessionW = totalW * 30 / 100;
    return { margin, margin, sessionW, panelH };
}

juce::Component* BottomDock::getSessionVideoHost()
{
    return sessionVideoHost_.get();
}

void BottomDock::setSessionVideoMode(bool showVideo)
{
    if (sessionVideoMode_ == showVideo)
        return;

    sessionVideoMode_ = showVideo;
    if (sessionVideoHost_)
        sessionVideoHost_->setVisible(showVideo);
    layoutSessionVideoHost();
    repaint();
}

void BottomDock::layoutSessionVideoHost()
{
    if (!sessionVideoHost_)
        return;

    auto session = getSessionPanelRect();
    sessionVideoHost_->setBounds(session.withTrimmedTop(22).reduced(1));
    if (sessionVideoMode_)
        sessionVideoHost_->toFront(false);

    if (onSessionVideoLayout)
        onSessionVideoLayout();
}

static void drawPanelHeader(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour led)
{
    auto header = juce::Rectangle<int>(r.getX(), r.getY(), r.getWidth(), 22);
    g.setColour(juce::Colour(0xff0d0d10));
    g.fillRect(header);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(header.getBottom() - 1, (float)r.getX(), (float)r.getRight());
    
    auto dot = juce::Rectangle<float>((float)r.getX() + 8, (float)r.getY() + 8, 6, 6);
    Theme::drawGlowLED(g, dot, led, true);
    
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
    g.drawText(title, r.getX() + 22, r.getY(), r.getWidth() - 28, 22, juce::Justification::centredLeft);
}

static void drawPanelChassis(juce::Graphics& g, juce::Rectangle<int> r)
{
    juce::ColourGradient grad(juce::Colour(0xff141417), 0.0f, (float)r.getY(),
                                juce::Colour(0xff0a0a0c), 0.0f, (float)r.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRect(r);
    g.setColour(juce::Colours::black);
    g.drawRect(r, 1);
    g.setColour(juce::Colour(0xff222226));
    g.drawHorizontalLine(r.getY(), (float)r.getX(), (float)r.getRight());
}

void BottomDock::paint(juce::Graphics& g)
{
    int w = getWidth();
    int h = getHeight();
    
    g.fillAll(juce::Colour(0xff0a0a0c));
    
    // Top divider
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(0, 0.0f, (float)w);
    
    int margin = 8;
    int gap = 8;
    int panelH = h - 2 * margin;
    int totalW = w - 2 * margin - 2 * gap;
    int sessionW = totalW * 30 / 100;
    int mixerPrevW = totalW * 40 / 100;
    int quickW = totalW - sessionW - mixerPrevW;
    
    // ── Panel 1: SESSION (or embedded VIDEO) ────────────────────
    auto session = juce::Rectangle<int>(margin, margin, sessionW, panelH);
    drawPanelChassis(g, session);
    drawPanelHeader(g, session, sessionVideoMode_ ? "VIDEO" : "SESSION", Theme::orange2);

    sessionRestoreRect_ = {};
    if (sessionVideoMode_)
    {
        sessionRestoreRect_ = juce::Rectangle<int>(session.getRight() - 78, session.getY() + 4, 66, 15);
        auto br = sessionRestoreRect_.toFloat();
        g.setColour(juce::Colour(0xff27272a));
        g.fillRoundedRectangle(br, 3.0f);
        g.setColour(Theme::zinc500);
        g.drawRoundedRectangle(br, 3.0f, 1.0f);
        g.setColour(Theme::zinc300);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
        g.drawText("SESSION", sessionRestoreRect_, juce::Justification::centred);
    }
    else
    {
        int sy = session.getY() + 32;
        auto drawRow = [&](const juce::String& label, const juce::String& value, juce::Colour valueColor) {
            g.setColour(Theme::zinc500);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
            g.drawText(label, session.getX() + 12, sy, 80, 18, juce::Justification::centredLeft);
            g.setColour(valueColor);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
            g.drawText(value, session.getX() + 12, sy, session.getWidth() - 24, 18, juce::Justification::centredRight);
            sy += 22;
            g.setColour(juce::Colour(0xff141417));
            g.drawHorizontalLine(sy - 1, (float)session.getX() + 12, (float)session.getRight() - 12);
        };
        drawRow("PROJECT",  "Untitled",   Theme::zinc200);
        drawRow("PATTERN",  "Pattern 1",  Theme::orange2);
        drawRow("STATUS",   "Stopped",    Theme::red2);
    }
    
    // ── Panel 2: MIXER PREVIEW ──────────────────────────────────
    auto mxp = juce::Rectangle<int>(session.getRight() + gap, margin, mixerPrevW, panelH);
    drawPanelChassis(g, mxp);
    drawPanelHeader(g, mxp, "MIXER PREVIEW", Theme::orange2);

    moreButtonRect_ = juce::Rectangle<int>(mxp.getRight() - 64, mxp.getY() + 4, 52, 15);
    {
        auto br = moreButtonRect_.toFloat();
        juce::ColourGradient mg(visualizerOpen_ ? Theme::orange3 : juce::Colour(0xff27272a),
                                br.getX(), br.getY(),
                                visualizerOpen_ ? Theme::orange5 : juce::Colour(0xff111114),
                                br.getX(), br.getBottom(), false);
        g.setGradientFill(mg);
        g.fillRoundedRectangle(br, 3.0f);
        g.setColour(visualizerOpen_ ? Theme::orange1 : juce::Colour(0xff3f3f46));
        g.drawRoundedRectangle(br, 3.0f, 1.0f);
        g.setColour(visualizerOpen_ ? juce::Colours::white : Theme::zinc300);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
        g.drawText(visualizerOpen_ ? "METERS" : "MORE", moreButtonRect_, juce::Justification::centred);
    }
    
    // Pull live data from Mixer if providers are wired, else use fallback defaults.
    static const char* fallbackNames[] = { "KICK", "SNARE", "HIHAT", "CLAP", "PERC", "BASS", "LEAD", "PAD" };
    static const float fallbackVols[]  = { 0.78f, 0.64f, 0.55f, 0.46f, 0.50f, 0.70f, 0.62f, 0.58f };
    
    int totalTracks = getMixerTrackCount ? getMixerTrackCount() : 8;
    int numTracks   = juce::jlimit(1, 8, totalTracks);  // preview shows up to 8 strips
    int stripW = (mxp.getWidth() - 16) / numTracks;
    int faderTop = mxp.getY() + 30;
    int faderBot = mxp.getBottom() - 22;
    int faderHeight = faderBot - faderTop;

    // Cache for mouse handlers
    previewPanelRect_  = mxp;
    previewStripW_     = stripW;
    previewFaderTop_   = faderTop;
    previewFaderBot_   = faderBot;
    previewNumTracks_  = numTracks;

    if (visualizerOpen_)
    {
        auto scope = mxp.reduced(12, 28);
        g.setColour(juce::Colour(0xff050507));
        g.fillRoundedRectangle(scope.toFloat(), 5.0f);
        g.setColour(juce::Colour(0xff27272a));
        g.drawRoundedRectangle(scope.toFloat(), 5.0f, 1.0f);

        for (int gx = scope.getX() + 12; gx < scope.getRight(); gx += 28)
        {
            g.setColour(juce::Colours::white.withAlpha(0.025f));
            g.drawVerticalLine(gx, (float)scope.getY() + 6, (float)scope.getBottom() - 6);
        }

        const int bands = juce::jlimit(4, 8, numTracks);
        const int bandGap = 8;
        const int bandW = juce::jmax(8, (scope.getWidth() - bandGap * (bands + 1)) / bands);
        const int baseY = scope.getBottom() - 12;
        const int maxH = scope.getHeight() - 28;

        juce::Path wave;
        for (int xpix = 0; xpix < scope.getWidth(); ++xpix)
        {
            const float t = ((float)xpix / (float)juce::jmax(1, scope.getWidth())) * juce::MathConstants<float>::twoPi;
            const float amp = 10.0f + 22.0f * juce::jlimit(0.0f, 1.0f, visualLevels_[0] + visualLevels_[2] * 0.65f);
            const float y = (float)scope.getCentreY()
                + std::sin(t * 3.0f + visualPhase_) * amp * 0.55f
                + std::sin(t * 8.0f + visualPhase_ * 1.7f) * amp * 0.18f;
            if (xpix == 0) wave.startNewSubPath((float)scope.getX(), y);
            else wave.lineTo((float)scope.getX() + (float)xpix, y);
        }
        g.setColour(Theme::orange2.withAlpha(0.28f));
        g.strokePath(wave, juce::PathStrokeType(5.0f));
        g.setColour(Theme::orange1);
        g.strokePath(wave, juce::PathStrokeType(1.4f));

        for (int i = 0; i < bands; ++i)
        {
            const float level = juce::jlimit(0.0f, 1.0f, visualLevels_[(size_t)i]);
            const int bh = juce::jmax(3, (int)(level * (float)maxH));
            const int bx = scope.getX() + bandGap + i * (bandW + bandGap);
            auto bar = juce::Rectangle<float>((float)bx, (float)baseY - (float)bh, (float)bandW, (float)bh);
            juce::ColourGradient vg(Theme::orange1, bar.getX(), bar.getY(),
                                    Theme::orange4, bar.getX(), bar.getBottom(), false);
            g.setGradientFill(vg);
            g.fillRoundedRectangle(bar, 3.0f);
            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.drawHorizontalLine((int)bar.getY() + 1, bar.getX() + 2, bar.getRight() - 2);

            juce::String name = getMixerTrackName ? getMixerTrackName(i).toUpperCase() : juce::String(fallbackNames[i]);
            g.setColour(Theme::zinc400);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(7.0f).withStyle("Bold"));
            g.drawText(name.substring(0, 5), bx - 3, scope.getBottom() - 11, bandW + 6, 9, juce::Justification::centred);
        }
    }
    else
    {
    for (int i = 0; i < numTracks; ++i)
    {
        int sx = mxp.getX() + 8 + i * stripW;
        int cx = sx + stripW / 2;

        float vol = getMixerTrackVolume ? getMixerTrackVolume(i) : fallbackVols[i];
        bool muted = getMixerTrackMuted ? getMixerTrackMuted(i) : false;
        juce::String name = getMixerTrackName ? getMixerTrackName(i).toUpperCase()
                                              : juce::String(fallbackNames[i]);

        // Fader track (recessed)
        auto track = juce::Rectangle<float>((float)cx - 2, (float)faderTop, 4, (float)faderHeight);
        g.setColour(juce::Colour(0xff050507));
        g.fillRoundedRectangle(track, 2.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(track, 2.0f, 1.0f);
        
        // Fader fill (blue, dimmed if muted)
        float fillH = faderHeight * vol;
        auto fill = juce::Rectangle<float>((float)cx - 2, (float)faderBot - fillH, 4, fillH);
        juce::Colour fillTop = muted ? Theme::zinc500 : Theme::blue.brighter(0.3f);
        juce::Colour fillBot = muted ? Theme::zinc500.darker(0.4f) : Theme::blue.darker(0.3f);
        juce::ColourGradient bGrad(fillTop, 0.0f, fill.getY(),
                                   fillBot, 0.0f, fill.getBottom(), false);
        g.setGradientFill(bGrad);
        g.fillRoundedRectangle(fill, 2.0f);
        
        // Fader handle
        auto handle = juce::Rectangle<float>((float)cx - 8, (float)faderBot - fillH - 4, 16, 8);
        juce::ColourGradient hGrad(juce::Colour(0xffd4d4d8), 0.0f, handle.getY(),
                                     juce::Colour(0xff52525b), 0.0f, handle.getBottom(), false);
        g.setGradientFill(hGrad);
        g.fillRoundedRectangle(handle, 2.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(handle, 2.0f, 1.0f);
        
        // Track label
        g.setColour(muted ? Theme::zinc600 : Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(7.5f).withStyle("Bold"));
        g.drawText(name, sx, mxp.getBottom() - 18, stripW, 14, juce::Justification::centred);
    }
    }

    // ── Panel 3: QUICK TOOLS ────────────────────────────────────
    auto qt = juce::Rectangle<int>(mxp.getRight() + gap, margin, quickW, panelH);
    drawPanelChassis(g, qt);
    drawPanelHeader(g, qt, "QUICK TOOLS", Theme::orange2);
    
    const char* tools[] = { "MIXER", "PIANO ROLL", "CHANNEL RACK", "PATTERNS", "VIDEO", "AI" };
    int btnsTop = qt.getY() + 30;
    int btnsAvailH = qt.getHeight() - 36;
    int colW = (qt.getWidth() - 24) / 2;
    int rowH = btnsAvailH / 3 - 4;
    
    for (int i = 0; i < 6; ++i)
    {
        int row = i / 2;
        int col = i % 2;
        int bx = qt.getX() + 8 + col * (colW + 8);
        int by = btnsTop + row * (rowH + 4);
        auto btn = juce::Rectangle<float>((float)bx, (float)by, (float)colW, (float)rowH);
        
        // Store button rect for click detection
        buttonRects_[i] = btn;
        
        const bool selected = (i == selectedButtonIndex_);
        juce::Colour top = selected ? Theme::orange3 : juce::Colour(0xff2a2a2e);
        juce::Colour bot = selected ? Theme::orange5 : juce::Colour(0xff18181b);
        juce::ColourGradient bgr(top, 0.0f, btn.getY(), bot, 0.0f, btn.getBottom(), false);
        g.setGradientFill(bgr);
        g.fillRoundedRectangle(btn, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawHorizontalLine((int)btn.getY() + 1, btn.getX() + 4, btn.getRight() - 4);
        g.setColour(selected ? Theme::orange1 : juce::Colours::black);
        g.drawRoundedRectangle(btn, 4.0f, 1.0f);
        
        g.setColour(selected ? juce::Colours::white : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(tools[i], btn.toNearestInt(), juce::Justification::centred);
    }
}

void BottomDock::resized()
{
    layoutSessionVideoHost();
}

void BottomDock::setSelectedButton(int index)
{
    const int clamped = (index >= 0 && index < 6) ? index : -1;
    if (selectedButtonIndex_ == clamped)
        return;
    selectedButtonIndex_ = clamped;
    repaint();
}

static int hitTestPreviewFader(const juce::Point<int>& p,
                                const juce::Rectangle<int>& panel,
                                int stripW, int numTracks,
                                int faderTop, int faderBot)
{
    if (numTracks <= 0 || stripW <= 0) return -1;
    if (p.y < faderTop - 4 || p.y > faderBot + 6) return -1;
    int relX = p.x - (panel.getX() + 8);
    if (relX < 0 || relX >= stripW * numTracks) return -1;
    return juce::jlimit(0, numTracks - 1, relX / stripW);
}

static float volumeFromMouseY(int mouseY, int faderTop, int faderBot)
{
    int h = juce::jmax(1, faderBot - faderTop);
    float v = (float)(faderBot - mouseY) / (float)h;
    return juce::jlimit(0.0f, 1.0f, v);
}

void BottomDock::mouseDown(const juce::MouseEvent& e)
{
    juce::Point<float> pos = e.getPosition().toFloat();

    if (sessionVideoMode_ && sessionRestoreRect_.contains(e.x, e.y))
    {
        if (onRestoreSessionInfo)
            onRestoreSessionInfo();
        return;
    }

    if (moreButtonRect_.contains(e.x, e.y))
    {
        visualizerOpen_ = !visualizerOpen_;
        repaint();
        return;
    }

    // Preview fader takes precedence
    int trackIdx = hitTestPreviewFader(e.getPosition(), previewPanelRect_,
                                       previewStripW_, previewNumTracks_,
                                       previewFaderTop_, previewFaderBot_);
    if (!visualizerOpen_ && trackIdx >= 0)
    {
        if (setMixerTrackVolume)
            setMixerTrackVolume(trackIdx, volumeFromMouseY(e.y, previewFaderTop_, previewFaderBot_));
        return;
    }

    if (buttonRects_[0].contains(pos) && onMixer) onMixer();
    else if (buttonRects_[1].contains(pos) && onPianoRoll) onPianoRoll();
    else if (buttonRects_[2].contains(pos) && onChannelRack) onChannelRack();
    else if (buttonRects_[3].contains(pos) && onPatterns) onPatterns();
    else if (buttonRects_[4].contains(pos) && onVideo) onVideo();
    else if (buttonRects_[5].contains(pos) && onAI) onAI();
}

void BottomDock::mouseDrag(const juce::MouseEvent& e)
{
    if (visualizerOpen_)
        return;

    int trackIdx = hitTestPreviewFader(e.getMouseDownPosition(), previewPanelRect_,
                                       previewStripW_, previewNumTracks_,
                                       previewFaderTop_, previewFaderBot_);
    if (trackIdx < 0) return;
    if (setMixerTrackVolume)
        setMixerTrackVolume(trackIdx, volumeFromMouseY(e.y, previewFaderTop_, previewFaderBot_));
}

void BottomDock::timerCallback()
{
    visualPhase_ += 0.18f;
    if (visualPhase_ > juce::MathConstants<float>::twoPi)
        visualPhase_ -= juce::MathConstants<float>::twoPi;

    bool changed = visualizerOpen_;
    for (int i = 0; i < 8; ++i)
    {
        float target = getMixerTrackActivity ? getMixerTrackActivity(i) : 0.0f;
        if (target <= 0.001f)
            target = 0.02f * (0.5f + 0.5f * std::sin(visualPhase_ + (float)i));

        const float old = visualLevels_[(size_t)i];
        visualLevels_[(size_t)i] = juce::jmax(target, old * 0.86f);
        if (std::abs(old - visualLevels_[(size_t)i]) > 0.002f)
            changed = true;
    }

    if (changed)
        repaint(previewPanelRect_);
}
