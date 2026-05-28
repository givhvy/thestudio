#include "AIPanel.h"
#include "Theme.h"

namespace
{
void drawSidePanelCloseGlyph(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    area = area.reduced(5.0f);
    const float barW = juce::jmax(2.0f, area.getWidth() * 0.24f);
    auto bar = area.removeFromLeft(barW);
    g.setColour(colour);
    g.fillRoundedRectangle(bar, 1.5f);

    auto chev = area.reduced(area.getWidth() * 0.12f, area.getHeight() * 0.18f);
    const float cx = chev.getCentreX();
    const float cy = chev.getCentreY();
    const float arm = chev.getWidth() * 0.34f;
    const float spread = chev.getHeight() * 0.42f;
    juce::Path arrow;
    arrow.startNewSubPath(cx - arm * 0.55f, cy - spread);
    arrow.lineTo(cx + arm * 0.45f, cy);
    arrow.lineTo(cx - arm * 0.55f, cy + spread);
    g.strokePath(arrow, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}
} // namespace

AIPanel::AIPanel()
{
    buttons_.push_back({ "boom_bap", "Boom Bap",   {} });
    buttons_.push_back({ "hiphop",   "Hip Hop",    {} });
    buttons_.push_back({ "trap",     "Trap",       {} });
    buttons_.push_back({ "drill",    "Drill",      {} });
    buttons_.push_back({ "house",    "House",      {} });
    buttons_.push_back({ "rnb",      "R&B",        {} });
    buttons_.push_back({ "lofi",     "Lo-Fi",      {} });
    buttons_.push_back({ "rock",     "Rock",         {} });
    buttons_.push_back({ "detroit",  "Detroit Flint",{} });
    buttons_.push_back({ "afrobeat", "Afrobeat",   {} });
    buttons_.push_back({ "reggaeton","Reggaeton",  {} });
    buttons_.push_back({ "jersey",   "Jersey Club",{} });
    buttons_.push_back({ "ukg",      "UK Garage",  {} });
    buttons_.push_back({ "dnb",      "Drum & Bass",{} });
    buttons_.push_back({ "techno",   "Techno",     {} });
    buttons_.push_back({ "phonk",    "Phonk",      {} });
    buttons_.push_back({ "memphis",  "Memphis",    {} });
    buttons_.push_back({ "funk",     "Funk",       {} });
    buttons_.push_back({ "empty",    "Clear All",    {} });

    for (const auto& p : PatternsPanel::getPatternLibrary())
    {
        if (!p.artistPattern)
            continue;
        artistLibrary_.push_back(p);
        if (!artists_.contains(p.genre))
            artists_.add(p.genre);
    }
    if (!artists_.isEmpty())
        selectedArtist_ = artists_[0];
    rebuildArtistPatternRows();

    addAssistantMessage("Hey - I can drop drum patterns straight into your Channel Rack.");
    addAssistantMessage("Use Presets for genres, or Artist for producer-inspired patterns.");
}

void AIPanel::addUserMessage(const juce::String& text)
{
    chat_.push_back({ true, text });
    repaint();
}

void AIPanel::addAssistantMessage(const juce::String& text)
{
    chat_.push_back({ false, text });
    repaint();
}

void AIPanel::rebuildArtistPatternRows()
{
    artistPatternRows_.clear();
    for (const auto& p : artistLibrary_)
    {
        if (p.genre == selectedArtist_)
            artistPatternRows_.push_back({ p, {} });
    }
    artistPatternScrollY_ = 0;
}

std::vector<int> AIPanel::visibleArtistPatternIndices() const
{
    std::vector<int> ids;
    for (int i = 0; i < (int)artistPatternRows_.size(); ++i)
        ids.push_back(i);
    return ids;
}

void AIPanel::drawTabs(juce::Graphics& g, juce::Rectangle<int> tabsArea)
{
    auto tabs = tabsArea;
    presetsTabRect_ = tabs.removeFromLeft(96).reduced(0, 4);
    artistTabRect_ = tabs.removeFromLeft(96).reduced(6, 4);

    auto drawTab = [&](juce::Rectangle<int> r, const juce::String& label, bool selected)
    {
        g.setColour(selected ? Theme::orange1.withAlpha(0.95f) : juce::Colour(0xff1c1c20));
        g.fillRoundedRectangle(r.toFloat(), 5.0f);
        g.setColour(selected ? juce::Colours::black.withAlpha(0.7f) : juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 5.0f, 1.0f);
        g.setColour(selected ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(label, r, juce::Justification::centred);
    };

    drawTab(presetsTabRect_, "PRESETS", activeTab_ == 0);
    drawTab(artistTabRect_, "ARTIST", activeTab_ == 1);
}

void AIPanel::drawPresetBrowser(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("DRUM PATTERN PRESETS",
               area.getX() + 4, area.getY(), area.getWidth() - 8, 18,
               juce::Justification::centredLeft);

    auto gridArea = area.withTrimmedTop(20).reduced(BTN_GAP, BTN_GAP);
    layoutPresetButtons(gridArea);

    for (auto& b : buttons_)
    {
        auto fr = b.rect.toFloat();
        juce::ColourGradient grad(juce::Colour(0xff2e2e34), 0.0f, fr.getY(),
                                  juce::Colour(0xff141416), 0.0f, fr.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fr, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawHorizontalLine((int)fr.getY() + 1, fr.getX() + 4, fr.getRight() - 4);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(fr, 4.0f, 1.0f);

        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
        g.drawText(b.label, b.rect, juce::Justification::centred);
    }
}

void AIPanel::drawArtistBrowser(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto left = area.removeFromLeft(ARTIST_LIST_W);
    auto right = area.withTrimmedLeft(8);

    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("ARTISTS", left.removeFromTop(18), juce::Justification::centredLeft);
    g.drawText("ARTIST PATTERNS", right.removeFromTop(18), juce::Justification::centredLeft);

    auto artistList = left.reduced(0, 2);
    for (int i = 0; i < artists_.size() && i < 24; ++i)
    {
        auto r = artistList.removeFromTop(26).reduced(0, 2);
        artistRects_[i] = r;
        const bool selected = artists_[i] == selectedArtist_;
        g.setColour(selected ? Theme::orange1.withAlpha(0.95f) : juce::Colour(0xff1c1c20));
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
        g.setColour(selected ? juce::Colours::black.withAlpha(0.65f) : juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 4.0f, 1.0f);
        g.setColour(selected ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
        g.drawText(artists_[i], r.reduced(6, 0), juce::Justification::centredLeft, true);
    }

    auto listArea = right.reduced(0, 2);
    g.setColour(juce::Colour(0xff050507));
    g.fillRoundedRectangle(listArea.toFloat(), 5.0f);
    g.setColour(juce::Colour(0xff222226));
    g.drawRoundedRectangle(listArea.toFloat(), 5.0f, 1.0f);

    g.saveState();
    g.reduceClipRegion(listArea);

    int y = listArea.getY() - artistPatternScrollY_;
    const auto visible = visibleArtistPatternIndices();
    for (int n = 0; n < (int)visible.size(); ++n)
    {
        auto& row = artistPatternRows_[(size_t)visible[(size_t)n]];
        auto r = juce::Rectangle<int>(listArea.getX() + 6, y + 4,
                                      listArea.getWidth() - 12, ARTIST_PATTERN_ROW_H - 6);
        row.rect = r;

        g.setColour(juce::Colour(0xff19191d));
        g.fillRoundedRectangle(r.toFloat(), 5.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 5.0f, 1.0f);

        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
        g.drawText(row.def.title, r.getX() + 10, r.getY() + 6, r.getWidth() - 90, 16,
                   juce::Justification::centredLeft);
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
        g.drawText(row.def.feel, r.getX() + 10, r.getY() + 24, r.getWidth() - 90, 14,
                   juce::Justification::centredLeft, true);
        g.setColour(Theme::orange2);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(juce::String(row.def.bpm) + " BPM", r.getRight() - 78, r.getCentreY() - 9, 70, 18,
                   juce::Justification::centredRight);

        y += ARTIST_PATTERN_ROW_H;
    }

    g.restoreState();
}

void AIPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    {
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.65f), 24, { 0, 8 });
        juce::Path p;
        p.addRoundedRectangle(bounds.reduced(2.0f), 10.0f);
        shadow.drawForPath(g, p);
    }

    juce::ColourGradient chassis(juce::Colour(0xff1a1a1d), 0.0f, 0.0f,
                                 juce::Colour(0xff0c0c0e), 0.0f, bounds.getHeight(), false);
    g.setGradientFill(chassis);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colour(0xff2a2a2e));
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    auto headerRect = juce::Rectangle<int>(0, 0, getWidth(), HEADER_H);
    juce::ColourGradient hg(Theme::orange1, 0.0f, 0.0f,
                              Theme::orange3, 0.0f, (float)HEADER_H, false);
    g.setGradientFill(hg);
    g.fillRect(headerRect.withTrimmedBottom(1));
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(HEADER_H - 1, 0.0f, (float)getWidth());

    sidePanelCloseBtnRect_ = { 0, 0, HEADER_H, HEADER_H };
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRect(sidePanelCloseBtnRect_.withTrimmedTop(4).withTrimmedBottom(4)
                   .withWidth(1).translated(sidePanelCloseBtnRect_.getRight() - 1, 0));
    drawSidePanelCloseIcon(g, sidePanelCloseBtnRect_);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
    g.drawText("AI Assistant", sidePanelCloseBtnRect_.getRight() + 6, 0,
               getWidth() - HEADER_H - sidePanelCloseBtnRect_.getRight() - 10, HEADER_H,
               juce::Justification::centredLeft);

    closeBtnRect_ = { getWidth() - HEADER_H, 0, HEADER_H, HEADER_H };
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(15.0f).withStyle("Bold"));
    g.drawText("X", closeBtnRect_, juce::Justification::centred);

    int btnRows  = (int)((buttons_.size() + BTN_COLS - 1) / BTN_COLS);
    int presetAreaH = btnRows * (BTN_H + BTN_GAP) + BTN_GAP * 2 + 48;
    int artistAreaH = juce::jmax(220, (int)artistPatternRows_.size() * ARTIST_PATTERN_ROW_H + 56);
    int browserH = activeTab_ == 0 ? presetAreaH : artistAreaH;
    browserH = juce::jmin(browserH, getHeight() - HEADER_H - FOOTER_H - TAB_BAR_H - 96);

    auto contentArea = juce::Rectangle<int>(0, HEADER_H, getWidth(),
                                              getHeight() - HEADER_H - FOOTER_H);
    auto browserBlock = contentArea.removeFromBottom(browserH + TAB_BAR_H);
    auto chatArea = contentArea.reduced(CHAT_PAD);

    {
        auto cb = chatArea.toFloat().expanded(4.0f);
        g.setColour(juce::Colour(0xff050507));
        g.fillRoundedRectangle(cb, 6.0f);
        g.setColour(juce::Colour(0xff222226));
        g.drawRoundedRectangle(cb, 6.0f, 1.0f);
    }
    drawChat(g, chatArea);

    auto tabsArea = browserBlock.removeFromTop(TAB_BAR_H).reduced(8, 4);
    drawTabs(g, tabsArea);

    browserAreaRect_ = browserBlock.reduced(8, 0);
    if (activeTab_ == 0)
        drawPresetBrowser(g, browserAreaRect_);
    else
        drawArtistBrowser(g, browserAreaRect_);

    g.setColour(Theme::zinc600);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
    g.drawText(activeTab_ == 1
                   ? "Artist patterns use the same library as the Patterns panel."
                   : "Genre presets drop a full kit into the Channel Rack.",
               0, getHeight() - FOOTER_H, getWidth(), FOOTER_H,
               juce::Justification::centred);
}

void AIPanel::layoutPresetButtons(juce::Rectangle<int> area)
{
    int cols = BTN_COLS;
    int colW = (area.getWidth() - (cols - 1) * BTN_GAP) / cols;
    int x = area.getX();
    int y = area.getY();
    int col = 0;

    for (auto& b : buttons_)
    {
        b.rect = juce::Rectangle<int>(x, y, colW, BTN_H);
        if (++col >= cols)
        {
            col = 0;
            x = area.getX();
            y += BTN_H + BTN_GAP;
        }
        else
        {
            x += colW + BTN_GAP;
        }
    }
}

void AIPanel::drawChat(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);

    juce::Font userFont = juce::Font(juce::FontOptions().withName("Segoe UI").withHeight(11.5f));
    juce::Font asstFont = juce::Font(juce::FontOptions().withName("Segoe UI").withHeight(11.5f));

    int y = area.getBottom() - 6;
    for (auto it = chat_.rbegin(); it != chat_.rend(); ++it)
    {
        auto& line = *it;
        const auto& font = line.fromUser ? userFont : asstFont;
        int bubbleW  = juce::jmin(area.getWidth() - 24, 320);
        int textW    = bubbleW - 16;

        juce::AttributedString as;
        as.setJustification(juce::Justification::topLeft);
        as.append(line.text, font, line.fromUser ? juce::Colour(0xff111111) : Theme::zinc200);

        juce::TextLayout tl;
        tl.createLayout(as, (float)textW);
        int bubbleH = (int)std::ceil(tl.getHeight()) + 12;

        int bubbleX = line.fromUser ? area.getRight() - bubbleW - 4 : area.getX() + 4;
        int bubbleY = y - bubbleH;
        if (bubbleY < area.getY()) break;

        juce::Rectangle<float> bubble((float)bubbleX, (float)bubbleY, (float)bubbleW, (float)bubbleH);
        if (line.fromUser)
        {
            juce::ColourGradient g2(Theme::orange1, 0.0f, bubble.getY(),
                                      Theme::orange3, 0.0f, bubble.getBottom(), false);
            g.setGradientFill(g2);
        }
        else
        {
            g.setColour(juce::Colour(0xff1c1c20));
        }
        g.fillRoundedRectangle(bubble, 6.0f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(bubble, 6.0f, 1.0f);

        tl.draw(g, bubble.reduced(8.0f, 6.0f));
        y = bubbleY - 6;
    }

    g.restoreState();
}

void AIPanel::drawSidePanelCloseIcon(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    drawSidePanelCloseGlyph(g, bounds.toFloat(), juce::Colours::white.withAlpha(0.95f));
}

void AIPanel::resized() {}

void AIPanel::mouseDown(const juce::MouseEvent& e)
{
    isDraggingPanel_ = false;

    if (sidePanelCloseBtnRect_.contains(e.x, e.y) || closeBtnRect_.contains(e.x, e.y))
    {
        if (onClose) onClose();
        return;
    }

    if (e.y < HEADER_H)
    {
        dragger_.startDraggingComponent(this, e);
        isDraggingPanel_ = true;
        return;
    }

    if (presetsTabRect_.contains(e.x, e.y) || artistTabRect_.contains(e.x, e.y))
    {
        activeTab_ = artistTabRect_.contains(e.x, e.y) ? 1 : 0;
        repaint();
        return;
    }

    if (activeTab_ == 1)
    {
        for (int i = 0; i < artists_.size() && i < 24; ++i)
        {
            if (artistRects_[i].contains(e.x, e.y))
            {
                selectedArtist_ = artists_[i];
                rebuildArtistPatternRows();
                repaint();
                return;
            }
        }

        for (auto& row : artistPatternRows_)
        {
            if (row.rect.contains(e.x, e.y))
            {
                addUserMessage("Load " + row.def.genre + " - " + row.def.title);
                if (onPatternVariant)
                    onPatternVariant(row.def);
                addAssistantMessage("Done! Loaded " + row.def.title
                    + " (" + juce::String(row.def.bpm) + " BPM).");
                return;
            }
        }
        return;
    }

    for (auto& b : buttons_)
    {
        if (b.rect.contains(e.x, e.y))
        {
            if (e.mods.isMiddleButtonDown() && b.id != "empty")
            {
                addUserMessage("Change " + b.label + " drum sounds");
                if (b.id == "boom_bap" && onRerollSounds && onRerollSounds(b.id, b.label))
                    addAssistantMessage("Done! Changed the " + b.label + " drum kit sounds.");
                else
                    addAssistantMessage("Sound changes are wired for Boom Bap first.");
                return;
            }

            if (e.mods.isRightButtonDown() && b.id != "empty")
            {
                auto variants = PatternsPanel::getPatternsForPreset(b.id);
                if (variants.empty())
                    return;

                juce::PopupMenu menu;
                for (int i = 0; i < (int)variants.size(); ++i)
                {
                    const auto& pat = variants[(size_t)i];
                    juce::String label = pat.title;
                    if (pat.feel.isNotEmpty())
                        label += " - " + pat.feel;
                    menu.addItem(i + 1, label);
                }

                menu.showMenuAsync(juce::PopupMenu::Options{}
                    .withTargetComponent(this)
                    .withTargetScreenArea(localAreaToGlobal(b.rect)),
                    [this, variants, label = b.label](int result) {
                        if (result <= 0 || result > (int)variants.size())
                            return;

                        const auto& pattern = variants[(size_t)result - 1];
                        addUserMessage("Change " + label + " to " + pattern.title);
                        if (onPatternVariant)
                            onPatternVariant(pattern);
                        addAssistantMessage("Done! Changed to " + pattern.title + ".");
                    });
                return;
            }

            addUserMessage("Make a " + b.label + " pattern");
            if (onPreset) onPreset(b.id, b.label);
            if (b.id == "empty")
                addAssistantMessage("Cleared all drum steps. Fresh canvas.");
            else
                addAssistantMessage("Done! Loaded a " + b.label + " drum pattern.");
            return;
        }
    }
}

void AIPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingPanel_)
        dragger_.dragComponent(this, e, nullptr);
}

void AIPanel::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (activeTab_ != 1 || artistPatternRows_.empty())
    {
        juce::Component::mouseWheelMove(e, wheel);
        return;
    }

    auto listArea = browserAreaRect_;
    listArea.removeFromLeft(ARTIST_LIST_W + 8);
    listArea.removeFromTop(18);

    if (!listArea.contains(e.getPosition()))
    {
        juce::Component::mouseWheelMove(e, wheel);
        return;
    }

    const int contentH = (int)artistPatternRows_.size() * ARTIST_PATTERN_ROW_H;
    const int maxScroll = juce::jmax(0, contentH - listArea.getHeight());
    artistPatternScrollY_ = juce::jlimit(0, maxScroll,
        artistPatternScrollY_ - (int)std::round(wheel.deltaY * 40.0f));
    repaint();
}
