#include "OrgChartPanel.h"
#include "Theme.h"
#include <cmath>
#include <vector>

namespace
{
constexpr int kMinCardH = 118;
constexpr int kMaxCardH = 136;

int responsivePad(int w)
{
    return w < 680 ? 14 : (w < 1080 ? 18 : 24);
}

float titleSize(int w)
{
    return w < 680 ? 17.0f : (w < 1080 ? 18.5f : 20.0f);
}

juce::String stateLabel(AgentJobState state)
{
    switch (state)
    {
        case AgentJobState::Running: return "RUNNING";
        case AgentJobState::Success: return "DONE";
        case AgentJobState::Failed:  return "FAILED";
        default:                     return "IDLE";
    }
}

juce::Colour stateColour(AgentJobState state)
{
    switch (state)
    {
        case AgentJobState::Running: return Theme::accentBright;
        case AgentJobState::Success: return juce::Colour(0xff22c55e);
        case AgentJobState::Failed:  return juce::Colour(0xffef4444);
        default:                     return juce::Colour(0xff52525b);
    }
}

void drawPill(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool active)
{
    if (r.isEmpty())
        return;

    juce::ColourGradient bg(active ? Theme::accentBright : juce::Colours::white.withAlpha(0.10f),
                            (float)r.getX(), (float)r.getY(),
                            active ? Theme::accentDim : juce::Colour(0xff15151a),
                            (float)r.getRight(), (float)r.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(r.toFloat(), 6.0f);
    g.setColour(active ? Theme::accentBright : juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 6.0f, 1.0f);
    g.setColour(active ? juce::Colours::black : Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
    g.drawText(text, r, juce::Justification::centred, true);
}

void drawNode(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
              const juce::String& subtitle, juce::Colour accent, bool highlight)
{
    g.setColour(highlight ? accent.withAlpha(0.22f) : juce::Colour(0xff141417));
    g.fillRoundedRectangle(r, 10.0f);
    g.setColour(highlight ? accent.withAlpha(0.75f) : juce::Colour(0xff3f3f46));
    g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, 1.2f);

    g.setColour(highlight ? juce::Colours::white : Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
    g.drawText(title, r.reduced(10, 8).removeFromTop(18), juce::Justification::centred, true);

    if (subtitle.isNotEmpty())
    {
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
        g.drawText(subtitle, r.reduced(10, 26).removeFromTop(16), juce::Justification::centred, true);
    }
}

void drawAgentCard(juce::Graphics& g, juce::Rectangle<float> r, const AgentDefinition& def,
                   const AgentRuntime& rt, bool enabled,
                   juce::Rectangle<int> toggleRect, juce::Rectangle<int> runRect)
{
    const bool active = rt.state == AgentJobState::Running;
    const bool dimmed = ! enabled;
    g.setColour(dimmed ? juce::Colour(0xff0a0a0c) : (active ? Theme::accent.withAlpha(0.14f) : juce::Colour(0xff101014)));
    g.fillRoundedRectangle(r, 9.0f);
    g.setColour(dimmed ? juce::Colour(0xff1f1f24) : (active ? Theme::accent.withAlpha(0.55f) : juce::Colour(0xff27272a)));
    g.drawRoundedRectangle(r.reduced(0.5f), 9.0f, 1.0f);

    auto header = r.reduced(10, 8);
    g.setColour(dimmed ? Theme::zinc500 : Theme::zinc100);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
    g.drawText(def.name, header.removeFromTop(16), juce::Justification::centredLeft, true);

    auto pill = header.removeFromTop(16).removeFromRight(62).reduced(0, 1);
    g.setColour(stateColour(rt.state).withAlpha(0.22f));
    g.fillRoundedRectangle(pill, 5.0f);
    g.setColour(stateColour(rt.state));
    g.drawRoundedRectangle(pill.reduced(0.5f), 5.0f, 0.9f);
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(8.0f).withStyle("Bold"));
    g.drawText(stateLabel(rt.state), pill, juce::Justification::centred, true);

    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
    g.drawText(def.description, r.reduced(10, 30).removeFromTop(28), juce::Justification::topLeft, true);

    drawPill(g, toggleRect, enabled ? "ON" : "OFF", enabled);
    drawPill(g, runRect, "RUN", enabled && ! active);

    auto barArea = r.reduced(10, 8).removeFromBottom(16);
    g.setColour(juce::Colour(0xff09090b));
    g.fillRoundedRectangle(barArea, 4.0f);

    const float fillW = barArea.getWidth() * (float)rt.progress / 100.0f;
    if (fillW > 1.0f)
    {
        juce::ColourGradient pg(stateColour(rt.state), barArea.getX(), barArea.getY(),
                                stateColour(rt.state).darker(0.35f), barArea.getX(), barArea.getBottom(), false);
        g.setGradientFill(pg);
        g.fillRoundedRectangle(barArea.withWidth(fillW), 4.0f);
    }

    juce::String status = rt.statusMessage;
    if (status.isEmpty())
        status = ! enabled ? "Agent disabled" : (rt.state == AgentJobState::Idle ? "Ready — click RUN" : stateLabel(rt.state));
    if (rt.state == AgentJobState::Running)
        status = juce::String(rt.progress) + "%  " + status;

    g.setColour(Theme::zinc400);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f));
    g.drawText(status, r.reduced(10, 56).removeFromTop(14), juce::Justification::centredLeft, true);
}

void drawConnector(juce::Graphics& g, juce::Point<float> a, juce::Point<float> b)
{
    g.setColour(juce::Colour(0xff3f3f46));
    g.drawLine(a.x, a.y, b.x, b.y, 1.2f);
}
} // namespace

OrgChartPanel::LayoutMode OrgChartPanel::modeForWidth(int w)
{
    if (w >= 1080) return LayoutMode::Wide;
    if (w >= 680)  return LayoutMode::Medium;
    return LayoutMode::Narrow;
}

OrgChartPanel::AgentHitRects OrgChartPanel::makeAgentHits(juce::Rectangle<float> card)
{
    AgentHitRects hits;
    hits.card = card.toNearestInt();
    auto controls = card.reduced(10, 8).removeFromBottom(34).removeFromTop(22);
    hits.toggle = controls.removeFromLeft(juce::jmin(46.0f, controls.getWidth() * 0.42f)).toNearestInt();
    controls.removeFromLeft(6);
    hits.run = controls.removeFromLeft(juce::jmin(52.0f, controls.getWidth())).toNearestInt();
    return hits;
}

void OrgChartPanel::placeAgentCardsForDepartment(const std::vector<AgentDefinition>& defs,
                                                 const juce::String& department,
                                                 juce::Rectangle<float> deptRect,
                                                 float cardH,
                                                 float cardGap,
                                                 int cardsPerRow,
                                                 float yStart,
                                                 LayoutSnapshot& layout)
{
    std::vector<const AgentDefinition*> deptAgents;
    for (const auto& def : defs)
        if (def.department == department)
            deptAgents.push_back(&def);

    if (deptAgents.empty())
        return;

    cardsPerRow = juce::jmax(1, cardsPerRow);
    const float innerW = deptRect.getWidth();
    const float cardW = (innerW - (cardsPerRow - 1) * cardGap) / (float)cardsPerRow;

    for (size_t i = 0; i < deptAgents.size(); ++i)
    {
        const int col = (int)(i % (size_t)cardsPerRow);
        const int row = (int)(i / (size_t)cardsPerRow);
        auto card = juce::Rectangle<float>(deptRect.getX() + col * (cardW + cardGap),
                                           yStart + row * (cardH + cardGap),
                                           cardW, cardH);
        layout.agentHits[deptAgents[i]->id] = makeAgentHits(card);
    }
}

float OrgChartPanel::departmentBlockHeight(int agentCount, int cardsPerRow, float cardH, float cardGap, float deptHeaderH)
{
    cardsPerRow = juce::jmax(1, cardsPerRow);
    const int rows = (agentCount + cardsPerRow - 1) / cardsPerRow;
    return deptHeaderH + 18.0f + rows * cardH + juce::jmax(0, rows - 1) * cardGap;
}

OrgChartPanel::OrgChartPanel()
{
    startTimerHz(4);
    AgentRegistry::get().onChanged = [this]() { repaint(); };
}

void OrgChartPanel::resized()
{
    rebuildLayout();
    const int maxScroll = juce::jmax(0, layout_.contentHeight - getHeight());
    scrollY_ = juce::jlimit(0, maxScroll, scrollY_);
}

OrgChartPanel::LayoutSnapshot OrgChartPanel::buildLayout() const
{
    LayoutSnapshot layout;
    const int w = getWidth();
    layout.mode = modeForWidth(w);
    layout.pad = responsivePad(w);
    layout.headerH = w < 680 ? 88 : 62;

    const int pad = layout.pad;
    const int topY = pad + 6;
    const bool narrowHeader = w < 680;

    if (narrowHeader)
        layout.runAllRect = juce::Rectangle<int>(pad, topY + 34, juce::jmin(148, w - pad * 2), 28);
    else
        layout.runAllRect = juce::Rectangle<int>(w - pad - juce::jmin(148, w / 3), topY + 2,
                                                 juce::jmin(148, w / 3), 28);

    const float chartTop = (float)(topY + layout.headerH);
    const float contentW = (float)(w - pad * 2);
    const float centerX = (float)w * 0.5f;
    const float conductorH = 52.0f;
    const float cardGap = 10.0f;
    const float cardH = (float)juce::jlimit(kMinCardH, kMaxCardH, w < 680 ? 122 : 128);
    const float deptHeaderH = 42.0f;

    const auto& registry = AgentRegistry::get();
    const juce::StringArray departments { "Music", "Media", "Distribution" };

    auto countAgents = [&](const juce::String& dept)
    {
        int n = 0;
        for (const auto& def : registry.definitions())
            if (def.department == dept) ++n;
        return n;
    };

    if (layout.mode == LayoutMode::Wide)
    {
        const float conductorW = juce::jmin(240.0f, contentW * 0.34f);
        layout.conductorRect = { centerX - conductorW * 0.5f, chartTop, conductorW, conductorH };

        const float deptY = layout.conductorRect.getBottom() + 34.0f;
        const float deptGap = 16.0f;
        const float deptW = (contentW - deptGap * 2.0f) / 3.0f;
        float deptX = (float)pad;

        for (const auto& dept : departments)
        {
            layout.deptRects[dept] = juce::Rectangle<float>(deptX, deptY, deptW, deptHeaderH);
            deptX += deptW + deptGap;
        }

        float maxBottom = deptY + deptHeaderH;
        for (const auto& dept : departments)
        {
            const auto deptRect = layout.deptRects[dept];
            const float cardsTop = deptRect.getBottom() + 24.0f;
            placeAgentCardsForDepartment(registry.definitions(), dept, deptRect, cardH, cardGap, 1, cardsTop, layout);
            maxBottom = juce::jmax(maxBottom, cardsTop + countAgents(dept) * (cardH + cardGap));
        }

        layout.contentHeight = (int)std::ceil(maxBottom + pad);
        return layout;
    }

    if (layout.mode == LayoutMode::Medium)
    {
        const float conductorW = juce::jmin(280.0f, contentW);
        layout.conductorRect = { centerX - conductorW * 0.5f, chartTop, conductorW, conductorH };

        float y = layout.conductorRect.getBottom() + 28.0f;
        const int cardsPerRow = contentW >= 860 ? 2 : 1;

        for (const auto& dept : departments)
        {
            auto deptRect = juce::Rectangle<float>((float)pad, y, contentW, deptHeaderH);
            layout.deptRects[dept] = deptRect;
            const float cardsTop = deptRect.getBottom() + 16.0f;
            placeAgentCardsForDepartment(registry.definitions(), dept, deptRect, cardH, cardGap, cardsPerRow, cardsTop, layout);
            y += departmentBlockHeight(countAgents(dept), cardsPerRow, cardH, cardGap, deptHeaderH) + 20.0f;
        }

        layout.contentHeight = (int)std::ceil(y + pad);
        return layout;
    }

    const float conductorW = contentW;
    layout.conductorRect = { (float)pad, chartTop, conductorW, conductorH };

    float y = layout.conductorRect.getBottom() + 22.0f;
    for (const auto& dept : departments)
    {
        auto deptRect = juce::Rectangle<float>((float)pad, y, contentW, deptHeaderH);
        layout.deptRects[dept] = deptRect;
        const float cardsTop = deptRect.getBottom() + 14.0f;
        placeAgentCardsForDepartment(registry.definitions(), dept, deptRect, cardH, cardGap, 1, cardsTop, layout);
        y += departmentBlockHeight(countAgents(dept), 1, cardH, cardGap, deptHeaderH) + 16.0f;
    }

    layout.contentHeight = (int)std::ceil(y + pad);
    return layout;
}

void OrgChartPanel::rebuildLayout()
{
    layout_ = buildLayout();
}

void OrgChartPanel::timerCallback()
{
    repaint();
}

void OrgChartPanel::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused(e);
    const int maxScroll = juce::jmax(0, layout_.contentHeight - getHeight());
    scrollY_ = juce::jlimit(0, maxScroll, scrollY_ - (int)(wheel.deltaY * 48.0));
    repaint();
}

void OrgChartPanel::mouseDown(const juce::MouseEvent& e)
{
    const int y = e.y + scrollY_;

    if (layout_.runAllRect.contains(e.x, y))
    {
        if (onRunAllEnabled)
            onRunAllEnabled();
        return;
    }

    auto& registry = AgentRegistry::get();
    for (const auto& [agentId, hits] : layout_.agentHits)
    {
        if (hits.toggle.contains(e.x, y))
        {
            registry.setAgentEnabled(agentId, ! registry.isAgentEnabled(agentId));
            repaint();
            return;
        }
        if (hits.run.contains(e.x, y))
        {
            if (! registry.isAgentEnabled(agentId))
                return;
            if (registry.runtimeFor(agentId).state == AgentJobState::Running)
                return;
            if (onRunAgent)
                onRunAgent(agentId);
            return;
        }
    }
}

void OrgChartPanel::paint(juce::Graphics& g)
{
    layout_ = buildLayout();
    const int maxScroll = juce::jmax(0, layout_.contentHeight - getHeight());
    scrollY_ = juce::jlimit(0, maxScroll, scrollY_);

    g.fillAll(juce::Colour(0xff09090b));

    const int w = getWidth();
    const int pad = layout_.pad;
    const int topY = pad + 6;
    const bool narrowHeader = w < 680;

    g.saveState();
    g.reduceClipRegion(getLocalBounds());
    g.addTransform(juce::AffineTransform::translation(0.0f, -(float)scrollY_));

    g.setColour(Theme::orange2);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(titleSize(w)).withStyle("Bold"));
    g.drawText("Agent Org Chart", pad, topY, w - pad * 2, 28, juce::Justification::centredLeft);

    drawPill(g, layout_.runAllRect, "RUN ALL ON", true);

    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(w < 680 ? 9.5f : 10.5f));
    const int subtitleY = narrowHeader ? topY + 66 : topY + 30;
    g.drawText("Toggle agents ON/OFF, then RUN individually or use RUN ALL ON.",
               pad, subtitleY, w - pad * 2, narrowHeader ? 28 : 18, juce::Justification::centredLeft);

    drawNode(g, layout_.conductorRect, "STRATUM CONDUCTOR", "Orchestrates all agents", Theme::orange1, true);

    const auto& registry = AgentRegistry::get();
    const float centerX = layout_.conductorRect.getCentreX();

    if (layout_.mode == LayoutMode::Wide)
    {
        for (const auto& [dept, deptRect] : layout_.deptRects)
        {
            drawNode(g, deptRect, dept.toUpperCase(), {}, Theme::accent, false);
            drawConnector(g, { centerX, layout_.conductorRect.getBottom() }, { deptRect.getCentreX(), deptRect.getY() });
        }

        for (const auto& def : registry.definitions())
        {
            auto deptRect = layout_.deptRects[def.department];
            auto hitsIt = layout_.agentHits.find(def.id);
            if (deptRect.isEmpty() || hitsIt == layout_.agentHits.end())
                continue;

            const auto card = hitsIt->second.card.toFloat();
            drawConnector(g, { deptRect.getCentreX(), deptRect.getBottom() }, { card.getCentreX(), card.getY() });
            drawAgentCard(g, card, def, registry.runtimeFor(def.id), registry.isAgentEnabled(def.id),
                          hitsIt->second.toggle, hitsIt->second.run);
        }
    }
    else
    {
        for (const auto& [dept, deptRect] : layout_.deptRects)
        {
            drawNode(g, deptRect, dept.toUpperCase(), {}, Theme::accent, false);
            if (layout_.mode == LayoutMode::Medium)
                drawConnector(g, { centerX, layout_.conductorRect.getBottom() }, { deptRect.getCentreX(), deptRect.getY() });

            for (const auto& def : registry.definitions())
            {
                if (def.department != dept)
                    continue;

                auto hitsIt = layout_.agentHits.find(def.id);
                if (hitsIt == layout_.agentHits.end())
                    continue;

                const auto card = hitsIt->second.card.toFloat();
                if (layout_.mode == LayoutMode::Medium)
                    drawConnector(g, { deptRect.getCentreX(), deptRect.getBottom() }, { card.getCentreX(), card.getY() });

                drawAgentCard(g, card, def, registry.runtimeFor(def.id), registry.isAgentEnabled(def.id),
                              hitsIt->second.toggle, hitsIt->second.run);
            }
        }
    }

    g.restoreState();

    if (maxScroll > 0)
    {
        const float trackX = (float)w - 8.0f;
        const float trackH = (float)getHeight() - 16.0f;
        const float thumbH = juce::jmax(28.0f, trackH * (float)getHeight() / (float)layout_.contentHeight);
        const float scrollRatio = (float)scrollY_ / (float)maxScroll;
        const float thumbY = 8.0f + (trackH - thumbH) * scrollRatio;

        g.setColour(juce::Colour(0xff27272a));
        g.fillRoundedRectangle(trackX - 3.0f, 8.0f, 4.0f, trackH, 2.0f);
        g.setColour(Theme::accent.withAlpha(0.75f));
        g.fillRoundedRectangle(trackX - 3.0f, thumbY, 4.0f, thumbH, 2.0f);
    }
}
