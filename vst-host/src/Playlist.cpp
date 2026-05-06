#include "Playlist.h"
#include "PluginHost.h"
#include "Theme.h"

Playlist::Playlist(PluginHost& pluginHost) : pluginHost_(pluginHost) {}
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
    g.drawText("\xe2\x96\xb6", 68, 0, 16, HEADER_H, juce::Justification::centredLeft);
    g.setColour(Theme::zinc300);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f));
    g.drawText("Pattern 1", 86, 0, 80, HEADER_H, juce::Justification::centredLeft);
    
    // PLAYLIST title (centered)
    auto dotRect = juce::Rectangle<float>((float)PATTERN_STRIP_W + 220, 11, 6, 6);
    Theme::drawGlowLED(g, dotRect, Theme::orange2, true);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
    g.drawText("PLAYLIST", PATTERN_STRIP_W + 234, 0, 100, HEADER_H, juce::Justification::centredLeft);
    
    // Snap: 1/4 (right)
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
    g.drawText("Snap: 1/4", w - 80, 0, 70, HEADER_H, juce::Justification::centredRight);
    
    // ── Pattern 1 strip (left vertical column) ──────────────────
    auto stripRect = juce::Rectangle<int>(0, HEADER_H, PATTERN_STRIP_W, h - HEADER_H);
    g.setColour(juce::Colour(0xff0d0d10));
    g.fillRect(stripRect);
    g.setColour(juce::Colours::black);
    g.drawVerticalLine(PATTERN_STRIP_W - 1, (float)HEADER_H, (float)h);
    
    // "Pattern 1" with orange triangle
    juce::Path stripTri;
    stripTri.addTriangle(8.0f, (float)HEADER_H + 12, 14.0f, (float)HEADER_H + 8, 14.0f, (float)HEADER_H + 16);
    g.setColour(Theme::orange2);
    g.fillPath(stripTri);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("Pattern 1", 18, HEADER_H + 4, 60, 18, juce::Justification::centredLeft);
    
    // ── Track rows ──────────────────────────────────────────────
    int tracksTopY = HEADER_H + RULER_H;
    int trackAreaX = PATTERN_STRIP_W;
    int trackAreaW = w - PATTERN_STRIP_W;
    
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
        
        // For TRACK 1: draw a "Pattern 1" orange block
        if (t == 0)
        {
            int blockX = trackAreaX + TRACK_LABEL_W + 4;
            int blockW = barW() - 8;
            auto block = juce::Rectangle<float>((float)blockX, (float)rowY + 3, (float)blockW, (float)TRACK_H - 6);
            juce::ColourGradient bGrad(Theme::orange1, 0.0f, block.getY(),
                                         Theme::orange3, 0.0f, block.getBottom(), false);
            g.setGradientFill(bGrad);
            g.fillRoundedRectangle(block, 3.0f);
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawHorizontalLine((int)block.getY() + 1, block.getX() + 3, block.getRight() - 3);
            g.setColour(juce::Colour(0xff431407));
            g.drawRoundedRectangle(block, 3.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
            g.drawText("Pattern 1", block.toNearestInt(), juce::Justification::centred);
        }
    }
    
    // Vertical playhead at bar 1.0 (just over track label area)
    int phX = trackAreaX + TRACK_LABEL_W;
    g.setColour(Theme::orange2.withAlpha(0.6f));
    g.drawVerticalLine(phX, (float)HEADER_H, (float)h);
    g.setColour(Theme::orange2.withAlpha(0.2f));
    g.drawVerticalLine(phX - 1, (float)HEADER_H, (float)h);
    g.drawVerticalLine(phX + 1, (float)HEADER_H, (float)h);
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
        // Vertical scroll
        scrollY_ -= (int)(wheel.deltaY * 60);
        int total = numTracks_ * TRACK_H;
        int avail = getHeight() - HEADER_H - RULER_H;
        int maxScroll = std::max(0, total - avail);
        scrollY_ = juce::jlimit(0, maxScroll, scrollY_);
        repaint();
    }
}

void Playlist::mouseDown(const juce::MouseEvent& e)
{
    int tracksTopY = HEADER_H + RULER_H;
    if (e.y >= tracksTopY)
    {
        int idx = (e.y - tracksTopY + scrollY_) / TRACK_H;
        if (idx >= 0 && idx < numTracks_)
        {
            selectedTrack_ = idx;
            repaint();
        }
    }
}
