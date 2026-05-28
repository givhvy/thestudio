#include "Playlist.h"
#include "PluginHost.h"
#include "Theme.h"
#include "LoopBpmUtils.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
bool gAutoArrangeProducerTagEnabled = false;

juce::File producerTagStorageFile()
{
    auto dir = juce::File("D:\\tags");
    dir.createDirectory();
    return dir.getChildFile("LVNH (consolidated).wav");
}

int parseRootMidiFromFileName(const juce::String& name)
{
    const juce::String lower = name.toLowerCase();
    const std::array<std::pair<const char*, int>, 17> roots {{
        { "c#", 37 }, { "db", 37 },
        { "d#", 39 }, { "eb", 39 },
        { "f#", 42 }, { "gb", 42 },
        { "g#", 44 }, { "ab", 44 },
        { "a#", 46 }, { "bb", 46 },
        { "c", 36 }, { "d", 38 }, { "e", 40 }, { "f", 41 },
        { "g", 43 }, { "a", 45 }, { "b", 47 }
    }};

    for (const auto& [token, midi] : roots)
    {
        const juce::String needle(token);
        int pos = lower.indexOf(needle);
        while (pos >= 0)
        {
            const bool leftOk = pos == 0 || !juce::CharacterFunctions::isLetterOrDigit(lower[pos - 1]);
            const int after = pos + needle.length();
            const bool rightOk = after >= lower.length() || !juce::CharacterFunctions::isLetterOrDigit(lower[after]);
            if (leftOk && rightOk)
                return midi;
            pos = lower.indexOf(pos + 1, needle);
        }
    }

    return -1;
}

struct PatternChannelDrag
{
    bool valid = false;
    juce::String patternName;
    int channelIndex = -1;
    juce::String channelName;
    int patternSteps = 16;
};

PatternChannelDrag parsePatternChannelDrag(const juce::String& description)
{
    PatternChannelDrag out;
    if (!description.startsWith("pattern-channel\n"))
        return out;

    const auto lines = juce::StringArray::fromLines(description);
    if (lines.size() < 4)
        return out;

    out.valid = true;
    out.patternName = lines[1].isNotEmpty() ? lines[1] : "Pattern 1";
    out.channelIndex = lines[2].getIntValue();
    out.channelName = lines[3].isNotEmpty() ? lines[3] : ("Slot " + juce::String(out.channelIndex + 1));
    if (lines.size() >= 5)
        out.patternSteps = juce::jlimit(16, 32, lines[4].getIntValue() <= 16 ? 16 : 32);
    return out;
}

int foldMidiIntoC4ToC6(int midi)
{
    if (midi < 0)
        return midi;

    while (midi < 60)
        midi += 12;
    while (midi > 84)
        midi -= 12;

    return juce::jlimit(60, 84, midi);
}

int pitchClassToC4ToC6(int pitchClass, int previousMidi)
{
    pitchClass = ((pitchClass % 12) + 12) % 12;

    int best = 60 + pitchClass;
    while (best < 60)
        best += 12;
    while (best > 84)
        best -= 12;

    if (previousMidi >= 60 && previousMidi <= 84)
    {
        int closest = best;
        for (int candidate = pitchClass; candidate <= 84; candidate += 12)
        {
            if (candidate < 60)
                continue;

            if (std::abs(candidate - previousMidi) < std::abs(closest - previousMidi))
                closest = candidate;
        }
        best = closest;
    }

    return juce::jlimit(60, 84, best);
}

juce::String parseAudioDragPath(const juce::String& description, bool* isLoopLibrary = nullptr)
{
    if (isLoopLibrary != nullptr)
        *isLoopLibrary = false;

    if (description.startsWith("audio\n"))
    {
        const auto parts = juce::StringArray::fromLines(description);
        if (parts.size() >= 3)
        {
            if (isLoopLibrary != nullptr)
                *isLoopLibrary = parts[1].equalsIgnoreCase("LOOPS")
                              || parts[1].equalsIgnoreCase("Loops");
            return parts[2];
        }
    }

    return description;
}
}

Playlist::Playlist(PluginHost& pluginHost) : pluginHost_(pluginHost)
{
    audioFormatManager_.registerBasicFormats();
    setWantsKeyboardFocus(true);
    trackEnabled_.assign((size_t)numTracks_, true);

    // Default starting clip: the "Pattern 1" block at bar 1 on track 1
    Clip c;
    c.kind      = ClipKind::Pattern;
    c.track     = 0;
    c.startBar  = 0.0f;
    c.lengthBar = defaultPatternLengthBar();
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
    g.drawText(currentPatternName_, 86, 0, 120, HEADER_H, juce::Justification::centredLeft);

    auto trimButton = trimToolRect().toFloat();
    juce::ColourGradient trimGrad(trimToolActive_ ? Theme::accentBright : juce::Colour(0xff2a2a2e),
                                  0.0f, trimButton.getY(),
                                  trimToolActive_ ? Theme::accentDim : juce::Colour(0xff18181b),
                                  0.0f, trimButton.getBottom(), false);
    g.setGradientFill(trimGrad);
    g.fillRoundedRectangle(trimButton, 4.0f);
    g.setColour(trimToolActive_ ? Theme::accentBright : juce::Colours::black);
    g.drawRoundedRectangle(trimButton, 4.0f, 1.0f);
    g.setColour(trimToolActive_ ? juce::Colours::black : Theme::zinc300);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
    g.drawText("TRIM", trimToolRect(), juce::Justification::centred);

    auto arrangeButton = arrangeToolRect().toFloat();
    juce::ColourGradient arrangeGrad(Theme::accentBright.withAlpha(0.92f), 0.0f, arrangeButton.getY(),
                                     Theme::accentDim.withAlpha(0.92f), 0.0f, arrangeButton.getBottom(), false);
    g.setGradientFill(arrangeGrad);
    g.fillRoundedRectangle(arrangeButton, 4.0f);
    g.setColour(Theme::accentBright);
    g.drawRoundedRectangle(arrangeButton, 4.0f, 1.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
    g.drawText("ARRANGE", arrangeToolRect(), juce::Justification::centred);

    // ── FLAT HP button ──────────────────────────────────────────
    {
        const bool flatOn = isHeadphoneFlatEnabled && isHeadphoneFlatEnabled();
        auto flatBtn = flatHpBtnRect().toFloat();
        // Cyan/teal when ON (monitoring tool), dark gradient when OFF
        juce::Colour topCol = flatOn ? juce::Colour(0xff06b6d4) : juce::Colour(0xff2a2a2e);
        juce::Colour botCol = flatOn ? juce::Colour(0xff0891b2) : juce::Colour(0xff18181b);
        juce::ColourGradient flatGrad(topCol, 0.0f, flatBtn.getY(),
                                       botCol, 0.0f, flatBtn.getBottom(), false);
        g.setGradientFill(flatGrad);
        g.fillRoundedRectangle(flatBtn, 4.0f);
        g.setColour(flatOn ? juce::Colour(0xff06b6d4) : juce::Colours::black);
        g.drawRoundedRectangle(flatBtn, 4.0f, 1.0f);
        // Headphone icon (simple glyph)
        g.setColour(flatOn ? juce::Colours::black : Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
        g.drawText(juce::String::fromUTF8("\xf0\x9f\x8e\xa7"), flatHpBtnRect().withWidth(16).withX(flatHpBtnRect().getX() + 2),
                   juce::Justification::centred);
        g.setColour(flatOn ? juce::Colours::black : Theme::zinc300);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.0f).withStyle("Bold"));
        g.drawText("FLAT HP", flatHpBtnRect().withTrimmedLeft(14), juce::Justification::centred);
    }
    
    // PLAYLIST title (centered)
    auto dotRect = juce::Rectangle<float>((float)patternStripW() + 220, 11, 6, 6);
    Theme::drawGlowLED(g, dotRect, Theme::orange2, true);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
    g.drawText("PLAYLIST", patternStripW() + 234, 0, 100, HEADER_H, juce::Justification::centredLeft);
    
    // AI Assistant reopen (replaces Snap label)
    const bool aiOpen = isAiAssistantOpen && isAiAssistantOpen();
    auto aiBtn = openAiAssistantBtnRect().toFloat();
    juce::ColourGradient aiGrad(aiOpen ? Theme::orange3 : Theme::orange1.withAlpha(0.92f),
                                0.0f, aiBtn.getY(),
                                aiOpen ? Theme::orange5 : Theme::orange3.withAlpha(0.92f),
                                0.0f, aiBtn.getBottom(), false);
    g.setGradientFill(aiGrad);
    g.fillRoundedRectangle(aiBtn, 4.0f);
    g.setColour(aiOpen ? Theme::orange1 : juce::Colours::black.withAlpha(0.65f));
    g.drawRoundedRectangle(aiBtn, 4.0f, 1.0f);
    {
        auto icon = aiBtn.reduced(6.0f, 4.0f).removeFromLeft(14.0f);
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRoundedRectangle(icon.removeFromRight(2.5f), 1.2f);
        juce::Path chev;
        const float cx = icon.getCentreX() - 1.0f;
        const float cy = icon.getCentreY();
        chev.startNewSubPath(cx + 3.0f, cy - 4.0f);
        chev.lineTo(cx - 2.0f, cy);
        chev.lineTo(cx + 3.0f, cy + 4.0f);
        g.strokePath(chev, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    }
    g.setColour(aiOpen ? juce::Colours::white : juce::Colours::black);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
    g.drawText("AI", openAiAssistantBtnRect().withTrimmedLeft(20), juce::Justification::centred);
    
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
    int bw = barW();
    const float firstBarFloat = std::floor(viewStartBar_);
    const int firstBar = juce::jmax(0, (int)firstBarFloat);
    int barX = trackAreaX + TRACK_LABEL_W - (int)((viewStartBar_ - firstBarFloat) * (float)bw);
    for (int b = firstBar + 1; barX < w; ++b)
    {
        if (barX + bw >= trackAreaX + TRACK_LABEL_W)
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
        const bool trackOn = t >= 0 && t < (int)trackEnabled_.size() ? trackEnabled_[(size_t)t] : true;
        
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
        g.setColour(trackOn ? Theme::zinc400 : Theme::zinc600);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
        g.drawText("TRACK " + juce::String(t + 1), labelRect.getX() + 8, rowY, 
                    labelRect.getWidth() - 24, TRACK_H, juce::Justification::centredLeft);
        
        // Orange dot on right of label
        auto trackDot = juce::Rectangle<float>((float)labelRect.getRight() - 16, (float)rowY + (float)TRACK_H/2 - 3, 6, 6);
        Theme::drawGlowLED(g, trackDot, trackOn ? Theme::orange2 : Theme::zinc600, trackOn);
        if (!trackOn)
        {
            g.setColour(juce::Colours::black.withAlpha(0.24f));
            g.fillRect(trackAreaX + TRACK_LABEL_W, rowY, juce::jmax(0, trackAreaW - TRACK_LABEL_W), TRACK_H);
        }
        
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
        const bool clipTrackOn = c.track >= 0 && c.track < (int)trackEnabled_.size() ? trackEnabled_[(size_t)c.track] : true;
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
                if (c.sourceChannelIndex >= 0)
                {
                    std::vector<std::vector<bool>> filtered;
                    if (c.sourceChannelIndex < (int)grid.size())
                        filtered.push_back(grid[(size_t)c.sourceChannelIndex]);
                    grid = std::move(filtered);
                }
                int rows = (int)grid.size();
                if (rows > 0)
                {
                    int cols = 0;
                    for (const auto& row : grid)
                        cols = juce::jmax(cols, (int)row.size());
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
            g.setColour(juce::Colours::white.withAlpha(0.42f));
            g.fillRoundedRectangle(block.withWidth(3.0f).reduced(0.0f, 5.0f), 1.0f);
            g.fillRoundedRectangle(block.withX(block.getRight() - 3.0f).withWidth(3.0f).reduced(0.0f, 5.0f), 1.0f);

            if (!clipTrackOn)
            {
                g.setColour(juce::Colours::black.withAlpha(0.58f));
                g.fillRoundedRectangle(block, 3.0f);
            }

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
            if (c.kind == ClipKind::Automation)
            {
                juce::ColourGradient aGrad(juce::Colour(0xff7c2d12), 0.0f, block.getY(),
                                           juce::Colour(0xff1f130c), 0.0f, block.getBottom(), false);
                g.setGradientFill(aGrad);
                g.fillRoundedRectangle(block, 3.0f);
                g.setColour(Theme::orange2);
                g.drawRoundedRectangle(block, 3.0f, 1.0f);

                const auto lane = block.reduced(6.0f, 5.0f);
                const float x1 = lane.getX();
                const float x2 = lane.getRight();
                const float y1 = lane.getBottom() - juce::jlimit(0.0f, 1.0f, c.automationStartValue) * lane.getHeight();
                const float y2 = lane.getBottom() - juce::jlimit(0.0f, 1.0f, c.automationEndValue) * lane.getHeight();
                juce::Path curve;
                curve.startNewSubPath(x1, y1);
                curve.lineTo(x2, y2);
                g.setColour(juce::Colour(0xffffb86b));
                g.strokePath(curve, juce::PathStrokeType(2.0f));
                g.fillEllipse(x1 - 3.0f, y1 - 3.0f, 6.0f, 6.0f);
                g.fillEllipse(x2 - 3.0f, y2 - 3.0f, 6.0f, 6.0f);

                g.setColour(juce::Colours::white);
                g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
                g.drawText(c.label, block.toNearestInt().reduced(4, 0), juce::Justification::centredLeft, true);
                continue;
            }

            juce::ColourGradient sGrad(juce::Colour(0xff2563eb), 0.0f, block.getY(),
                                         juce::Colour(0xff1e40af), 0.0f, block.getBottom(), false);
            g.setGradientFill(sGrad);
            g.fillRoundedRectangle(block, 3.0f);
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawHorizontalLine((int)block.getY() + 1, block.getX() + 3, block.getRight() - 3);
            if (!c.waveformPeaks.empty() && block.getWidth() > 12.0f)
            {
                auto wave = block.reduced(3.0f, 5.0f);
                const float midY = wave.getCentreY();
                const float halfH = wave.getHeight() * 0.40f;
                g.setColour(juce::Colour(0xffcbd5e1).withAlpha(0.72f));
                const int cols = juce::jmax(1, (int)wave.getWidth());
                const float totalBars = c.sourceBars > 0.0f
                    ? c.sourceBars
                    : (c.sourceSeconds > 0.0
                        ? juce::jmax(0.25f, (float)(c.sourceSeconds / (240.0 / juce::jlimit(20.0, 999.0, bpm_))))
                        : c.lengthBar);
                const float trimStartNorm = juce::jlimit(0.0f, 1.0f, c.trimStartBar / juce::jmax(0.25f, totalBars));
                const float trimEndNorm = juce::jlimit(trimStartNorm, 1.0f,
                    (c.trimStartBar + c.lengthBar) / juce::jmax(0.25f, totalBars));
                for (int px = 0; px < cols; ++px)
                {
                    const float xNorm = (float)px / (float)cols;
                    const float sourceNorm = trimStartNorm + xNorm * (trimEndNorm - trimStartNorm);
                    const int peakIdx = juce::jlimit(0, (int)c.waveformPeaks.size() - 1,
                        (int)(sourceNorm * (float)c.waveformPeaks.size()));
                    const float rawPeak = juce::jlimit(0.0f, 1.0f, c.waveformPeaks[(size_t)peakIdx]);
                    const float peak = juce::jlimit(0.05f, 0.82f, std::sqrt(rawPeak) * 0.95f);
                    const float x = wave.getX() + (float)px;
                    g.drawVerticalLine((int)x, midY - peak * halfH, midY + peak * halfH);
                }
            }
            g.setColour(juce::Colour(0xff0c1f4d));
            g.drawRoundedRectangle(block, 3.0f, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.fillRoundedRectangle(block.withWidth(3.0f).reduced(0.0f, 5.0f), 1.0f);
            g.fillRoundedRectangle(block.withX(block.getRight() - 3.0f).withWidth(3.0f).reduced(0.0f, 5.0f), 1.0f);
            if (!clipTrackOn)
            {
                g.setColour(juce::Colours::black.withAlpha(0.58f));
                g.fillRoundedRectangle(block, 3.0f);
            }
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
        int bx = trackAreaX + TRACK_LABEL_W + (int)(((float)dropHighlightBar_ - viewStartBar_) * (float)barW());
        int by = tracksTopY + dropHighlightTrack_ * TRACK_H - scrollY_;
        auto ghost = juce::Rectangle<float>((float)bx + CLIP_INSET_X, (float)by + 3,
                                              (float)barW() - (CLIP_INSET_X * 2), (float)TRACK_H - 6);
        g.setColour(juce::Colour(0xfff97316).withAlpha(0.35f));
        g.fillRoundedRectangle(ghost, 3.0f);
        g.setColour(juce::Colour(0xfff97316));
        g.drawRoundedRectangle(ghost, 3.0f, 1.5f);
    }

    if (sliceDragging_ && slicingClip_ >= 0 && slicingClip_ < (int)clips_.size())
    {
        const int sx = trackAreaX + TRACK_LABEL_W + (int)((slicePreviewBar_ - viewStartBar_) * (float)barW());
        auto clip = clipRect(clips_[(size_t)slicingClip_]);
        g.setColour(juce::Colour(0xffffb86b).withAlpha(0.28f));
        g.fillRect(sx - 3, (int)clip.getY() - 3, 6, (int)clip.getHeight() + 6);
        g.setColour(juce::Colour(0xffffb86b));
        g.drawVerticalLine(sx, clip.getY() - 4.0f, clip.getBottom() + 4.0f);
    }
    g.restoreState();
    
    // ── Smooth playhead ─────────────────────────────────────────
    const int gridStartX = trackAreaX + TRACK_LABEL_W;
    int phX = playheadX();

    if (isPlaying_ && playStep_ >= 0)
    {
        float phase = 0.0f;
        if (stepMs_ > 1.0)
        {
            double elapsed = juce::Time::getMillisecondCounterHiRes() - lastTickMs_;
            phase = (float) juce::jlimit(0.0, 1.0, elapsed / stepMs_);
        }
        // 16 steps = 1 bar wide.
        float barsElapsed = ((float)absoluteStep_ + phase) / 16.0f;
        phX = gridStartX + CLIP_INSET_X + (int)((barsElapsed - viewStartBar_) * (float)barW());
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

    drawClipEditor(g);
}

juce::Rectangle<int> Playlist::clipEditorBounds() const
{
    const int width = juce::jmin(560, getWidth() - 40);
    const int height = 250;
    return juce::Rectangle<int>((getWidth() - width) / 2,
                                juce::jmax(HEADER_H + 18, getHeight() / 2 - height / 2),
                                width, height);
}

void Playlist::drawClipEditor(juce::Graphics& g)
{
    if (editorClip_ < 0 || editorClip_ >= (int)clips_.size()) return;
    const auto& c = clips_[(size_t)editorClip_];
    if (c.kind != ClipKind::Sample) return;

    auto modal = clipEditorBounds();
    g.setColour(juce::Colours::black.withAlpha(0.42f));
    g.fillRect(getLocalBounds());

    juce::ColourGradient bg(juce::Colour(0xff1b1b1f), 0.0f, (float)modal.getY(),
                            juce::Colour(0xff09090b), 0.0f, (float)modal.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(modal.toFloat(), 7.0f);
    g.setColour(juce::Colour(0xff3f3f46));
    g.drawRoundedRectangle(modal.toFloat().reduced(0.5f), 7.0f, 1.0f);

    auto header = modal.removeFromTop(34);
    g.setColour(juce::Colour(0xff121214));
    g.fillRect(header);
    g.setColour(Theme::orange2);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
    g.drawText(c.label + "  (Audio Clip)", header.reduced(12, 0), juce::Justification::centredLeft, true);

    editorCloseRect_ = juce::Rectangle<int>(header.getRight() - 30, header.getY() + 6, 22, 22);
    g.setColour(juce::Colour(0xff27272a));
    g.fillRoundedRectangle(editorCloseRect_.toFloat(), 4.0f);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(14.0f).withStyle("Bold"));
    g.drawText("X", editorCloseRect_, juce::Justification::centred);

    auto body = modal.reduced(14, 12);
    g.setColour(Theme::zinc300);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f).withStyle("Bold"));
    g.drawText("FILE", body.getX(), body.getY(), 50, 16, juce::Justification::centredLeft);
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
    g.drawText(c.sampleFile.getFileName(), body.getX() + 50, body.getY(), body.getWidth() - 50, 16,
               juce::Justification::centredLeft, true);

    g.setColour(Theme::zinc400);
    g.drawText("Track " + juce::String(c.track + 1) + "   Start bar " + juce::String(c.startBar + 1.0f, 2)
               + "   Length " + juce::String(c.lengthBar, 2) + " bars"
               + "   Source " + juce::String(c.sourceSeconds, 2) + "s",
               body.getX(), body.getY() + 22, body.getWidth(), 16, juce::Justification::centredLeft, true);

    editorVolumeRect_ = juce::Rectangle<int>(body.getX(), body.getY() + 56, body.getWidth() - 118, 24);
    g.setColour(Theme::zinc300);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f).withStyle("Bold"));
    g.drawText("VOLUME", editorVolumeRect_.getX(), editorVolumeRect_.getY() - 18, 80, 16, juce::Justification::centredLeft);
    auto lane = editorVolumeRect_.reduced(0, 8);
    g.setColour(juce::Colour(0xff050507));
    g.fillRoundedRectangle(lane.toFloat(), 4.0f);
    const int fillW = (int)((float)lane.getWidth() * juce::jlimit(0.0f, 1.0f, c.volume / 2.0f));
    g.setColour(Theme::orange2);
    g.fillRoundedRectangle(lane.withWidth(fillW).toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff3f3f46));
    g.drawRoundedRectangle(lane.toFloat(), 4.0f, 1.0f);
    g.setColour(Theme::zinc200);
    g.drawText(juce::String((int)std::round(c.volume * 100.0f)) + "%",
               editorVolumeRect_.getRight() + 10, editorVolumeRect_.getY(), 70, 24, juce::Justification::centredLeft);

    editorLengthResetRect_ = juce::Rectangle<int>(body.getX(), body.getY() + 92, 116, 24);
    g.setColour(juce::Colour(0xff27272a));
    g.fillRoundedRectangle(editorLengthResetRect_.toFloat(), 4.0f);
    g.setColour(Theme::orange2);
    g.drawRoundedRectangle(editorLengthResetRect_.toFloat(), 4.0f, 1.0f);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
    g.drawText("FIT FULL LENGTH", editorLengthResetRect_, juce::Justification::centred);

    editorExtractBassRect_ = juce::Rectangle<int>(body.getX() + 126, body.getY() + 92, 116, 24);
    g.setColour(juce::Colour(0xff27272a));
    g.fillRoundedRectangle(editorExtractBassRect_.toFloat(), 4.0f);
    g.setColour(Theme::orange2);
    g.drawRoundedRectangle(editorExtractBassRect_.toFloat(), 4.0f, 1.0f);
    g.setColour(Theme::zinc200);
    g.drawText("EXTRACT BASS", editorExtractBassRect_, juce::Justification::centred);

    editorChordifyMidiRect_ = juce::Rectangle<int>(body.getX() + 252, body.getY() + 92, 132, 24);
    g.setColour(juce::Colour(0xff1e3a5f));
    g.fillRoundedRectangle(editorChordifyMidiRect_.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff60a5fa));
    g.drawRoundedRectangle(editorChordifyMidiRect_.toFloat(), 4.0f, 1.0f);
    g.setColour(Theme::zinc200);
    g.drawText("CHORDIFY MIDI", editorChordifyMidiRect_, juce::Justification::centred);

    auto wave = juce::Rectangle<int>(body.getX(), body.getY() + 130, body.getWidth(), 54);
    g.setColour(juce::Colour(0xff050507));
    g.fillRect(wave);
    g.setColour(juce::Colour(0xff1d4ed8));
    g.drawRect(wave, 1);
    if (!c.waveformPeaks.empty())
    {
        g.setColour(juce::Colour(0xffdbeafe).withAlpha(0.85f));
        const float midY = (float)wave.getCentreY();
        const float halfH = (float)wave.getHeight() * 0.46f;
        for (int px = 0; px < wave.getWidth(); ++px)
        {
            const int peakIdx = juce::jlimit(0, (int)c.waveformPeaks.size() - 1,
                (int)((float)px / (float)juce::jmax(1, wave.getWidth()) * (float)c.waveformPeaks.size()));
            const float peak = juce::jlimit(0.02f, 1.0f, c.waveformPeaks[(size_t)peakIdx]);
            g.drawVerticalLine(wave.getX() + px, midY - peak * halfH, midY + peak * halfH);
        }
    }
}

int Playlist::frequencyToMidi(double frequencyHz)
{
    if (frequencyHz <= 0.0)
        return 36;

    return juce::jlimit(0, 127,
        (int)std::round(69.0 + 12.0 * std::log2(frequencyHz / 440.0)));
}

std::vector<Playlist::ExtractedBassNote> Playlist::extractBassMidiFromClip(const Clip& c)
{
    std::vector<ExtractedBassNote> notes;
    if (c.kind != ClipKind::Sample || !c.sampleFile.existsAsFile())
        return notes;

    std::unique_ptr<juce::AudioFormatReader> reader(audioFormatManager_.createReaderFor(c.sampleFile));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return notes;

    const double sourceBpm = (c.tempoSync && c.sourceBpm > 0.0)
        ? c.sourceBpm
        : juce::jlimit(20.0, 999.0, bpm_);
    const double sourceSecondsPerStep = (60.0 / sourceBpm) / 4.0;
    const double sourceStartSeconds = (double)juce::jmax(0.0f, c.trimStartBar) * 16.0 * sourceSecondsPerStep;
    const int stepCount = juce::jlimit(1, 256, (int)std::ceil(c.lengthBar * 16.0f));
    const int channels = juce::jlimit(1, 2, (int)reader->numChannels);
    const int fileRootMidi = parseRootMidiFromFileName(c.label);
    int previousRootMidi = fileRootMidi >= 0 ? foldMidiIntoC4ToC6(fileRootMidi) : -1;

    for (int barStart = 0; barStart < stepCount; barStart += 16)
    {
        const int barSteps = juce::jmin(16, stepCount - barStart);
        const double barStartSeconds = sourceStartSeconds + (double)barStart * sourceSecondsPerStep;
        const juce::int64 sampleStart = (juce::int64)std::llround(barStartSeconds * reader->sampleRate);
        const int numSamples = (int)juce::jlimit<juce::int64>(
            0, (juce::int64)std::llround((double)barSteps * sourceSecondsPerStep * reader->sampleRate),
            reader->lengthInSamples - sampleStart);

        int chosenRootMidi = -1;
        int chosenVelocity = 92;
        double chosenScore = 0.0;

        if (numSamples > 256 && sampleStart < reader->lengthInSamples)
        {
            juce::AudioBuffer<float> buffer(channels, numSamples);
            buffer.clear();
            reader->read(&buffer, 0, numSamples, sampleStart, true, channels > 1);

            std::vector<float> mono((size_t)numSamples, 0.0f);
            double mean = 0.0;
            double energy = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                float v = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                    v += buffer.getReadPointer(ch)[i];
                v /= (float)channels;
                mono[(size_t)i] = v;
                mean += v;
            }

            mean /= (double)numSamples;
            for (auto& v : mono)
            {
                v = (float)((double)v - mean);
                energy += (double)v * (double)v;
            }

            const double rms = std::sqrt(energy / (double)juce::jmax(1, numSamples));
            const int stride = juce::jmax(1, numSamples / 24000);
            const double sr = reader->sampleRate / (double)stride;
            const int analysisSamples = numSamples / stride;

            auto goertzelMagnitude = [&](double frequency)
            {
                if (frequency <= 15.0 || frequency >= sr * 0.45 || analysisSamples <= 4)
                    return 0.0;

                const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sr;
                const double coeff = 2.0 * std::cos(omega);
                double q0 = 0.0;
                double q1 = 0.0;
                double q2 = 0.0;
                for (int i = 0; i < analysisSamples; ++i)
                {
                    const int src = i * stride;
                    const double window = 0.5 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * (double)i / (double)juce::jmax(1, analysisSamples - 1));
                    q0 = coeff * q1 - q2 + (double)mono[(size_t)src] * window;
                    q2 = q1;
                    q1 = q0;
                }
                return q1 * q1 + q2 * q2 - q1 * q2 * coeff;
            };

            std::array<double, 12> chroma {};
            for (int pitch = 24; pitch <= 71; ++pitch)
            {
                const double fundamental = 440.0 * std::pow(2.0, ((double)pitch - 69.0) / 12.0);
                double score = goertzelMagnitude(fundamental);
                score += 0.58 * goertzelMagnitude(fundamental * 2.0);
                score += 0.28 * goertzelMagnitude(fundamental * 3.0);
                score += 0.12 * goertzelMagnitude(fundamental * 4.0);

                const double octaveWeight = pitch <= 47 ? 1.0 : (pitch <= 59 ? 0.62 : 0.32);
                chroma[(size_t)(pitch % 12)] += score * octaveWeight;
            }

            int chosenPitchClass = -1;
            for (int pc = 0; pc < 12; ++pc)
            {
                double score = chroma[(size_t)pc];
                if (fileRootMidi >= 0 && pc == (fileRootMidi % 12))
                    score *= 1.05;
                if (previousRootMidi >= 0 && pc == (previousRootMidi % 12))
                    score *= 1.04;
                if (score > chosenScore)
                {
                    chosenScore = score;
                    chosenPitchClass = pc;
                }
            }

            chosenVelocity = juce::jlimit(70, 115, (int)std::round(rms * 850.0));
            if (chosenPitchClass >= 0 && chosenScore > 0.0)
                chosenRootMidi = pitchClassToC4ToC6(chosenPitchClass, previousRootMidi);
        }

        if (chosenRootMidi < 0)
            chosenRootMidi = previousRootMidi;
        if (chosenRootMidi < 0)
            continue;

        previousRootMidi = chosenRootMidi;
        notes.push_back({
            chosenRootMidi,
            barStart,
            barSteps,
            chosenVelocity
        });
    }

    return notes;
}

void Playlist::requestBassExtractionForClip(const Clip& c, bool autoApply)
{
    if (c.kind != ClipKind::Sample || !c.sampleFile.existsAsFile())
        return;

    const double sourceBpm = (c.tempoSync && c.sourceBpm > 0.0)
        ? c.sourceBpm
        : juce::jlimit(20.0, 999.0, bpm_);
    const int maxSteps = juce::jlimit(1, 256, (int) std::ceil(c.lengthBar * 16.0f));

    auto deliverNotes = [this, autoApply, label = c.label](const std::vector<ExtractedBassNote>& notes)
    {
        if (notes.empty())
            return;

        if (autoApply)
        {
            if (onAutoExtractBassMidi)
                onAutoExtractBassMidi(label, notes);
        }
        else if (onExtractBassMidi)
        {
            onExtractBassMidi(label, notes);
        }
    };

    if (onRequestBassExtraction)
    {
        BassExtractionRequest req;
        req.sourceName = c.label;
        req.audioFile = c.sampleFile;
        req.bpmHint = sourceBpm;
        req.maxSteps = maxSteps;
        req.autoApply = autoApply;
        onRequestBassExtraction(req, deliverNotes);
        return;
    }

    deliverNotes(extractBassMidiFromClip(c));
}

std::vector<Playlist::ExtractedBassNote> Playlist::extractBassMidiFallback(const juce::File& file,
                                                                           const juce::String& label,
                                                                           double bpmHint,
                                                                           int maxSteps)
{
    Clip temp;
    temp.kind = ClipKind::Sample;
    temp.sampleFile = file;
    temp.label = label;
    temp.sourceBpm = bpmHint;
    temp.tempoSync = bpmHint > 0.0;
    temp.lengthBar = juce::jmax(1.0f, (float) maxSteps / 16.0f);
    return extractBassMidiFromClip(temp);
}

void Playlist::extractBassFromEditorClip()
{
    if (editorClip_ < 0 || editorClip_ >= (int)clips_.size())
        return;

    const auto& clip = clips_[(size_t)editorClip_];
    requestBassExtractionForClip(clip, false);
}

void Playlist::importChordifyMidiFromEditorClip()
{
    if (editorClip_ < 0 || editorClip_ >= (int)clips_.size())
        return;

    const auto& clip = clips_[(size_t)editorClip_];
    if (clip.kind != ClipKind::Sample)
        return;

    if (onImportChordifyMidiForClip)
    {
        BassExtractionRequest req;
        req.sourceName = clip.label;
        req.audioFile = clip.sampleFile;
        req.bpmHint = (clip.tempoSync && clip.sourceBpm > 0.0) ? clip.sourceBpm : bpm_;
        req.maxSteps = juce::jlimit(1, 256, (int) std::ceil(clip.lengthBar * 16.0f));
        req.autoApply = false;
        onImportChordifyMidiForClip(req);
    }
}
void Playlist::setPlayhead(int currentStep, bool playing, double bpm)
{
    if (currentStep >= 0)
        absoluteStep_ = juce::jmax(0, currentStep);

    playStep_   = absoluteStep_ % 16;
    isPlaying_  = playing;
    stepMs_     = (bpm > 1.0) ? (60000.0 / bpm / 4.0) : 166.67;
    lastTickMs_ = juce::Time::getMillisecondCounterHiRes();

    const bool stepChanged = (absoluteStep_ != lastSampleTriggerStep_);
    const bool justStarted = playing && !transportPlaying_;

    applyEffectAutomationAt(absoluteStep_);

    if (playing)
    {
        if (!isTimerRunning()) startTimerHz(60);
        if (playbackEnabled_)
        {
            // Resync playlist sample clips after scrub only. Do NOT stop voices on
            // transport start — ChannelRack triggers step 0 first, then calls us via
            // onPlayheadTick; stopSamplePlayback() here was cutting those hits (e43b997).
            // Stale voices are cleared in MainComponent before setPlaying().
            if (pendingSampleResync_)
            {
                pluginHost_.stopSamplePlaybackImmediate();
                triggerSampleClipsAt(absoluteStep_);
                lastSampleTriggerStep_ = absoluteStep_;
                pendingSampleResync_ = false;
            }
            else if (justStarted || stepChanged)
            {
                triggerSampleClipsAt(absoluteStep_);
                lastSampleTriggerStep_ = absoluteStep_;
            }
        }
    }
    else
    {
        stopTimer();
        for (auto& c : clips_) c.lastFiredStep = -1;
        lastSampleTriggerStep_ = -1;
        pendingSampleResync_ = false;
    }

    transportPlaying_ = playing;
    repaint();
}

void Playlist::setAbsoluteStep(int step)
{
    absoluteStep_ = juce::jmax(0, step);
    playStep_ = absoluteStep_ % 16;
    applyEffectAutomationAt(absoluteStep_);
    lastTickMs_ = juce::Time::getMillisecondCounterHiRes();
    for (auto& c : clips_) c.lastFiredStep = -1;
    lastSampleTriggerStep_ = -1;
    pendingSampleResync_ = true;
    pluginHost_.stopSamplePlaybackImmediate();
    repaint();
}

void Playlist::setPlaybackEnabled(bool enabled)
{
    if (playbackEnabled_ == enabled)
        return;

    playbackEnabled_ = enabled;
    for (auto& c : clips_) c.lastFiredStep = -1;
}

void Playlist::setBPM(double bpm)
{
    const double newBpm = juce::jlimit(20.0, 999.0, bpm);
    if (std::abs(newBpm - bpm_) < 0.001)
        return;

    bpm_ = newBpm;
    const double secondsPerBar = 240.0 / bpm_;
    for (auto& c : clips_)
    {
        if (c.kind != ClipKind::Sample || c.sourceSeconds <= 0.0)
            continue;
        if (c.manuallyTrimmed)
            continue;

        if (c.tempoSync && c.sourceBars > 0.0f)
            c.lengthBar = c.sourceBars;
        else
            c.lengthBar = juce::jmax(0.25f, (float)(c.sourceSeconds / secondsPerBar));
        c.lastFiredStep = -1;
    }
    repaint();
}

float Playlist::automationValueAt(const Clip& c, float bar) const
{
    const float rel = juce::jlimit(0.0f, 1.0f, (bar - c.startBar) / juce::jmax(0.001f, c.lengthBar));
    return c.automationStartValue + (c.automationEndValue - c.automationStartValue) * rel;
}

void Playlist::applyEffectAutomationAt(int absoluteStep)
{
    if (!onEffectAutomationValue)
        return;

    const float bar = (float)absoluteStep / 16.0f;
    for (const auto& c : clips_)
    {
        if (c.kind != ClipKind::Automation || c.automationSlotId == 0)
            continue;

        if (bar >= c.startBar && bar <= c.startBar + c.lengthBar + 0.0001f)
            onEffectAutomationValue(c.automationSlotId, automationValueAt(c, bar));
    }
}

void Playlist::setAutomationValueFromPoint(int clipIdx, int x, int y)
{
    if (clipIdx < 0 || clipIdx >= (int)clips_.size())
        return;

    auto& c = clips_[(size_t)clipIdx];
    if (c.kind != ClipKind::Automation)
        return;

    auto r = clipRect(c).reduced(6.0f, 5.0f);
    const float value = juce::jlimit(0.0f, 1.0f, (r.getBottom() - (float)y) / juce::jmax(1.0f, r.getHeight()));
    const float midX = r.getCentreX();
    if ((float)x >= midX)
    {
        c.automationEndValue = value;
        draggingAutomationEnd_ = true;
    }
    else
    {
        c.automationStartValue = value;
        draggingAutomationEnd_ = false;
    }

    applyEffectAutomationAt(absoluteStep_);
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
        // Ctrl+wheel = fast horizontal zoom. Wheel delta is small on Windows,
        // so use multiplicative zoom for a DAW-like feel.
        const float factor = std::pow(1.45f, wheel.deltaY * 8.0f);
        zoomX_ = juce::jlimit(0.18f, 12.0f, zoomX_ * factor);
        setHorizontalBarOffset(viewStartBar_);
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

    int x = gridStartX + (int)((c.startBar - viewStartBar_) * (float)bw) + CLIP_INSET_X;
    int y = tracksTopY + c.track * TRACK_H - scrollY_ + 3;
    int wpx = (int)(c.lengthBar * (float)bw) - (CLIP_INSET_X * 2);
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
    return viewStartBar_ + (float)(x - gridStartX) / (float)barW();
}

int Playlist::pixelToStep(int x) const
{
    float bar = pixelToBar(x);
    if (bar < 0.0f) return 0;
    return juce::jmax(0, (int)std::floor(bar * 16.0f));
}

int Playlist::playheadX() const
{
    const int gridStartX = patternStripW() + TRACK_LABEL_W;
    return gridStartX + CLIP_INSET_X
        + (int)((((float)absoluteStep_ / 16.0f) - viewStartBar_) * (float)barW());
}

float Playlist::maxHorizontalBarOffset() const
{
    const int gridStartX = patternStripW() + TRACK_LABEL_W;
    const float visibleBars = juce::jmax(1.0f, (float)juce::jmax(1, getWidth() - gridStartX) / (float)barW());
    float contentEndBar = 64.0f;
    for (const auto& c : clips_)
        contentEndBar = juce::jmax(contentEndBar, c.startBar + juce::jmax(0.25f, c.lengthBar));

    return juce::jmax(0.0f, contentEndBar + 2.0f - visibleBars);
}

void Playlist::setHorizontalBarOffset(float bar)
{
    viewStartBar_ = juce::jlimit(0.0f, maxHorizontalBarOffset(), bar);
}

juce::Rectangle<int> Playlist::patternToggleRect() const
{
    // 16-px wide square at top of the pattern strip (just below the header).
    return juce::Rectangle<int>(0, HEADER_H, patternStripW(), 18);
}

juce::Rectangle<int> Playlist::trimToolRect() const
{
    return juce::Rectangle<int>(168, 6, 54, 16);
}

juce::Rectangle<int> Playlist::arrangeToolRect() const
{
    const int preferredX = patternStripW() + 328;
    const int maxX = juce::jmax(228, getWidth() - 162);
    return juce::Rectangle<int>(juce::jmin(preferredX, maxX), 6, 74, 16);
}

juce::Rectangle<int> Playlist::flatHpBtnRect() const
{
    auto arrange = arrangeToolRect();
    return juce::Rectangle<int>(arrange.getRight() + 8, 6, 62, 16);
}

juce::Rectangle<int> Playlist::openAiAssistantBtnRect() const
{
    return juce::Rectangle<int>(juce::jmax(0, getWidth() - 86), 6, 78, 16);
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

int Playlist::getClipEdgeAt(int x, int y) const
{
    constexpr int edgePx = 7;
    for (int i = (int)clips_.size() - 1; i >= 0; --i)
    {
        auto r = clipRect(clips_[i]);
        if (!r.expanded((float)edgePx, 2.0f).contains((float)x, (float)y))
            continue;
        if (std::abs((float)x - r.getX()) <= (float)edgePx) return -(i + 1);
        if (std::abs((float)x - r.getRight()) <= (float)edgePx) return i + 1;
    }
    return 0;
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

void Playlist::showAutoArrangeMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader("Auto Arrange Beat");
    m.addItem(100, "Audio Producer Tag", true, gAutoArrangeProducerTagEnabled);
    m.addSeparator();
    m.addItem(1, "Boom Bap");
    m.addItem(2, "Hip Hop");
    m.addItem(3, "Trap");
    m.addItem(4, "R&B");
    m.addItem(5, "Lo-Fi");
    m.addItem(6, "Drill");
    m.addItem(7, "Afrobeat");
    m.addItem(8, "House");
    m.addSeparator();
    m.addSectionHeader("Auto Cut Pattern");
    m.addItem(200, "Cut Drum Pattern Only");
    m.addItem(201, "Cut Drums + Loops");

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                                                .withTargetScreenArea(arrangeToolRect()),
        [this](int result)
        {
            switch (result)
            {
                case 100:
                    gAutoArrangeProducerTagEnabled = !gAutoArrangeProducerTagEnabled;
                    showAutoArrangeMenu();
                    break;
                case 1: autoArrange("boom bap"); break;
                case 2: autoArrange("hip hop"); break;
                case 3: autoArrange("trap"); break;
                case 4: autoArrange("rnb"); break;
                case 5: autoArrange("lofi"); break;
                case 6: autoArrange("drill"); break;
                case 7: autoArrange("afrobeat"); break;
                case 8: autoArrange("house"); break;
                case 200: autoCutPattern(false); break;
                case 201: autoCutPattern(true); break;
                default: break;
            }
        });
}

void Playlist::autoArrange(const juce::String& genre)
{
    Clip patternTemplate;
    bool hasPattern = false;
    std::vector<Clip> sampleTemplates;
    int selectedSampleTemplate = -1;

    for (int i = 0; i < (int)clips_.size(); ++i)
    {
        const auto& c = clips_[(size_t)i];
        if (c.kind == ClipKind::Pattern && !hasPattern)
        {
            patternTemplate = c;
            hasPattern = true;
        }
        else if (c.kind == ClipKind::Sample && c.sampleFile.existsAsFile())
        {
            if (selectedSampleTemplate < 0 && selectedClips_.count(i) > 0)
                selectedSampleTemplate = (int)sampleTemplates.size();
            sampleTemplates.push_back(c);
        }
    }

    if (!sampleTemplates.empty())
    {
        Clip primaryLoop = selectedSampleTemplate >= 0
            ? sampleTemplates[(size_t)selectedSampleTemplate]
            : *std::min_element(sampleTemplates.begin(), sampleTemplates.end(),
                [](const Clip& a, const Clip& b)
                {
                    if (std::abs(a.startBar - b.startBar) > 0.001f)
                        return a.startBar < b.startBar;
                    return a.track < b.track;
                });

        primaryLoop.lastFiredStep = -1;
        sampleTemplates.clear();
        sampleTemplates.push_back(primaryLoop);
    }

    if (!hasPattern)
    {
        patternTemplate.kind = ClipKind::Pattern;
        patternTemplate.track = 0;
        patternTemplate.startBar = 0.0f;
        patternTemplate.lengthBar = defaultPatternLengthBar();
        patternTemplate.label = currentPatternName_;
        hasPattern = true;
    }

    const auto normalized = genre.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz ");
    const float patLen = juce::jmax(0.25f, patternTemplate.lengthBar);
    clips_.clear();
    selectedClips_.clear();
    editorClip_ = -1;

    auto addPatternRange = [this, &patternTemplate, patLen](float start, float end)
    {
        for (float b = start; b < end - 0.001f; b += patLen)
        {
            Clip c = patternTemplate;
            c.kind = ClipKind::Pattern;
            c.startBar = b;
            c.lengthBar = juce::jmin(patLen, end - b);
            c.lastFiredStep = -1;
            clips_.push_back(c);
        }
    };

    auto addSampleRange = [this](const Clip& src, float start, float end)
    {
        auto overlapsExistingSample = [this](int track, float newStart, float newEnd)
        {
            for (const auto& existing : clips_)
            {
                if (existing.kind != ClipKind::Sample || existing.track != track)
                    continue;

                const float existingStart = existing.startBar;
                const float existingEnd = existing.startBar + existing.lengthBar;
                if (newStart < existingEnd - 0.001f && newEnd > existingStart + 0.001f)
                    return true;
            }

            return false;
        };

        const float srcLen = juce::jmax(0.25f, src.lengthBar);
        if (srcLen >= end - start)
        {
            if (overlapsExistingSample(src.track, start, end))
                return;

            Clip c = src;
            c.startBar = start;
            c.lengthBar = end - start;
            c.trimStartBar = juce::jmax(0.0f, src.trimStartBar);
            c.lastFiredStep = -1;
            clips_.push_back(c);
            return;
        }

        for (float b = start; b < end - 0.001f; b += srcLen)
        {
            const float length = juce::jmin(srcLen, end - b);
            if (overlapsExistingSample(src.track, b, b + length))
                continue;

            Clip c = src;
            c.startBar = b;
            c.lengthBar = length;
            c.trimStartBar = juce::jmax(0.0f, src.trimStartBar);
            c.lastFiredStep = -1;
            clips_.push_back(c);
        }
    };

    auto addSamplesRange = [&sampleTemplates, &addSampleRange](float start, float end, bool thinOut = false)
    {
        for (size_t i = 0; i < sampleTemplates.size(); ++i)
        {
            if (thinOut && (i % 2) == 1)
                continue;
            addSampleRange(sampleTemplates[i], start, end);
        }
    };

    auto addProducerTag = [this]()
    {
        if (!gAutoArrangeProducerTagEnabled)
            return;

        const auto tagFile = producerTagStorageFile();
        if (!tagFile.existsAsFile())
            return;

        Clip tag;
        tag.kind = ClipKind::Sample;
        tag.track = juce::jlimit(0, juce::jmax(0, numTracks_ - 1), 1);
        tag.startBar = 0.0f;
        tag.label = "Producer Tag - LVNH";
        configureSampleClip(tag, tagFile);

        const float tagLen = juce::jmax(0.25f, tag.lengthBar);
        tag.startBar = 0.0f;
        clips_.push_back(tag);

        if (tagLen <= 2.0f)
        {
            Clip preDrop = tag;
            preDrop.startBar = 4.0f - tagLen;
            preDrop.lastFiredStep = -1;
            clips_.push_back(preDrop);
        }
    };

    if (normalized.contains("trap") || normalized.contains("drill"))
    {
        addSamplesRange(0.0f, 4.0f, true);
        addPatternRange(4.0f, 20.0f);
        addSamplesRange(4.0f, 20.0f);
        addPatternRange(20.0f, 36.0f);
        addSamplesRange(20.0f, 36.0f);
        addSamplesRange(36.0f, 40.0f, true);
        addPatternRange(40.0f, 56.0f);
        addSamplesRange(40.0f, 56.0f);
        addSamplesRange(56.0f, 60.0f, true);
    }
    else if (normalized.contains("rnb") || normalized.contains("lofi"))
    {
        addSamplesRange(0.0f, 8.0f, true);
        addPatternRange(8.0f, 24.0f);
        addSamplesRange(8.0f, 24.0f);
        addPatternRange(24.0f, 32.0f);
        addSamplesRange(24.0f, 32.0f, true);
        addPatternRange(32.0f, 48.0f);
        addSamplesRange(32.0f, 48.0f);
        addSamplesRange(48.0f, 56.0f, true);
    }
    else if (normalized.contains("house") || normalized.contains("afrobeat"))
    {
        addSamplesRange(0.0f, 8.0f, true);
        addPatternRange(8.0f, 24.0f);
        addSamplesRange(8.0f, 24.0f);
        addPatternRange(24.0f, 40.0f);
        addSamplesRange(24.0f, 40.0f);
        addSamplesRange(40.0f, 48.0f, true);
        addPatternRange(48.0f, 64.0f);
        addSamplesRange(48.0f, 64.0f);
    }
    else
    {
        addSamplesRange(0.0f, 4.0f, true);
        addPatternRange(4.0f, 20.0f);
        addSamplesRange(4.0f, 20.0f);
        addPatternRange(20.0f, 28.0f);
        addSamplesRange(20.0f, 28.0f);
        addSamplesRange(28.0f, 32.0f, true);
        addPatternRange(32.0f, 48.0f);
        addSamplesRange(32.0f, 48.0f);
    }

    addProducerTag();
    repaint();
    notifyClipsChanged();
}

void Playlist::autoCutPattern(bool includeLoops)
{
    const float endBar = getContentEndBar();
    if (endBar <= 4.0f)
        return;

    struct CutGap
    {
        float start = 0.0f;
        float end = 0.0f;
    };

    std::vector<CutGap> gaps;
    auto addGapBefore = [&gaps, endBar](float markerBar, float lengthBar)
    {
        if (markerBar <= 0.0f || markerBar >= endBar - 0.001f)
            return;

        CutGap gap;
        gap.end = markerBar;
        gap.start = juce::jmax(0.0f, markerBar - lengthBar);
        if (gap.end - gap.start >= 0.0625f)
            gaps.push_back(gap);
    };

    for (float marker = 8.0f; marker < endBar - 0.001f; marker += 8.0f)
    {
        const int phrase = (int)std::round(marker / 8.0f);
        addGapBefore(marker, (phrase % 2 == 0) ? 0.5f : 0.25f);
    }

    // A shorter pickup silence halfway through longer sections keeps the cut musical.
    for (float marker = 12.0f; marker < endBar - 0.001f; marker += 16.0f)
        addGapBefore(marker, 0.25f);

    if (gaps.empty())
        return;

    std::sort(gaps.begin(), gaps.end(), [](const CutGap& a, const CutGap& b)
    {
        return a.start < b.start;
    });

    std::vector<Clip> updated;
    updated.reserve(clips_.size() + gaps.size());

    for (const auto& clip : clips_)
    {
        const bool shouldCut = clip.kind == ClipKind::Pattern
                            || (includeLoops && clip.kind == ClipKind::Sample);
        if (!shouldCut)
        {
            updated.push_back(clip);
            continue;
        }

        const float clipStart = clip.startBar;
        const float clipEnd = clip.startBar + clip.lengthBar;
        float segmentStart = clipStart;
        bool madeCut = false;

        for (const auto& gap : gaps)
        {
            if (gap.end <= clipStart + 0.001f || gap.start >= clipEnd - 0.001f)
                continue;

            const float gapStart = juce::jlimit(clipStart, clipEnd, gap.start);
            const float gapEnd = juce::jlimit(clipStart, clipEnd, gap.end);
            if (gapEnd <= gapStart + 0.001f)
                continue;

            if (gapStart > segmentStart + 0.001f)
            {
                Clip left = clip;
                left.startBar = segmentStart;
                left.lengthBar = gapStart - segmentStart;
                left.trimStartBar = juce::jmax(0.0f, clip.trimStartBar + (segmentStart - clipStart));
                left.manuallyTrimmed = true;
                left.lastFiredStep = -1;
                updated.push_back(left);
            }

            segmentStart = gapEnd;
            madeCut = true;
        }

        if (madeCut)
        {
            if (segmentStart < clipEnd - 0.001f)
            {
                Clip right = clip;
                right.startBar = segmentStart;
                right.lengthBar = clipEnd - segmentStart;
                right.trimStartBar = juce::jmax(0.0f, clip.trimStartBar + (segmentStart - clipStart));
                right.manuallyTrimmed = true;
                right.lastFiredStep = -1;
                updated.push_back(right);
            }
        }
        else
        {
            updated.push_back(clip);
        }
    }

    clips_ = std::move(updated);
    selectedClips_.clear();
    editorClip_ = -1;
    repaint();
    notifyClipsChanged();
}

bool Playlist::splitClipAtBar(int clipIdx, float cutBar)
{
    if (clipIdx < 0 || clipIdx >= (int)clips_.size())
        return false;

    auto& left = clips_[(size_t)clipIdx];
    cutBar = (float)snapBars(cutBar);
    const float leftStart = left.startBar;
    const float oldEnd = left.startBar + left.lengthBar;
    if (cutBar <= leftStart || cutBar >= oldEnd)
        return false;

    Clip rightClip = left;
    rightClip.startBar = cutBar;
    rightClip.lengthBar = oldEnd - cutBar;
    rightClip.trimStartBar = juce::jmax(0.0f, left.trimStartBar + (cutBar - leftStart));
    rightClip.manuallyTrimmed = true;

    left.lengthBar = cutBar - leftStart;
    left.manuallyTrimmed = true;

    clips_.insert(clips_.begin() + clipIdx + 1, std::move(rightClip));
    selectedClips_.clear();
    selectedClips_.insert(clipIdx + 1);
    repaint();
    return true;
}

// ── Mouse interactions ────────────────────────────────────────
void Playlist::mouseDown(const juce::MouseEvent& e)
{
    if (editorClip_ >= 0)
    {
        if (editorCloseRect_.contains(e.x, e.y))
        {
            editorClip_ = -1;
            draggingClipVolume_ = false;
            repaint();
            return;
        }
        if (editorVolumeRect_.contains(e.x, e.y))
        {
            draggingClipVolume_ = true;
            setEditorVolumeFromX(e.x);
            return;
        }
        if (editorLengthResetRect_.contains(e.x, e.y)
            && editorClip_ < (int)clips_.size()
            && clips_[(size_t)editorClip_].kind == ClipKind::Sample
            && clips_[(size_t)editorClip_].sourceSeconds > 0.0)
        {
            const double secondsPerBar = 240.0 / juce::jlimit(20.0, 999.0, bpm_);
            clips_[(size_t)editorClip_].trimStartBar = 0.0f;
            clips_[(size_t)editorClip_].manuallyTrimmed = false;
            clips_[(size_t)editorClip_].lengthBar =
                clips_[(size_t)editorClip_].tempoSync && clips_[(size_t)editorClip_].sourceBars > 0.0f
                    ? clips_[(size_t)editorClip_].sourceBars
                    : juce::jmax(0.25f, (float)(clips_[(size_t)editorClip_].sourceSeconds / secondsPerBar));
            repaint();
            return;
        }
        if (editorExtractBassRect_.contains(e.x, e.y)
            && editorClip_ < (int)clips_.size()
            && clips_[(size_t)editorClip_].kind == ClipKind::Sample)
        {
            extractBassFromEditorClip();
            return;
        }
        if (editorChordifyMidiRect_.contains(e.x, e.y)
            && editorClip_ < (int)clips_.size()
            && clips_[(size_t)editorClip_].kind == ClipKind::Sample)
        {
            importChordifyMidiFromEditorClip();
            return;
        }
        if (clipEditorBounds().contains(e.x, e.y))
            return;
        editorClip_ = -1;
        repaint();
    }

    draggingClip_ = -1;
    dragMoved_    = false;
    boxSelecting_ = false;
    draggingPlayhead_ = false;
    sliceDragging_ = false;
    slicingClip_ = -1;
    panningTimeline_ = false;
    grabKeyboardFocus();

    if (trimToolRect().contains(e.x, e.y))
    {
        trimToolActive_ = !trimToolActive_;
        repaint();
        return;
    }

    if (arrangeToolRect().contains(e.x, e.y))
    {
        showAutoArrangeMenu();
        return;
    }

    if (flatHpBtnRect().contains(e.x, e.y))
    {
        if (onHeadphoneFlatToggled && isHeadphoneFlatEnabled)
        {
            bool newState = !isHeadphoneFlatEnabled();
            onHeadphoneFlatToggled(newState);
            repaint();
        }
        return;
    }

    if (openAiAssistantBtnRect().contains(e.x, e.y))
    {
        if (onOpenAIAssistant)
            onOpenAIAssistant();
        return;
    }

    const int tracksTopY = HEADER_H + RULER_H;
    const int gridStartX = patternStripW() + TRACK_LABEL_W;
    if (e.mods.isMiddleButtonDown() && e.x >= gridStartX && e.y >= HEADER_H)
    {
        panningTimeline_ = true;
        panStartX_ = e.x;
        panStartBar_ = viewStartBar_;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        return;
    }
    const bool onRuler = e.y >= HEADER_H && e.y < tracksTopY && e.x >= gridStartX;
    const bool nearPlayhead = std::abs(e.x - playheadX()) <= 6 && e.x >= gridStartX;
    if (onRuler || nearPlayhead)
    {
        draggingPlayhead_ = true;
        setAbsoluteStep(pixelToStep(e.x));
        if (onPlayheadSeek) onPlayheadSeek(absoluteStep_);
        return;
    }

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
        if (t >= 0)
        {
            if ((int)trackEnabled_.size() < numTracks_)
                trackEnabled_.resize((size_t)numTracks_, true);

            const int rowY = tracksTopY + t * TRACK_H - scrollY_;
            const auto dotRect = juce::Rectangle<int>(trackAreaX + TRACK_LABEL_W - 22,
                                                      rowY + TRACK_H / 2 - 8,
                                                      18, 16);
            if (dotRect.contains(e.x, e.y))
            {
                trackEnabled_[(size_t)t] = !trackEnabled_[(size_t)t];
                if (!trackEnabled_[(size_t)t])
                {
                    for (auto& c : clips_)
                        if (c.track == t)
                            c.lastFiredStep = -1;
                    pluginHost_.stopSamplePlaybackImmediate();
                }
                repaint();
                return;
            }

            selectedTrack_ = t;
            repaint();
        }
        return;
    }

    const bool ctrl  = e.mods.isCtrlDown();
    const bool right = e.mods.isRightButtonDown();
    const bool alt   = e.mods.isAltDown();
    const bool shift = e.mods.isShiftDown();
    int edgeHit = getClipEdgeAt(e.x, e.y);
    int hit = findClipAt(e.x, e.y);

    if ((trimToolActive_ || shift || alt) && !right && hit >= 0)
    {
        sliceDragging_ = true;
        slicingClip_ = hit;
        slicePreviewBar_ = (float)snapBars(pixelToBar(e.x));
        selectedClips_.clear();
        selectedClips_.insert(hit);
        repaint();
        return;
    }

    // Ctrl + drag (left or right mouse) on a clip → CLONE-drag (duplicate it).
    // Matches FL Studio's "hold Ctrl and drag the pattern to copy" behaviour.
    if (ctrl && hit >= 0)
    {
        Clip dup = clips_[hit];
        clips_.push_back(dup);
        int newIdx = (int)clips_.size() - 1;
        draggingClip_     = newIdx;
        clipDragMode_ = ClipDragMode::Move;
        dragGrabDeltaBar_ = clips_[newIdx].startBar - pixelToBar(e.x);
        dragStartBar_ = clips_[newIdx].startBar;
        dragStartLengthBar_ = clips_[newIdx].lengthBar;
        dragStartTrimBar_ = clips_[newIdx].trimStartBar;
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

    if (!right && edgeHit != 0)
    {
        hit = std::abs(edgeHit) - 1;
        if (!selectedClips_.count(hit))
            selectedClips_.clear();

        draggingClip_ = hit;
        clipDragMode_ = edgeHit < 0 ? ClipDragMode::ResizeStart : ClipDragMode::ResizeEnd;
        dragStartBar_ = clips_[hit].startBar;
        dragStartLengthBar_ = clips_[hit].lengthBar;
        dragStartTrimBar_ = clips_[hit].trimStartBar;
        selectedTrack_ = clips_[hit].track;
        repaint();
        return;
    }

    if (hit >= 0)
    {
        if (clips_[(size_t)hit].kind == ClipKind::Automation)
        {
            draggingAutomationClip_ = hit;
            setAutomationValueFromPoint(hit, e.x, e.y);
            selectedClips_.clear();
            selectedClips_.insert(hit);
            return;
        }

        // Plain click on a clip that's NOT in the current selection clears it
        if (!selectedClips_.count(hit))
            selectedClips_.clear();

        // Begin drag-move
        draggingClip_     = hit;
        clipDragMode_ = ClipDragMode::Move;
        dragGrabDeltaBar_ = clips_[hit].startBar - pixelToBar(e.x);
        dragStartBar_ = clips_[hit].startBar;
        dragStartLengthBar_ = clips_[hit].lengthBar;
        dragStartTrimBar_ = clips_[hit].trimStartBar;
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
        c.lengthBar = defaultPatternLengthBar();
        c.label    = currentPatternName_;
        clips_.push_back(c);
        selectedTrack_ = t;
        draggingClip_     = (int)clips_.size() - 1;
        clipDragMode_ = ClipDragMode::Move;
        dragGrabDeltaBar_ = 0.0f;
        repaint();
    }
}

void Playlist::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int hit = findClipAt(e.x, e.y);
    if (hit >= 0 && hit < (int)clips_.size() && clips_[(size_t)hit].kind == ClipKind::Sample)
    {
        editorClip_ = hit;
        selectedClips_.clear();
        selectedClips_.insert(hit);
        repaint();
    }
}

void Playlist::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingClipVolume_)
    {
        setEditorVolumeFromX(e.x);
        return;
    }

    if (draggingPlayhead_)
    {
        setAbsoluteStep(pixelToStep(e.x));
        if (onPlayheadSeek) onPlayheadSeek(absoluteStep_);
        return;
    }

    if (panningTimeline_)
    {
        const float deltaBars = (float)(panStartX_ - e.x) / (float)barW();
        setHorizontalBarOffset(panStartBar_ + deltaBars);
        repaint();
        return;
    }

    if (draggingAutomationClip_ >= 0)
    {
        setAutomationValueFromPoint(draggingAutomationClip_, e.x, e.y);
        return;
    }

    if (sliceDragging_)
    {
        slicePreviewBar_ = (float)snapBars(juce::jmax(0.0f, pixelToBar(e.x)));
        repaint();
        return;
    }

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

    if (clipDragMode_ == ClipDragMode::ResizeEnd)
    {
        float endBar = (float)snapBars(pixelToBar(e.x));
        c.lengthBar = juce::jmax(0.25f, endBar - c.startBar);
        c.manuallyTrimmed = true;
        dragMoved_ = true;
        repaint();
        return;
    }

    if (clipDragMode_ == ClipDragMode::ResizeStart)
    {
        float newStart = (float)snapBars(juce::jmax(0.0f, pixelToBar(e.x)));
        const float oldEnd = dragStartBar_ + dragStartLengthBar_;
        newStart = juce::jmin(newStart, oldEnd - 0.25f);
        const float trimDelta = newStart - dragStartBar_;
        c.startBar = newStart;
        c.lengthBar = juce::jmax(0.25f, oldEnd - newStart);
        c.trimStartBar = juce::jmax(0.0f, dragStartTrimBar_ + trimDelta);
        c.manuallyTrimmed = true;
        dragMoved_ = true;
        repaint();
        return;
    }

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
    if (sliceDragging_)
        splitClipAtBar(slicingClip_, slicePreviewBar_);

    draggingClip_ = -1;
    clipDragMode_ = ClipDragMode::None;
    dragMoved_    = false;
    boxSelecting_ = false;
    draggingPlayhead_ = false;
    panningTimeline_ = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    draggingClipVolume_ = false;
    draggingAutomationClip_ = -1;
    sliceDragging_ = false;
    slicingClip_ = -1;
    boxRect_      = {};
    repaint();
}

void Playlist::setEditorVolumeFromX(int x)
{
    if (editorClip_ < 0 || editorClip_ >= (int)clips_.size()) return;
    auto& c = clips_[(size_t)editorClip_];
    if (c.kind != ClipKind::Sample) return;
    const float rel = (float)(x - editorVolumeRect_.getX()) / (float)juce::jmax(1, editorVolumeRect_.getWidth());
    c.volume = juce::jlimit(0.0f, 2.0f, rel * 2.0f);
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
    if (!d.description.isString())
        return false;
    const juce::String description = d.description.toString();
    return description.startsWith("pattern-channel\n")
        || description.startsWith("audio\n")
        || juce::File(description).existsAsFile();
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

void Playlist::splitPatternChannelsToTracks(const juce::String& patternName,
                                            const juce::StringArray& channelNames,
                                            int patternSteps)
{
    const juce::String cleanPattern = patternName.isNotEmpty() ? patternName : "Pattern 1";
    const int steps = juce::jlimit(16, 256, patternSteps > 0 ? patternSteps : patternDefaultSteps_);
    const float lengthBars = juce::jmax(0.25f, (float)steps / 16.0f);
    const auto splitPrefix = cleanPattern + " - ";

    clips_.erase(std::remove_if(clips_.begin(), clips_.end(),
        [&splitPrefix](const Clip& c)
        {
            return c.kind == ClipKind::Pattern
                && c.sourceChannelIndex >= 0
                && c.label.startsWith(splitPrefix);
        }),
        clips_.end());

    const int count = juce::jmin(channelNames.size(), numTracks_);
    for (int i = 0; i < count; ++i)
    {
        Clip c;
        c.kind = ClipKind::Pattern;
        c.track = i;
        c.startBar = 0.0f;
        c.lengthBar = lengthBars;
        c.label = splitPrefix + (channelNames[i].isNotEmpty() ? channelNames[i] : ("Slot " + juce::String(i + 1)));
        c.sourceChannelIndex = i;
        clips_.push_back(c);
    }

    selectedTrack_ = count > 0 ? 0 : selectedTrack_;
    repaint();
}

void Playlist::itemDropped(const SourceDetails& d)
{
    const juce::String description = d.description.toString();
    auto patternDrag = parsePatternChannelDrag(description);
    if (patternDrag.valid)
    {
        int t = pixelToTrack(d.localPosition.y);
        float b = pixelToBar(d.localPosition.x);
        dropHighlightTrack_ = -1;
        dropHighlightBar_ = -1;

        if (t >= 0 && b >= 0.0f && patternDrag.channelIndex >= 0)
        {
            Clip c;
            c.kind = ClipKind::Pattern;
            c.track = t;
            c.startBar = (float)snapBars(b);
            c.lengthBar = (float)patternDrag.patternSteps / 16.0f;
            c.label = patternDrag.patternName + " - " + patternDrag.channelName;
            c.sourceChannelIndex = patternDrag.channelIndex;
            clips_.push_back(c);
            selectedTrack_ = t;
        }
        repaint();
        return;
    }

    bool isLoopLibrary = false;
    juce::String path = parseAudioDragPath(description, &isLoopLibrary);
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
        c.label      = file.getFileNameWithoutExtension();
        configureSampleClip(c, file);
        clips_.push_back(c);
        selectedTrack_ = t;

        if (isLoopLibrary)
            requestBassExtractionForClip(clips_.back(), true);
    }
    repaint();
    notifyClipsChanged();
}

// ── External file drops (from Windows Explorer) ───────────────
void Playlist::addAudioFileFromExternalBrowserDrag(const juce::File& file, bool isLoopLibrary)
{
    if (!file.existsAsFile())
        return;

    const auto ext = file.getFileExtension().toLowerCase();
    if (ext != ".wav" && ext != ".mp3" && ext != ".flac"
        && ext != ".aif" && ext != ".aiff" && ext != ".ogg")
        return;

    const int t = juce::jlimit(0, juce::jmax(0, numTracks_ - 1), selectedTrack_);
    const float bar = (float)snapBars((float)absoluteStep_ / 16.0f);

    Clip c;
    c.kind = ClipKind::Sample;
    c.track = t;
    c.startBar = bar;
    c.label = file.getFileNameWithoutExtension();
    configureSampleClip(c, file);

    for (const auto& existing : clips_)
    {
        if (existing.kind == ClipKind::Sample
            && existing.sampleFile == c.sampleFile
            && existing.track == c.track
            && std::abs(existing.startBar - c.startBar) < 0.001f)
            return;
    }

    clips_.push_back(c);
    selectedTrack_ = t;

    if (isLoopLibrary)
        requestBassExtractionForClip(clips_.back(), true);

    repaint();
    notifyClipsChanged();
}

void Playlist::addEffectAutomationClip(int pluginSlotId, const juce::String& label, int preferredTrack)
{
    if (pluginSlotId == 0)
        return;

    Clip c;
    c.kind = ClipKind::Automation;
    c.track = juce::jlimit(0, juce::jmax(0, numTracks_ - 1), preferredTrack);
    c.startBar = (float)snapBars((float)absoluteStep_ / 16.0f);
    c.lengthBar = 4.0f;
    c.label = "Automation - " + label;
    c.automationSlotId = pluginSlotId;
    c.automationTarget = label;
    c.automationStartValue = 1.0f;
    c.automationEndValue = 0.0f;
    clips_.push_back(c);
    selectedClips_.clear();
    selectedClips_.insert((int)clips_.size() - 1);
    selectedTrack_ = c.track;
    repaint();
}

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
    for (const auto& path : files)
    {
        const juce::File f(path);
        const auto ext = f.getFileExtension().toLowerCase();
        if (f.existsAsFile() && (ext == ".mid" || ext == ".midi"))
        {
            if (onChordifyMidiDroppedFor808)
                onChordifyMidiDroppedFor808(f);
            dropHighlightTrack_ = -1;
            dropHighlightBar_   = -1;
            repaint();
            return;
        }
    }

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
        c.label      = f.getFileNameWithoutExtension();
        configureSampleClip(c, f);
        clips_.push_back(c);
        bar += juce::jmax(0.25f, c.lengthBar);
    }
    selectedTrack_ = t;
    repaint();
}

// ── Sample-clip one-shot triggering during playback ───────────
// ─── Project I/O ─────────────────────────────────────────────────────
void Playlist::configureSampleClip(Clip& c, const juce::File& file)
{
    c.sampleFile = file;
    c.sourceSeconds = 0.0;
    c.sourceBpm = 0.0;
    c.sourceBars = 0.0f;
    c.trimStartBar = 0.0f;
    c.manuallyTrimmed = false;
    c.tempoSync = false;
    c.waveformPeaks.clear();

    std::unique_ptr<juce::AudioFormatReader> reader(audioFormatManager_.createReaderFor(file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        c.lengthBar = juce::jmax(1.0f, c.lengthBar);
        return;
    }

    c.sourceSeconds = (double)reader->lengthInSamples / reader->sampleRate;
    c.sourceBpm = parseBpmFromFileName(file.getFileNameWithoutExtension());
    if (c.sourceBpm > 0.0)
    {
        const double sourceSecondsPerBar = 240.0 / c.sourceBpm;
        c.sourceBars = juce::jmax(0.25f, (float)std::round(c.sourceSeconds / sourceSecondsPerBar));
        c.tempoSync = c.sourceBars > 0.0f;
        c.lengthBar = c.tempoSync ? c.sourceBars
                                  : juce::jmax(0.25f, (float)(c.sourceSeconds / (240.0 / juce::jlimit(20.0, 999.0, bpm_))));
    }
    else
    {
        const double secondsPerBar = 240.0 / juce::jlimit(20.0, 999.0, bpm_);
        c.lengthBar = juce::jmax(0.25f, (float)(c.sourceSeconds / secondsPerBar));
    }

    constexpr int peakCount = 192;
    c.waveformPeaks.assign((size_t)peakCount, 0.0f);
    const juce::int64 totalSamples = reader->lengthInSamples;
    const int channels = juce::jlimit(1, 2, (int)reader->numChannels);

    for (int i = 0; i < peakCount; ++i)
    {
        const juce::int64 start = (totalSamples * i) / peakCount;
        const juce::int64 end = (totalSamples * (i + 1)) / peakCount;
        const int num = (int)juce::jlimit<juce::int64>(1, 8192, end - start);
        juce::AudioBuffer<float> buffer(channels, num);
        buffer.clear();
        reader->read(&buffer, 0, num, start, true, channels > 1);

        float peak = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* data = buffer.getReadPointer(ch);
            for (int s = 0; s < num; ++s)
                peak = juce::jmax(peak, std::abs(data[s]));
        }
        c.waveformPeaks[(size_t)i] = juce::jlimit(0.0f, 1.0f, peak);
    }
}

juce::var Playlist::toJson() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("numTracks",   numTracks_);
    obj->setProperty("zoomX",       zoomX_);
    obj->setProperty("patternStripCollapsed", patternStripCollapsed_);
    obj->setProperty("currentPatternName",    currentPatternName_);
    obj->setProperty("patternDefaultSteps",   patternDefaultSteps_);
    juce::Array<juce::var> enabledTracks;
    for (int i = 0; i < numTracks_; ++i)
        enabledTracks.add(i < (int)trackEnabled_.size() ? trackEnabled_[(size_t)i] : true);
    obj->setProperty("trackEnabled", enabledTracks);

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
        o->setProperty("sourceSeconds", c.sourceSeconds);
        o->setProperty("sourceBpm",   c.sourceBpm);
        o->setProperty("sourceBars",  c.sourceBars);
        o->setProperty("trimStartBar", c.trimStartBar);
        o->setProperty("manuallyTrimmed", c.manuallyTrimmed);
        o->setProperty("tempoSync",   c.tempoSync);
        o->setProperty("volume",     c.volume);
        o->setProperty("sourceChannelIndex", c.sourceChannelIndex);
        o->setProperty("loopMixerTrack", c.loopMixerTrack);
        o->setProperty("automationSlotId", c.automationSlotId);
        o->setProperty("automationTarget", c.automationTarget);
        o->setProperty("automationStartValue", c.automationStartValue);
        o->setProperty("automationEndValue", c.automationEndValue);
        arr.add(juce::var(o));
    }
    obj->setProperty("clips", arr);
    return juce::var(obj);
}

void Playlist::fromJson(const juce::var& v)
{
    if (!v.isObject()) return;
    numTracks_ = (int)v.getProperty("numTracks", 30);
    trackEnabled_.assign((size_t)numTracks_, true);
    if (auto* enabledTracks = v.getProperty("trackEnabled", juce::var()).getArray())
    {
        for (int i = 0; i < numTracks_ && i < enabledTracks->size(); ++i)
            trackEnabled_[(size_t)i] = (bool)(*enabledTracks)[i];
    }
    zoomX_     = (float)(double)v.getProperty("zoomX", 1.0);
    patternStripCollapsed_ = (bool)v.getProperty("patternStripCollapsed", false);
    currentPatternName_    = v.getProperty("currentPatternName", "Pattern 1").toString();
    patternDefaultSteps_   = juce::jlimit(16, 4096, (int)v.getProperty("patternDefaultSteps", patternDefaultSteps_));

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
            const float savedLengthBar = c.lengthBar;
            c.label     = cv.getProperty("label", "Pattern 1").toString();
            juce::String path = cv.getProperty("sampleFile", "").toString();
            c.sourceSeconds = (double)cv.getProperty("sourceSeconds", 0.0);
            c.sourceBpm = (double)cv.getProperty("sourceBpm", 0.0);
            c.sourceBars = (float)(double)cv.getProperty("sourceBars", 0.0);
            c.trimStartBar = (float)(double)cv.getProperty("trimStartBar", 0.0);
            c.manuallyTrimmed = (bool)cv.getProperty("manuallyTrimmed", false);
            c.tempoSync = (bool)cv.getProperty("tempoSync", false);
            c.volume = (float)(double)cv.getProperty("volume", 1.0);
            c.sourceChannelIndex = (int)cv.getProperty("sourceChannelIndex", -1);
            c.loopMixerTrack = (int)cv.getProperty("loopMixerTrack", -1);
            c.automationSlotId = (int)cv.getProperty("automationSlotId", 0);
            c.automationTarget = cv.getProperty("automationTarget", "").toString();
            c.automationStartValue = (float)(double)cv.getProperty("automationStartValue", 1.0);
            c.automationEndValue = (float)(double)cv.getProperty("automationEndValue", 0.0);
            if (path.isNotEmpty())
            {
                c.sampleFile = juce::File(path);
                if (c.kind == ClipKind::Sample && c.sampleFile.existsAsFile())
                {
                    configureSampleClip(c, c.sampleFile);
                    c.lengthBar = savedLengthBar;
                    c.tempoSync = (bool)cv.getProperty("tempoSync", c.tempoSync);
                    c.sourceBpm = (double)cv.getProperty("sourceBpm", c.sourceBpm);
                    c.sourceBars = (float)(double)cv.getProperty("sourceBars", c.sourceBars);
                    c.trimStartBar = (float)(double)cv.getProperty("trimStartBar", c.trimStartBar);
                    c.manuallyTrimmed = (bool)cv.getProperty("manuallyTrimmed", c.manuallyTrimmed);
                }
            }
            clips_.push_back(std::move(c));
        }
    }
    scrollY_ = 0;
    repaint();
}

void Playlist::triggerSampleClipsAt(int playStep)
{
    const double secondsPerStep = (60.0 / juce::jlimit(20.0, 999.0, bpm_)) / 4.0;

    // Pass 1: start any clips that begin at this step (including back-to-back
    // segments of the same file from Auto Arrange).
    for (auto& c : clips_)
    {
        if (c.kind != ClipKind::Sample) continue;
        if (c.track >= 0 && c.track < (int)trackEnabled_.size() && !trackEnabled_[(size_t)c.track])
        {
            c.lastFiredStep = -1;
            continue;
        }

        const int clipStartStep = juce::jmax(0, (int)std::round(c.startBar * 16.0f));
        const int clipEndStep = clipStartStep + juce::jmax(1, (int)std::ceil(c.lengthBar * 16.0f));

        if (playStep < clipStartStep || playStep >= clipEndStep)
            continue;
        if (c.lastFiredStep == clipStartStep)
            continue;

        if (c.sampleFile.existsAsFile())
        {
            const double rate = (c.tempoSync && c.sourceBpm > 0.0)
                ? juce::jlimit(0.25, 4.0, bpm_ / c.sourceBpm)
                : 1.0;
            const double trimSteps = (double)juce::jmax(0.0f, c.trimStartBar) * 16.0;
            const double offsetSeconds = ((double)(playStep - clipStartStep) + trimSteps)
                                         * secondsPerStep * rate;
            // Only play until this clip's end on the timeline (critical when starting
            // from a scrubbed playhead inside the clip).
            const int stepsRemaining = juce::jmax(1, clipEndStep - playStep);
            const double maxTimelineSeconds = stepsRemaining * secondsPerStep;
            if (maxTimelineSeconds > 0.0001)
            {
                const int mixerTrack = c.loopMixerTrack >= 0 ? c.loopMixerTrack : -1;
                pluginHost_.playSampleFile(c.sampleFile, mixerTrack, offsetSeconds, c.volume, rate, maxTimelineSeconds);
            }
        }
        c.lastFiredStep = clipStartStep;
    }

    // Pass 2: allow ended clips to retrigger on the next pass. Do not call
    // stopSampleFileVoices() here — consecutive clips often share one file path,
    // and each voice already stops via its timeline duration cap.
    for (auto& c : clips_)
    {
        if (c.kind != ClipKind::Sample) continue;
        const int clipEndStep = juce::jmax(0, (int)std::round(c.startBar * 16.0f))
                              + juce::jmax(1, (int)std::ceil(c.lengthBar * 16.0f));
        if (playStep >= clipEndStep)
            c.lastFiredStep = -1;
    }
}

bool Playlist::hasPatternClipAtStep(int step) const
{
    const int s = juce::jmax(0, step);
    for (const auto& c : clips_)
    {
        if (c.kind != ClipKind::Pattern) continue;
        if (c.track >= 0 && c.track < (int)trackEnabled_.size() && !trackEnabled_[(size_t)c.track])
            continue;
        const int clipStart = juce::jmax(0, (int)std::floor(c.startBar * 16.0f));
        const int clipEnd = clipStart + juce::jmax(1, (int)std::ceil(c.lengthBar * 16.0f));
        if (s >= clipStart && s < clipEnd)
            return true;
    }
    return false;
}

int Playlist::patternLocalStepAt(int step, int patternSteps) const
{
    const int s = juce::jmax(0, step);
    juce::ignoreUnused(patternSteps);
    for (const auto& c : clips_)
    {
        if (c.kind != ClipKind::Pattern) continue;
        if (c.track >= 0 && c.track < (int)trackEnabled_.size() && !trackEnabled_[(size_t)c.track])
            continue;
        const int clipStart = juce::jmax(0, (int)std::floor(c.startBar * 16.0f));
        const int clipEnd = clipStart + juce::jmax(1, (int)std::ceil(c.lengthBar * 16.0f));
        if (s >= clipStart && s < clipEnd)
        {
            const int trimSteps = juce::jmax(0, (int)std::round(c.trimStartBar * 16.0f));
            return s - clipStart + trimSteps;
        }
    }
    return -1;
}

int Playlist::patternLocalStepForChannelAt(int step, int patternSteps, int channelIndex) const
{
    const int s = juce::jmax(0, step);
    juce::ignoreUnused(patternSteps);

    for (const auto& c : clips_)
    {
        if (c.kind != ClipKind::Pattern)
            continue;
        if (c.track >= 0 && c.track < (int)trackEnabled_.size() && !trackEnabled_[(size_t)c.track])
            continue;

        if (c.sourceChannelIndex >= 0 && c.sourceChannelIndex != channelIndex)
            continue;

        const int clipStart = juce::jmax(0, (int)std::floor(c.startBar * 16.0f));
        const int clipEnd = clipStart + juce::jmax(1, (int)std::ceil(c.lengthBar * 16.0f));
        if (s >= clipStart && s < clipEnd)
        {
            const int trimSteps = juce::jmax(0, (int)std::round(c.trimStartBar * 16.0f));
            return s - clipStart + trimSteps;
        }
    }

    return -1;
}

bool Playlist::patternAllowsChannelAtStep(int step, int channelIndex) const
{
    const int s = juce::jmax(0, step);
    bool foundPatternAtStep = false;
    for (const auto& c : clips_)
    {
        if (c.kind != ClipKind::Pattern)
            continue;
        if (c.track >= 0 && c.track < (int)trackEnabled_.size() && !trackEnabled_[(size_t)c.track])
            continue;

        const int clipStart = juce::jmax(0, (int)std::floor(c.startBar * 16.0f));
        const int clipEnd = clipStart + juce::jmax(1, (int)std::ceil(c.lengthBar * 16.0f));
        if (s < clipStart || s >= clipEnd)
            continue;

        foundPatternAtStep = true;
        if (c.sourceChannelIndex < 0 || c.sourceChannelIndex == channelIndex)
            return true;
    }

    return !foundPatternAtStep;
}

void Playlist::setPatternDefaultSteps(int steps)
{
    const int previousSteps = patternDefaultSteps_;
    const float previousLength = defaultPatternLengthBar();
    patternDefaultSteps_ = juce::jlimit(16, 4096, steps);
    const float nextLength = defaultPatternLengthBar();

    for (auto& c : clips_)
    {
        if (c.kind != ClipKind::Pattern || c.manuallyTrimmed)
            continue;
        if (std::abs(c.lengthBar - previousLength) < 0.001f
            || (previousSteps == 16 && std::abs(c.lengthBar - 1.0f) < 0.001f)
            || (patternDefaultSteps_ > 16 && std::abs(c.lengthBar - 1.0f) < 0.001f))
        {
            c.lengthBar = nextLength;
        }
    }
    repaint();
}

float Playlist::getContentEndBar() const
{
    float endBar = 0.0f;
    for (const auto& c : clips_)
        endBar = juce::jmax(endBar, c.startBar + juce::jmax(0.0f, c.lengthBar));
    return endBar;
}

bool Playlist::hasSampleClips() const
{
    for (const auto& c : clips_)
        if (c.kind == ClipKind::Sample && c.sampleFile.existsAsFile())
            return true;
    return false;
}

std::vector<Playlist::SampleClipRenderInfo> Playlist::getSampleClipRenderInfos(double bpm) const
{
    std::vector<SampleClipRenderInfo> result;
    const double safeBpm = juce::jlimit(20.0, 999.0, bpm);
    const double secondsPerStep = (60.0 / safeBpm) / 4.0;

    for (const auto& c : clips_)
    {
        if (c.kind != ClipKind::Sample || !c.sampleFile.existsAsFile())
            continue;
        if (c.track >= 0 && c.track < (int)trackEnabled_.size() && !trackEnabled_[(size_t)c.track])
            continue;

        SampleClipRenderInfo info;
        info.file = c.sampleFile;
        info.startStep = juce::jmax(0, (int)std::round(c.startBar * 16.0f));
        info.lengthSteps = juce::jmax(1, (int)std::ceil(c.lengthBar * 16.0f));
        info.playbackRate = (c.tempoSync && c.sourceBpm > 0.0)
            ? juce::jlimit(0.25, 4.0, safeBpm / c.sourceBpm)
            : 1.0;

        const double trimSteps = (double)juce::jmax(0.0f, c.trimStartBar) * 16.0;
        info.startOffsetSeconds = trimSteps * secondsPerStep * info.playbackRate;
        info.volume = c.volume;
        info.mixerTrack = c.loopMixerTrack >= 0 ? c.loopMixerTrack : -1;
        result.push_back(std::move(info));
    }

    return result;
}

std::vector<std::pair<juce::File, juce::String>> Playlist::getUniqueSampleLoopsInOrder() const
{
    std::vector<std::pair<juce::File, juce::String>> loops;
    for (const auto& c : clips_)
    {
        if (c.kind != ClipKind::Sample || !c.sampleFile.existsAsFile())
            continue;

        const auto path = c.sampleFile.getFullPathName();
        bool seen = false;
        for (const auto& existing : loops)
        {
            if (existing.first.getFullPathName() == path)
            {
                seen = true;
                break;
            }
        }
        if (!seen)
            loops.emplace_back(c.sampleFile, c.label);
    }
    return loops;
}

void Playlist::assignLoopMixerTracks(const std::function<int(const juce::File&)>& resolver)
{
    if (!resolver)
        return;

    for (auto& c : clips_)
    {
        if (c.kind == ClipKind::Sample && c.sampleFile.existsAsFile())
            c.loopMixerTrack = resolver(c.sampleFile);
    }
}

void Playlist::notifyClipsChanged()
{
    if (onClipsChanged)
        onClipsChanged();
}

float Playlist::defaultPatternLengthBar() const
{
    return (float)std::ceil((double)juce::jmax(16, patternDefaultSteps_) / 16.0);
}
