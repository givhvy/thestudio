#include "Playlist.h"
#include "PluginHost.h"
#include "Theme.h"
#include <algorithm>
#include <limits>

Playlist::Playlist(PluginHost& pluginHost) : pluginHost_(pluginHost)
{
    setWantsKeyboardFocus(true);

    // Default starting clip: the "Pattern 1" block at bar 1 on track 1
    Clip c;
    c.kind      = ClipKind::Pattern;
    c.track     = 0;
    c.startBar  = 0.0f;
    c.lengthBar = 1.0f;
    c.label     = "Pattern 1";
    clips_.push_back(c);
}
Playlist::~Playlist() = default;

void Playlist::paint(juce::Graphics& g)
{
    int w = getWidth();
    int h = getHeight();
    
    // Background (deep dark)
    g.fillAll(juce::Colour(0xff09090b));
    
    // ── Header (PLAYLIST title + Snap) ──────────────────────────
    auto headerRect = juce::Rectangle<int>(0, 0, w, HEADER_H);
    g.setColour(juce::Colour(0xff141417));
    g.fillRect(headerRect);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(HEADER_H - 1, 0.0f, (float)w);
    
    // Mini play arrow + ALL filter pill
    juce::Path playArr;
    playArr.addTriangle(8.0f, 8.0f, 8.0f, 20.0f, 18.0f, 14.0f);
    g.setColour(Theme::zinc500);
    g.fillPath(playArr);
    
    auto allPill = juce::Rectangle<float>(24, 6, 36, 16);
    juce::ColourGradient apGrad(juce::Colour(0xff2a2a2e), 0.0f, allPill.getY(),
                                  juce::Colour(0xff18181b), 0.0f, allPill.getBottom(), false);
    g.setGradientFill(apGrad);
    g.fillRoundedRectangle(allPill, 3.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(allPill, 3.0f, 1.0f);
    g.setColour(Theme::zinc300);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f));
    g.drawText("ALL", (int)allPill.getX(), (int)allPill.getY(), 22, 16, juce::Justification::centred);
    juce::Path apArr;
    apArr.addTriangle(allPill.getRight() - 11, allPill.getCentreY() - 2, 
                       allPill.getRight() - 5, allPill.getCentreY() - 2,
                       allPill.getRight() - 8, allPill.getCentreY() + 2);
    g.setColour(Theme::zinc500);
    g.fillPath(apArr);
    
    // Pattern label
    g.setColour(Theme::orange2);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f));
    g.drawText(juce::String::fromUTF8("\xe2\x96\xb6"),
               68, 0, 16, HEADER_H, juce::Justification::centredLeft);
    g.setColour(Theme::zinc300);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f));
    g.drawText("Pattern 1", 86, 0, 80, HEADER_H, juce::Justification::centredLeft);
    
    // PLAYLIST title (centered)
    auto dotRect = juce::Rectangle<float>((float)patternStripW() + 220, 11, 6, 6);
    Theme::drawGlowLED(g, dotRect, Theme::orange2, true);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
    g.drawText("PLAYLIST", patternStripW() + 234, 0, 100, HEADER_H, juce::Justification::centredLeft);
    
    // Snap: 1/4 (right)
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
    g.drawText("Snap: 1/4", w - 80, 0, 70, HEADER_H, juce::Justification::centredRight);
    
    // ── Pattern strip (left vertical column) ───────────────────
    auto stripRect = juce::Rectangle<int>(0, HEADER_H, patternStripW(), h - HEADER_H);
    g.setColour(juce::Colour(0xff0d0d10));
    g.fillRect(stripRect);
    g.setColour(juce::Colours::black);
    g.drawVerticalLine(patternStripW() - 1, (float)HEADER_H, (float)h);

    // Collapse / expand chevron at the top of the strip
    {
        auto tog = patternToggleRect();
        g.setColour(Theme::zinc500);
        juce::Path ch;
        if (patternStripCollapsed_)
        {
            // ▶ expand
            ch.addTriangle((float)tog.getX() + 4, (float)tog.getCentreY() - 4,
                           (float)tog.getX() + 4, (float)tog.getCentreY() + 4,
                           (float)tog.getRight() - 4, (float)tog.getCentreY());
        }
        else
        {
            // ◀ collapse
            ch.addTriangle((float)tog.getRight() - 4, (float)tog.getCentreY() - 4,
                           (float)tog.getRight() - 4, (float)tog.getCentreY() + 4,
                           (float)tog.getX() + 4,     (float)tog.getCentreY());
        }
        g.fillPath(ch);
    }

    // "Pattern X" label only when the strip is expanded
    if (!patternStripCollapsed_)
    {
    juce::Path stripTri;
    stripTri.addTriangle(8.0f, (float)HEADER_H + 12, 14.0f, (float)HEADER_H + 8, 14.0f, (float)HEADER_H + 16);
    g.setColour(Theme::orange2);
    g.fillPath(stripTri);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText(currentPatternName_, 18, HEADER_H + 4, 60, 18, juce::Justification::centredLeft);
    } // end if (!patternStripCollapsed_)

    // ── Track rows ──────────────────────────────────────────────
    int tracksTopY = HEADER_H + RULER_H;
    int trackAreaX = patternStripW();
    int trackAreaW = w - patternStripW();
    
    // Ruler (1, 2, 3, 4, 5...)
    auto rulerRect = juce::Rectangle<int>(trackAreaX, HEADER_H, trackAreaW, RULER_H);
    g.setColour(juce::Colour(0xff141417));
    g.fillRect(rulerRect);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(rulerRect.getBottom() - 1, (float)trackAreaX, (float)w);
    
    // Bar numbers
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
    int barX = trackAreaX + TRACK_LABEL_W;
    int bw = barW();
    for (int b = 1; b <= 64 && barX < w; ++b)
    {
        g.drawText(juce::String(b), barX - 4, rulerRect.getY(), 20, RULER_H, juce::Justification::centredLeft);
        // Bar tick
        g.setColour(juce::Colour(0xff222226));
        g.drawVerticalLine(barX, (float)rulerRect.getY() + 14, (float)rulerRect.getBottom());
        g.setColour(Theme::zinc500);
        barX += bw;
    }
    
    // Track rows
    for (int t = 0; t < numTracks_; ++t)
    {
        int rowY = tracksTopY + t * TRACK_H - scrollY_;
        if (rowY + TRACK_H < tracksTopY) continue;
        if (rowY > h) break;
        
        bool isSelected = (t == selectedTrack_);
        
        // Row background
        if (t % 2 == 0)
        {
            g.setColour(juce::Colour(0xff0c0c0e));
            g.fillRect(trackAreaX, rowY, trackAreaW, TRACK_H);
        }
        
        // Track label area (left of timeline)
        auto labelRect = juce::Rectangle<int>(trackAreaX, rowY, TRACK_LABEL_W, TRACK_H);
        if (isSelected)
        {
            g.setColour(juce::Colour(0xff1a1a1e));
            g.fillRect(labelRect);
        }
        // Right border on label
        g.setColour(juce::Colour(0xff222226));
        g.drawVerticalLine(labelRect.getRight() - 1, (float)rowY, (float)rowY + TRACK_H);
        
        // TRACK X label
        g.setColour(Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
        g.drawText("TRACK " + juce::String(t + 1), labelRect.getX() + 8, rowY, 
                    labelRect.getWidth() - 24, TRACK_H, juce::Justification::centredLeft);
        
        // Orange dot on right of label
        auto trackDot = juce::Rectangle<float>((float)labelRect.getRight() - 16, (float)rowY + (float)TRACK_H/2 - 3, 6, 6);
        Theme::drawGlowLED(g, trackDot, Theme::orange2, true);
        
        // Bottom row separator
        g.setColour(juce::Colour(0xff141417));
        g.drawHorizontalLine(rowY + TRACK_H - 1, (float)trackAreaX, (float)w);
        
    }

    // ── Draw clips ─────────────────────────────────────────────
    g.saveState();
    g.reduceClipRegion(trackAreaX + TRACK_LABEL_W, tracksTopY,
                       w - trackAreaX - TRACK_LABEL_W, h - tracksTopY);
    for (const auto& c : clips_)
    {
        auto block = clipRect(c);
        if (block.getBottom() < tracksTopY || block.getY() > h) continue;

        if (c.kind == ClipKind::Pattern)
        {
            // Base orange gradient block (slightly darker so the preview pops)
            juce::ColourGradient bGrad(Theme::orange3, 0.0f, block.getY(),
                                         Theme::orange4, 0.0f, block.getBottom(), false);
            g.setGradientFill(bGrad);
            g.fillRoundedRectangle(block, 3.0f);

            // Top sheen
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.drawHorizontalLine((int)block.getY() + 1, block.getX() + 3, block.getRight() - 3);

            // ── Mini "what's inside" preview ─────────────────────────
            // Title strip on top, preview grid fills the rest.
            const float titleH = juce::jmin(12.0f, block.getHeight() * 0.45f);
            auto titleArea = block.withHeight(titleH).reduced(2.0f, 1.0f);
            auto previewArea = block.withTrimmedTop(titleH).reduced(2.0f, 1.0f);

            if (previewArea.getWidth() > 6.0f && previewArea.getHeight() > 4.0f
                && getPatternGrid)
            {
                auto grid = getPatternGrid();
                int rows = (int)grid.size();
                if (rows > 0)
                {
                    int cols = (int)grid[0].size();
                    if (cols > 0)
                    {
                        // Translucent background for the preview
                        g.setColour(juce::Colour(0xff2a0a02).withAlpha(0.55f));
                        g.fillRect(previewArea);

                        const float cellW = previewArea.getWidth()  / (float)cols;
                        const float cellH = previewArea.getHeight() / (float)rows;
                        // Active step dots
                        g.setColour(juce::Colours::white.withAlpha(0.92f));
                        for (int r = 0; r < rows; ++r)
                        {
                            for (int s = 0; s < cols && s < (int)grid[r].size(); ++s)
                            {
                                if (!grid[r][s]) continue;
                                float x = previewArea.getX() + s * cellW;
                                float y = previewArea.getY() + r * cellH;
                                float w = juce::jmax(1.0f, cellW - 0.6f);
                                float h = juce::jmax(1.0f, cellH - 0.6f);
                                g.fillRect(x + 0.3f, y + 0.3f, w, h);
                            }
                        }
                    }
                }
            }

            // Border (deep orange)
            g.setColour(juce::Colour(0xff431407));
            g.drawRoundedRectangle(block, 3.0f, 1.0f);

            // Pattern label across the title strip (only if there's room)
            if (titleH >= 9.0f)
            {
                g.setColour(juce::Colours::white);
                g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
                g.drawText(c.label, titleArea.toNearestInt(),
                           juce::Justification::centredLeft, true);
            }
        }
        else // Sample
        {
            juce::ColourGradient sGrad(juce::Colour(0xff2563eb), 0.0f, block.getY(),
                                         juce::Colour(0xff1e40af), 0.0f, block.getBottom(), false);
            g.setGradientFill(sGrad);
            g.fillRoundedRectangle(block, 3.0f);
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawHorizontalLine((int)block.getY() + 1, block.getX() + 3, block.getRight() - 3);
            g.setColour(juce::Colour(0xff0c1f4d));
            g.drawRoundedRectangle(block, 3.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
            g.drawText(c.label, block.toNearestInt().reduced(4, 0), juce::Justification::centredLeft, true);
        }
    }

    // Selection ring around any clips in selectedClips_
    if (!selectedClips_.empty())
    {
        g.setColour(juce::Colour(0xff60a5fa));   // sky blue accent
        for (int idx : selectedClips_)
        {
            if (idx < 0 || idx >= (int)clips_.size()) continue;
            auto r = clipRect(clips_[idx]).expanded(1.5f);
            g.drawRoundedRectangle(r, 4.0f, 1.8f);
        }
    }

    // Box-select rectangle while dragging
    if (boxSelecting_ && !boxRect_.isEmpty())
    {
        g.setColour(juce::Colour(0xff60a5fa).withAlpha(0.18f));
        g.fillRect(boxRect_);
        g.setColour(juce::Colour(0xff60a5fa));
        g.drawRect(boxRect_, 1.0f);
    }

    // Drop highlight (single-bar ghost cell)
    if (dropHighlightTrack_ >= 0 && dropHighlightBar_ >= 0)
    {
        int bx = trackAreaX + TRACK_LABEL_W + dropHighlightBar_ * barW();
        int by = tracksTopY + dropHighlightTrack_ * TRACK_H - scrollY_;
        auto ghost = juce::Rectangle<float>((float)bx + 4, (float)by + 3,
                                              (float)barW() - 8, (float)TRACK_H - 6);
        g.setColour(juce::Colour(0xfff97316).withAlpha(0.35f));
        g.fillRoundedRectangle(ghost, 3.0f);
        g.setColour(juce::Colour(0xfff97316));
        g.drawRoundedRectangle(ghost, 3.0f, 1.5f);
    }
    g.restoreState();
    
    // ── Smooth playhead ─────────────────────────────────────────
    const int gridStartX = trackAreaX + TRACK_LABEL_W;
    int phX = gridStartX;   // at bar 1.0 when idle

    if (isPlaying_ && playStep_ >= 0)
    {
        float phase = 0.0f;
        if (stepMs_ > 1.0)
        {
            double elapsed = juce::Time::getMillisecondCounterHiRes() - lastTickMs_;
            phase = (float) juce::jlimit(0.0, 1.0, elapsed / stepMs_);
        }
        // 16 steps = 1 bar wide.
        float barsElapsed = (playStep_ + phase) / 16.0f;
        phX = gridStartX + (int)(barsElapsed * (float)barW());
    }

    if (phX >= gridStartX && phX <= w)
    {
        // Glow
        g.setColour(juce::Colour(0xfff97316).withAlpha(0.25f));
        g.fillRect(phX - 2, HEADER_H, 4, h - HEADER_H);
        // Crisp line
        g.setColour(juce::Colour(0xffff9a3d));
        g.drawVerticalLine(phX, (float)HEADER_H, (float)h);

        // Ruler triangle
        if (isPlaying_)
        {
            juce::Path tri;
            tri.addTriangle((float)phX - 5, (float)(HEADER_H + RULER_H) - 8,
                            (float)phX + 5, (float)(HEADER_H + RULER_H) - 8,
                            (float)phX,     (float)(HEADER_H + RULER_H) - 1);
            g.setColour(juce::Colour(0xffff9a3d));
            g.fillPath(tri);
        }
    }
}

void Playlist::setPlayhead(int currentStep, bool playing, double bpm)
{
    playStep_   = currentStep;
    isPlaying_  = playing;
    stepMs_     = (bpm > 1.0) ? (60000.0 / bpm / 4.0) : 166.67;
    lastTickMs_ = juce::Time::getMillisecondCounterHiRes();

    if (playing)
    {
        if (!isTimerRunning()) startTimerHz(60);
        // Fire any sample clips that should sound at this step.
        triggerSampleClipsAt(currentStep);
    }
    else
    {
        stopTimer();
        // Reset trigger latches so re-starting plays clips again.
        for (auto& c : clips_) c.lastFiredStep = -1;
    }
    repaint();
}

void Playlist::timerCallback()
{
    if (isPlaying_) repaint();
}

void Playlist::resized() {}

void Playlist::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        // Ctrl+wheel = zoom horizontally
        float delta = wheel.deltaY * 0.15f;
        zoomX_ = juce::jlimit(0.25f, 8.0f, zoomX_ + delta);
        repaint();
    }
    else
    {
        // Vertical scroll (~3 rows per notch on Windows; deltaY ≈ 0.15 / notch)
        scrollY_ -= (int)(wheel.deltaY * (float)(TRACK_H * 20));
        int total = numTracks_ * TRACK_H;
        int avail = getHeight() - HEADER_H - RULER_H;
        int maxScroll = std::max(0, total - avail);
        scrollY_ = juce::jlimit(0, maxScroll, scrollY_);
        repaint();
    }
}

// ── Geometry helpers ──────────────────────────────────────────
juce::Rectangle<float> Playlist::clipRect(const Clip& c) const
{
    const int trackAreaX = patternStripW();
    const int gridStartX = trackAreaX + TRACK_LABEL_W;
    const int tracksTopY = HEADER_H + RULER_H;
    const int bw = barW();

    int x = gridStartX + (int)(c.startBar * (float)bw) + 4;
    int y = tracksTopY + c.track * TRACK_H - scrollY_ + 3;
    int wpx = (int)(c.lengthBar * (float)bw) - 8;
    int hpx = TRACK_H - 6;
    return juce::Rectangle<float>((float)x, (float)y, (float)wpx, (float)hpx);
}

int Playlist::pixelToTrack(int y) const
{
    const int tracksTopY = HEADER_H + RULER_H;
    if (y < tracksTopY) return -1;
    int idx = (y - tracksTopY + scrollY_) / TRACK_H;
    return (idx >= 0 && idx < numTracks_) ? idx : -1;
}

float Playlist::pixelToBar(int x) const
{
    const int gridStartX = patternStripW() + TRACK_LABEL_W;
    if (x < gridStartX) return -1.0f;
    return (float)(x - gridStartX) / (float)barW();
}

juce::Rectangle<int> Playlist::patternToggleRect() const
{
    // 16-px wide square at top of the pattern strip (just below the header).
    return juce::Rectangle<int>(0, HEADER_H, patternStripW(), 18);
}

int Playlist::findClipAt(int x, int y) const
{
    for (int i = (int)clips_.size() - 1; i >= 0; --i)
    {
        if (clipRect(clips_[i]).contains((float)x, (float)y))
            return i;
    }
    return -1;
}

void Playlist::showClipContextMenu(int clipIdx)
{
    if (clipIdx < 0 || clipIdx >= (int)clips_.size()) return;
    juce::PopupMenu m;
    m.addItem(1, "Delete clip");
    m.addItem(2, "Duplicate clip");
    m.showMenuAsync(juce::PopupMenu::Options(),
        [this, clipIdx](int r)
        {
            if (clipIdx >= (int)clips_.size()) return;
            if (r == 1) { clips_.erase(clips_.begin() + clipIdx); repaint(); }
            else if (r == 2) {
                Clip dup = clips_[clipIdx];
                dup.startBar += dup.lengthBar;
                clips_.push_back(dup);
                repaint();
            }
        });
}

// ── Mouse interactions ────────────────────────────────────────
void Playlist::mouseDown(const juce::MouseEvent& e)
{
    draggingClip_ = -1;
    dragMoved_    = false;
    boxSelecting_ = false;
    grabKeyboardFocus();

    // Pattern-strip collapse / expand chevron (top of the left strip)
    if (patternToggleRect().contains(e.x, e.y))
    {
        patternStripCollapsed_ = !patternStripCollapsed_;
        repaint();
        return;
    }

    // Track-row selection from label gutter
    int trackAreaX = patternStripW();
    if (e.x < trackAreaX + TRACK_LABEL_W)
    {
        int t = pixelToTrack(e.y);
        if (t >= 0) { selectedTrack_ = t; repaint(); }
        return;
    }

    const bool ctrl  = e.mods.isCtrlDown();
    const bool right = e.mods.isRightButtonDown();
    int hit = findClipAt(e.x, e.y);

    // Ctrl + drag (left or right mouse) on a clip → CLONE-drag (duplicate it).
    // Matches FL Studio's "hold Ctrl and drag the pattern to copy" behaviour.
    if (ctrl && hit >= 0)
    {
        Clip dup = clips_[hit];
        clips_.push_back(dup);
        int newIdx = (int)clips_.size() - 1;
        draggingClip_     = newIdx;
        dragGrabDeltaBar_ = clips_[newIdx].startBar - pixelToBar(e.x);
        selectedTrack_    = clips_[newIdx].track;
        selectedClips_.clear();
        repaint();
        return;
    }

    // Ctrl + RMB on empty space → box-select. (Ctrl+LMB on empty also.)
    if (ctrl && (right || hit < 0))
    {
        boxSelecting_ = true;
        boxStart_     = e.getPosition();
        boxRect_      = juce::Rectangle<int>(boxStart_, boxStart_);
        if (!e.mods.isShiftDown()) selectedClips_.clear();
        repaint();
        return;
    }

    // Right-click on a clip → DELETE immediately (FL Studio-style; no popup).
    if (right)
    {
        if (hit >= 0)
        {
            if (selectedClips_.count(hit))
            {
                std::vector<int> toErase(selectedClips_.begin(), selectedClips_.end());
                std::sort(toErase.begin(), toErase.end(), std::greater<int>());
                for (int i : toErase)
                    if (i >= 0 && i < (int)clips_.size())
                        clips_.erase(clips_.begin() + i);
                selectedClips_.clear();
            }
            else
            {
                clips_.erase(clips_.begin() + hit);
            }
            repaint();
        }
        return;
    }

    if (hit >= 0)
    {

        // Plain click on a clip that's NOT in the current selection clears it
        if (!selectedClips_.count(hit))
            selectedClips_.clear();

        // Begin drag-move
        draggingClip_     = hit;
        dragGrabDeltaBar_ = clips_[hit].startBar - pixelToBar(e.x);
        selectedTrack_    = clips_[hit].track;
        repaint();
        return;
    }

    // Empty cell click on timeline → drop a Pattern clip there (no Ctrl)
    int t = pixelToTrack(e.y);
    float b = pixelToBar(e.x);
    if (t >= 0 && b >= 0.0f)
    {
        selectedClips_.clear();
        Clip c;
        c.kind     = ClipKind::Pattern;
        c.track    = t;
        c.startBar = (float)snapBars(b);
        c.lengthBar = 1.0f;
        c.label    = "Pattern 1";
        clips_.push_back(c);
        selectedTrack_ = t;
        draggingClip_     = (int)clips_.size() - 1;
        dragGrabDeltaBar_ = 0.0f;
        repaint();
    }
}

void Playlist::mouseDrag(const juce::MouseEvent& e)
{
    // Box-select drag
    if (boxSelecting_)
    {
        boxRect_ = juce::Rectangle<int>(boxStart_, e.getPosition());
        // Recompute hit set (live)
        std::set<int> hits;
        for (int i = 0; i < (int)clips_.size(); ++i)
            if (clipRect(clips_[i]).toNearestInt().intersects(boxRect_))
                hits.insert(i);
        // Shift-drag adds, plain drag replaces
        if (e.mods.isShiftDown())
            selectedClips_.insert(hits.begin(), hits.end());
        else
            selectedClips_ = hits;
        repaint();
        return;
    }

    if (draggingClip_ < 0 || draggingClip_ >= (int)clips_.size()) return;
    auto& c = clips_[draggingClip_];

    float newStart = pixelToBar(e.x) + dragGrabDeltaBar_;
    newStart = (float)snapBars(juce::jmax(0.0f, newStart));

    int newTrack = pixelToTrack(e.y);
    if (newTrack < 0) newTrack = c.track;

    int dBar   = (int)newStart - (int)c.startBar;
    int dTrack = newTrack      - c.track;

    if (dBar != 0 || dTrack != 0)
    {
        // Move ALL selected clips by the same delta if the dragged clip is in the selection.
        if (selectedClips_.count(draggingClip_))
        {
            for (int idx : selectedClips_)
            {
                if (idx < 0 || idx >= (int)clips_.size()) continue;
                auto& s = clips_[idx];
                s.startBar = juce::jmax(0.0f, s.startBar + (float)dBar);
                s.track    = juce::jlimit(0, numTracks_ - 1, s.track + dTrack);
            }
        }
        else
        {
            c.startBar = newStart;
            c.track    = newTrack;
        }
        dragMoved_ = true;
        repaint();
    }
}

void Playlist::mouseUp(const juce::MouseEvent&)
{
    draggingClip_ = -1;
    dragMoved_    = false;
    boxSelecting_ = false;
    boxRect_      = {};
    repaint();
}

bool Playlist::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (selectedClips_.empty()) return false;
        // Erase from highest index down to keep indices valid
        std::vector<int> toErase(selectedClips_.begin(), selectedClips_.end());
        std::sort(toErase.begin(), toErase.end(), std::greater<int>());
        for (int i : toErase)
            if (i >= 0 && i < (int)clips_.size())
                clips_.erase(clips_.begin() + i);
        selectedClips_.clear();
        repaint();
        return true;
    }
    if (key == juce::KeyPress('a', juce::ModifierKeys::ctrlModifier, 0))
    {
        selectedClips_.clear();
        for (int i = 0; i < (int)clips_.size(); ++i) selectedClips_.insert(i);
        repaint();
        return true;
    }
    if (key == juce::KeyPress('c', juce::ModifierKeys::ctrlModifier, 0))
    {
        clipboard_.clear();
        for (int i : selectedClips_)
            if (i >= 0 && i < (int)clips_.size())
                clipboard_.push_back(clips_[i]);
        return true;
    }
    if (key == juce::KeyPress('v', juce::ModifierKeys::ctrlModifier, 0))
    {
        if (clipboard_.empty()) return false;

        // Find the leftmost edge in the clipboard so pasted group keeps relative spacing.
        float clipboardMinStart = std::numeric_limits<float>::max();
        for (const auto& c : clipboard_)
            clipboardMinStart = std::min(clipboardMinStart, c.startBar);

        // Paste anchor: just after the rightmost edge of the current selection,
        // or at bar 0 if nothing's selected.
        float pasteAt = 0.0f;
        for (int i : selectedClips_)
            if (i >= 0 && i < (int)clips_.size())
                pasteAt = std::max(pasteAt, clips_[i].startBar + clips_[i].lengthBar);

        selectedClips_.clear();
        for (auto c : clipboard_)
        {
            c.startBar = pasteAt + (c.startBar - clipboardMinStart);
            clips_.push_back(c);
            selectedClips_.insert((int)clips_.size() - 1);
        }
        repaint();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        if (selectedClips_.empty()) return false;
        selectedClips_.clear();
        repaint();
        return true;
    }
    return false;
}

// ── Drag and drop from in-app sources (Browser) ───────────────
bool Playlist::isInterestedInDragSource(const SourceDetails& d)
{
    return d.description.isString();
}

void Playlist::itemDragEnter(const SourceDetails& d) { itemDragMove(d); }

void Playlist::itemDragMove(const SourceDetails& d)
{
    int t = pixelToTrack(d.localPosition.y);
    float b = pixelToBar(d.localPosition.x);
    dropHighlightTrack_ = t;
    dropHighlightBar_   = (b >= 0.0f) ? snapBars(b) : -1;
    repaint();
}

void Playlist::itemDragExit(const SourceDetails&)
{
    dropHighlightTrack_ = -1;
    dropHighlightBar_   = -1;
    repaint();
}

void Playlist::itemDropped(const SourceDetails& d)
{
    juce::String path = d.description.toString();
    juce::File file(path);
    int t = pixelToTrack(d.localPosition.y);
    float b = pixelToBar(d.localPosition.x);
    dropHighlightTrack_ = -1;
    dropHighlightBar_   = -1;

    if (file.existsAsFile() && t >= 0 && b >= 0.0f)
    {
        Clip c;
        c.kind       = ClipKind::Sample;
        c.track      = t;
        c.startBar   = (float)snapBars(b);
        c.lengthBar  = 1.0f;
        c.label      = file.getFileNameWithoutExtension();
        c.sampleFile = file;
        clips_.push_back(c);
        selectedTrack_ = t;
    }
    repaint();
}

// ── External file drops (from Windows Explorer) ───────────────
bool Playlist::isInterestedInFileDrag(const juce::StringArray&) { return true; }

void Playlist::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    fileDragMove(files, x, y);
}

void Playlist::fileDragMove(const juce::StringArray&, int x, int y)
{
    int t = pixelToTrack(y);
    float b = pixelToBar(x);
    dropHighlightTrack_ = t;
    dropHighlightBar_   = (b >= 0.0f) ? snapBars(b) : -1;
    repaint();
}

void Playlist::fileDragExit(const juce::StringArray&)
{
    dropHighlightTrack_ = -1;
    dropHighlightBar_   = -1;
    repaint();
}

void Playlist::filesDropped(const juce::StringArray& files, int x, int y)
{
    int t = pixelToTrack(y);
    float startBar = pixelToBar(x);
    dropHighlightTrack_ = -1;
    dropHighlightBar_   = -1;

    if (t < 0 || startBar < 0.0f) { repaint(); return; }
    float bar = (float)snapBars(startBar);

    for (const auto& path : files)
    {
        juce::File f(path);
        if (!f.existsAsFile()) continue;
        // Only accept audio-ish extensions; ignore folders, midi, plugin files
        auto ext = f.getFileExtension().toLowerCase();
        if (ext != ".wav" && ext != ".mp3" && ext != ".flac"
            && ext != ".aif" && ext != ".aiff" && ext != ".ogg") continue;

        Clip c;
        c.kind       = ClipKind::Sample;
        c.track      = t;
        c.startBar   = bar;
        c.lengthBar  = 1.0f;
        c.label      = f.getFileNameWithoutExtension();
        c.sampleFile = f;
        clips_.push_back(c);
        bar += 1.0f;
    }
    selectedTrack_ = t;
    repaint();
}

// ── Sample-clip one-shot triggering during playback ───────────
// ─── Project I/O ─────────────────────────────────────────────────────
juce::var Playlist::toJson() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("numTracks",   numTracks_);
    obj->setProperty("zoomX",       zoomX_);
    obj->setProperty("patternStripCollapsed", patternStripCollapsed_);
    obj->setProperty("currentPatternName",    currentPatternName_);

    juce::Array<juce::var> arr;
    for (const auto& c : clips_)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("kind",       (int)c.kind);
        o->setProperty("track",      c.track);
        o->setProperty("startBar",   c.startBar);
        o->setProperty("lengthBar",  c.lengthBar);
        o->setProperty("label",      c.label);
        o->setProperty("sampleFile", c.sampleFile.getFullPathName());
        arr.add(juce::var(o));
    }
    obj->setProperty("clips", arr);
    return juce::var(obj);
}

void Playlist::fromJson(const juce::var& v)
{
    if (!v.isObject()) return;
    numTracks_ = (int)v.getProperty("numTracks", 30);
    zoomX_     = (float)(double)v.getProperty("zoomX", 1.0);
    patternStripCollapsed_ = (bool)v.getProperty("patternStripCollapsed", false);
    currentPatternName_    = v.getProperty("currentPatternName", "Pattern 1").toString();

    clips_.clear();
    if (auto* arr = v.getProperty("clips", juce::var()).getArray())
    {
        for (auto& cv : *arr)
        {
            Clip c;
            c.kind      = (ClipKind)(int)cv.getProperty("kind",     (int)ClipKind::Pattern);
            c.track     = (int)cv.getProperty("track",     0);
            c.startBar  = (float)(double)cv.getProperty("startBar",  0.0);
            c.lengthBar = (float)(double)cv.getProperty("lengthBar", 1.0);
            c.label     = cv.getProperty("label", "Pattern 1").toString();
            juce::String path = cv.getProperty("sampleFile", "").toString();
            if (path.isNotEmpty()) c.sampleFile = juce::File(path);
            clips_.push_back(std::move(c));
        }
    }
    scrollY_ = 0;
    repaint();
}

void Playlist::triggerSampleClipsAt(int playStep)
{
    // V1: the channel rack loops a single 16-step bar. Each time we hit
    // step 0 of a new loop, fire every Sample clip once. Use a per-clip
    // "armed" latch so we only fire once per loop iteration.
    if (playStep != 0)
    {
        // Re-arm clips for the next loop pass.
        for (auto& c : clips_) c.lastFiredStep = -1;
        return;
    }
    for (auto& c : clips_)
    {
        if (c.kind != ClipKind::Sample) continue;
        if (c.lastFiredStep == 0) continue; // already fired this pass
        if (c.sampleFile.existsAsFile())
            pluginHost_.playSampleFile(c.sampleFile);
        c.lastFiredStep = 0;
    }
}
