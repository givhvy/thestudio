#include "AIPanel.h"
#include "Theme.h"

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
    buttons_.push_back({ "empty",    "Clear All",    {} });

    addAssistantMessage("Hey — I can drop drum patterns straight into your Channel Rack.");
    addAssistantMessage("Pick a style below and I'll set up Kick / Snare / Hihat / Clap for you.");
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

void AIPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Backdrop shadow
    {
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.65f), 24, { 0, 8 });
        juce::Path p;
        p.addRoundedRectangle(bounds.reduced(2.0f), 10.0f);
        shadow.drawForPath(g, p);
    }

    // Chassis
    juce::ColourGradient chassis(juce::Colour(0xff1a1a1d), 0.0f, 0.0f,
                                 juce::Colour(0xff0c0c0e), 0.0f, bounds.getHeight(), false);
    g.setGradientFill(chassis);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colour(0xff2a2a2e));
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    // ── Header ─────────────────────────────────────────────
    auto headerRect = juce::Rectangle<int>(0, 0, getWidth(), HEADER_H);
    juce::ColourGradient hg(Theme::orange1, 0.0f, 0.0f,
                              Theme::orange3, 0.0f, (float)HEADER_H, false);
    g.setGradientFill(hg);
    g.fillRect(headerRect.withTrimmedBottom(1));
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(HEADER_H - 1, 0.0f, (float)getWidth());

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
    g.drawText(juce::String::fromUTF8("\xE2\x9A\xA1") + " AI Assistant", 12, 0,
               getWidth() - 60, HEADER_H, juce::Justification::centredLeft);

    // Close X
    closeBtnRect_ = { getWidth() - HEADER_H, 0, HEADER_H, HEADER_H };
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(15.0f).withStyle("Bold"));
    g.drawText("X", closeBtnRect_, juce::Justification::centred);

    // ── Layout: chat (top) + buttons (bottom) ──────────────
    int btnRows  = (int)((buttons_.size() + BTN_COLS - 1) / BTN_COLS);
    int btnAreaH = btnRows * (BTN_H + BTN_GAP) + BTN_GAP * 2 + 28; // +28 for label
    auto contentArea = juce::Rectangle<int>(0, HEADER_H, getWidth(),
                                              getHeight() - HEADER_H - FOOTER_H);

    auto btnArea = contentArea.removeFromBottom(btnAreaH);
    auto chatArea = contentArea.reduced(CHAT_PAD);

    // Chat background (recessed)
    {
        auto cb = chatArea.toFloat().expanded(4.0f);
        g.setColour(juce::Colour(0xff050507));
        g.fillRoundedRectangle(cb, 6.0f);
        g.setColour(juce::Colour(0xff222226));
        g.drawRoundedRectangle(cb, 6.0f, 1.0f);
    }
    drawChat(g, chatArea);

    // ── Preset buttons label ───────────────────────────────
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("DRUM PATTERN PRESETS",
               btnArea.getX() + 12, btnArea.getY(), btnArea.getWidth() - 24, 20,
               juce::Justification::centredLeft);

    // ── Preset buttons ─────────────────────────────────────
    auto gridArea = btnArea.withTrimmedTop(20).reduced(BTN_GAP, BTN_GAP);
    layoutButtons(gridArea);

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

    // Footer hint
    g.setColour(Theme::zinc600);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
    g.drawText("Patterns drop into the Channel Rack instantly.",
               0, getHeight() - FOOTER_H, getWidth(), FOOTER_H,
               juce::Justification::centred);
}

void AIPanel::layoutButtons(juce::Rectangle<int> area)
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

    // Render bottom-up so newest stays visible.
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
        if (bubbleY < area.getY()) break; // out of view

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

void AIPanel::resized() {}

void AIPanel::mouseDown(const juce::MouseEvent& e)
{
    isDraggingPanel_ = false;

    if (closeBtnRect_.contains(e.x, e.y))
    {
        if (onClose) onClose();
        return;
    }

    // Header strip (outside the close button) → start dragging the panel.
    if (e.y < HEADER_H)
    {
        dragger_.startDraggingComponent(this, e);
        isDraggingPanel_ = true;
        return;
    }

    for (auto& b : buttons_)
    {
        if (b.rect.contains(e.x, e.y))
        {
            addUserMessage("Make a " + b.label + " pattern");
            if (onPreset) onPreset(b.id, b.label);
            if (b.id == "empty")
                addAssistantMessage("Cleared all drum steps. Fresh canvas.");
            else
                addAssistantMessage("Done! Loaded a " + b.label + " pattern into Kick / Snare / Hihat / Clap.");
            return;
        }
    }
}

void AIPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingPanel_)
        dragger_.dragComponent(this, e, nullptr);
}
