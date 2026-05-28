#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// Custom LookAndFeel that re-skins JUCE's built-in widgets (popup menus,
// tooltips, alert windows, scrollbars) so they match the dark / orange
// skeuomorphic theme used everywhere else in the app.
class StratumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StratumLookAndFeel()
    {
        // Popup menu palette
        setColour(juce::PopupMenu::backgroundColourId,         Theme::zinc900);
        setColour(juce::PopupMenu::textColourId,               Theme::zinc200);
        setColour(juce::PopupMenu::headerTextColourId,         Theme::orange1);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::orange3);
        setColour(juce::PopupMenu::highlightedTextColourId,    juce::Colours::white);

        // Alert / Combo / Label baseline so anything that pops up looks at home
        setColour(juce::AlertWindow::backgroundColourId,       Theme::zinc850);
        setColour(juce::AlertWindow::textColourId,             Theme::zinc200);
        setColour(juce::AlertWindow::outlineColourId,          Theme::zinc700);

        setColour(juce::ComboBox::backgroundColourId,          Theme::zinc850);
        setColour(juce::ComboBox::outlineColourId,             Theme::zinc700);
        setColour(juce::ComboBox::textColourId,                Theme::zinc200);
        setColour(juce::ComboBox::arrowColourId,               Theme::zinc500);

        setColour(juce::Label::textColourId,                   Theme::zinc200);
        setColour(juce::TextEditor::backgroundColourId,        Theme::zinc850);
        setColour(juce::TextEditor::textColourId,              Theme::zinc200);
        setColour(juce::TextEditor::outlineColourId,           Theme::zinc700);
        setColour(juce::TextEditor::focusedOutlineColourId,    Theme::orange2);

        setColour(juce::TooltipWindow::backgroundColourId,     Theme::zinc850);
        setColour(juce::TooltipWindow::textColourId,           Theme::zinc200);
        setColour(juce::TooltipWindow::outlineColourId,        Theme::zinc700);
    }

    // ── Popup menu chrome ────────────────────────────────────────────
    void drawPopupMenuBackgroundWithOptions(juce::Graphics& g,
                                            int width, int height,
                                            const juce::PopupMenu::Options&) override
    {
        const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);

        // Drop shadow rim
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(bounds, 8.0f);

        // Body: subtle vertical gradient (zinc-850 → zinc-900), inset 1px
        juce::ColourGradient body(Theme::zinc850, 0.0f, 0.0f,
                                  Theme::zinc900, 0.0f, (float)height, false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(bounds.reduced(1.0f), 7.0f);

        // Top inner highlight (1-px shine)
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawHorizontalLine(2, 4.0f, (float)width - 4.0f);

        // Border (zinc-700)
        g.setColour(Theme::zinc700);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);
    }

    void drawPopupMenuItemWithOptions(juce::Graphics& g,
                                      const juce::Rectangle<int>& area,
                                      bool                        highlighted,
                                      const juce::PopupMenu::Item& item,
                                      const juce::PopupMenu::Options&) override
    {
        if (item.isSeparator)
        {
            auto sep = area.toFloat().reduced(8.0f, 0.0f);
            g.setColour(Theme::zinc700);
            g.drawHorizontalLine((int)sep.getCentreY(), sep.getX(), sep.getRight());
            return;
        }

        const bool   enabled = item.isEnabled;
        const auto   bounds  = area.toFloat();

        // Hover / highlight band — orange gradient with subtle top sheen
        if (highlighted && enabled)
        {
            juce::ColourGradient hl(Theme::orange2, 0.0f, bounds.getY(),
                                    Theme::orange3, 0.0f, bounds.getBottom(), false);
            g.setGradientFill(hl);
            g.fillRect(bounds.reduced(2.0f, 0.0f));
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.drawHorizontalLine((int)bounds.getY() + 1,
                                  bounds.getX() + 4.0f, bounds.getRight() - 4.0f);
        }

        // Tick mark for ticked items (12-px square on the left)
        const int tickW = 18;
        if (item.isTicked)
        {
            const float cx = (float)area.getX() + 10.0f;
            const float cy = (float)area.getCentreY();
            juce::Path tick;
            tick.startNewSubPath(cx - 4.0f, cy);
            tick.lineTo(cx - 1.0f, cy + 3.0f);
            tick.lineTo(cx + 4.0f, cy - 3.0f);
            g.setColour(highlighted ? juce::Colours::white : Theme::orange1);
            g.strokePath(tick, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }

        // Label text
        auto textBounds = area.withTrimmedLeft(tickW + 6).withTrimmedRight(8);
        g.setFont(getPopupMenuFont());
        if (highlighted && enabled)
            g.setColour(juce::Colours::white);
        else if (enabled)
            g.setColour(Theme::zinc200);
        else
            g.setColour(Theme::zinc600);

        g.drawText(item.text, textBounds, juce::Justification::centredLeft, true);

        // Sub-menu arrow
        if (item.subMenu != nullptr)
        {
            const float ax = (float)area.getRight() - 12.0f;
            const float ay = (float)area.getCentreY();
            juce::Path arrow;
            arrow.addTriangle(ax - 3.0f, ay - 4.0f,
                              ax - 3.0f, ay + 4.0f,
                              ax + 2.0f, ay);
            g.setColour(highlighted ? juce::Colours::white : Theme::zinc500);
            g.fillPath(arrow);
        }

        // Shortcut text (right-aligned)
        if (item.shortcutKeyDescription.isNotEmpty())
        {
            g.setFont(getPopupMenuFont().withHeight(getPopupMenuFont().getHeight() - 1.0f));
            g.setColour(Theme::zinc500);
            g.drawText(item.shortcutKeyDescription,
                       area.withTrimmedRight(20),
                       juce::Justification::centredRight, true);
        }
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font(juce::FontOptions().withName("Segoe UI").withHeight(12.5f));
    }

    int getPopupMenuBorderSize() override { return 6; }

    void getIdealPopupMenuItemSizeWithOptions(const juce::String& text,
                                              bool                isSeparator,
                                              int                 standardMenuItemHeight,
                                              int&                idealWidth,
                                              int&                idealHeight,
                                              const juce::PopupMenu::Options& opts) override
    {
        juce::LookAndFeel_V4::getIdealPopupMenuItemSizeWithOptions(
            text, isSeparator,
            standardMenuItemHeight > 0 ? standardMenuItemHeight : 28,
            idealWidth, idealHeight, opts);
        if (!isSeparator) idealHeight = juce::jmax(idealHeight, 28);
        idealWidth += 24; // a little extra breathing room around the label
    }
};
