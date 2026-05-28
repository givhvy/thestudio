#include "ConsistencyPanel.h"
#include "Theme.h"
#include <cmath>
#include <map>

// ─────────────────────────────────────────────────────────────────────────────
//  Persistent storage helpers
// ─────────────────────────────────────────────────────────────────────────────

juce::File ConsistencyPanel::sessionsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                      .getChildFile("Stratum DAW")
                      .getChildFile("sessions.json");
}

juce::var ConsistencyPanel::loadJson()
{
    auto f = sessionsFile();
    if (!f.existsAsFile()) return juce::var();
    return juce::JSON::parse(f.loadFileAsString());
}

void ConsistencyPanel::saveJson(const juce::var& json)
{
    auto f = sessionsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText(juce::JSON::toString(json, true));
}

void ConsistencyPanel::recordSessionStart()
{
    auto json = loadJson();

    juce::DynamicObject::Ptr root;
    if (json.isObject())
        root = json.getDynamicObject();
    else
        root = new juce::DynamicObject();

    juce::Array<juce::var> sessions;
    if (auto* arr = root->getProperty("sessions").getArray())
        sessions = *arr;

    auto now = juce::Time::getCurrentTime();
    juce::DynamicObject::Ptr entry = new juce::DynamicObject();
    entry->setProperty("date",      now.formatted("%Y-%m-%d"));
    entry->setProperty("startTime", (double)(now.toMilliseconds() / 1000LL));
    entry->setProperty("endTime",   (double)(-1));
    sessions.add(juce::var(entry.get()));

    root->setProperty("sessions", sessions);
    saveJson(juce::var(root.get()));
}

void ConsistencyPanel::recordSessionEnd()
{
    auto json = loadJson();
    if (!json.isObject()) return;

    auto* arr = json["sessions"].getArray();
    if (!arr || arr->isEmpty()) return;

    for (int i = arr->size() - 1; i >= 0; --i)
    {
        auto& s = arr->getReference(i);
        if ((double)s["endTime"] < 0)
        {
            if (auto* obj = s.getDynamicObject())
            {
                auto now = juce::Time::getCurrentTime();
                obj->setProperty("endTime", (double)(now.toMilliseconds() / 1000LL));
            }
            break;
        }
    }

    saveJson(json);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Data aggregation
// ─────────────────────────────────────────────────────────────────────────────

std::map<juce::String, int> ConsistencyPanel::buildDayMinutesMap() const
{
    auto json = loadJson();
    std::map<juce::String, int> result;
    if (!json.isObject()) return result;

    auto* arr = json["sessions"].getArray();
    if (!arr) return result;

    auto now = juce::Time::getCurrentTime();

    for (const auto& s : *arr)
    {
        auto date      = s["date"].toString();
        if (date.isEmpty()) continue;

        double startSec = (double)s["startTime"];
        double endSec   = (double)s["endTime"];

        // Treat still-open session as ending now
        if (endSec < 0)
            endSec = (double)(now.toMilliseconds() / 1000LL);

        if (endSec <= startSec) continue;

        int minutes = (int)((endSec - startSec) / 60.0);
        result[date] += minutes;
    }

    return result;
}

int ConsistencyPanel::computeStreak(const std::map<juce::String, int>& dayMap) const
{
    if (dayMap.empty()) return 0;

    auto now = juce::Time::getCurrentTime();

    // Start streak check from today; if today has no session, try yesterday.
    juce::Time cursor = now;
    if (dayMap.find(now.formatted("%Y-%m-%d")) == dayMap.end())
    {
        cursor = now - juce::RelativeTime::days(1);
        if (dayMap.find(cursor.formatted("%Y-%m-%d")) == dayMap.end())
            return 0;
    }

    int streak = 0;
    while (true)
    {
        if (dayMap.find(cursor.formatted("%Y-%m-%d")) == dayMap.end()) break;
        ++streak;
        cursor = cursor - juce::RelativeTime::days(1);
    }
    return streak;
}

int ConsistencyPanel::countTotalSessions() const
{
    auto json = loadJson();
    if (!json.isObject()) return 0;
    auto* arr = json["sessions"].getArray();
    return arr ? arr->size() : 0;
}

double ConsistencyPanel::computeAvgTimePerDay(const std::map<juce::String, int>& dayMap) const
{
    if (dayMap.empty()) return 0.0;
    int total = 0;
    for (const auto& kv : dayMap) total += kv.second;
    return (double)total / (double)dayMap.size();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Colour scale for heatmap cells
// ─────────────────────────────────────────────────────────────────────────────

juce::Colour ConsistencyPanel::heatColour(int minutes)
{
    if (minutes <= 0)  return juce::Colour(0xff1a1a1c);
    if (minutes < 15)  return juce::Colour(0xff0e4429);
    if (minutes < 60)  return juce::Colour(0xff006d32);
    if (minutes < 120) return juce::Colour(0xff26a641);
    return                    juce::Colour(0xff39d353);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Component
// ─────────────────────────────────────────────────────────────────────────────

ConsistencyPanel::ConsistencyPanel()
{
    setOpaque(true);
}

void ConsistencyPanel::resized() {}

void ConsistencyPanel::paint(juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();

    g.fillAll(juce::Colour(0xff0d0d0f));

    auto dayMap       = buildDayMinutesMap();
    const int streak  = computeStreak(dayMap);
    const int total   = countTotalSessions();
    const double avg  = computeAvgTimePerDay(dayMap);

    // ── Layout ───────────────────────────────────────────────────────────────
    constexpr int CELL  = 14;
    constexpr int GAP   = 3;
    constexpr int STEP  = CELL + GAP;
    constexpr int COLS  = 8;   // 8 weeks
    constexpr int ROWS  = 7;   // Mon..Sun
    constexpr int GRID_W = COLS * STEP - GAP;
    constexpr int GRID_H = ROWS * STEP - GAP;

    // Total content height: flame(36) + gap(8) + num(58) + subtitle(16)
    //                     + gap(28) + heatTitle(14) + gap(4) + grid + gap(22) + stats(18)
    constexpr int contentH = 36 + 8 + 58 + 16 + 28 + 14 + 4 + GRID_H + 22 + 18;
    const int startY = juce::jmax(20, (h - contentH) / 2);

    const int flameY    = startY;
    const int numY      = startY + 44;
    const int subtitleY = numY + 60;
    const int htitleY   = subtitleY + 30;
    const int heatY     = htitleY + 18;
    const int statsY    = heatY + GRID_H + 22;

    // Day-label column is 18px wide on the left of the grid.
    const int labelW  = 18;
    const int gridX   = (w - GRID_W - labelW - 6) / 2 + labelW + 6;
    const int gridCX  = gridX + GRID_W / 2;   // horizontal center of grid

    // ── Flame icon ────────────────────────────────────────────────────────────
    {
        const float fw = 24.0f, fh = 36.0f;
        const float fx = (float)gridCX - fw * 0.5f;
        const float fy = (float)flameY;

        juce::Path flame;
        flame.startNewSubPath(fx + fw * 0.5f, fy + fh);
        flame.cubicTo(fx,              fy + fh * 0.72f,
                      fx + fw * 0.08f, fy + fh * 0.48f,
                      fx + fw * 0.32f, fy + fh * 0.32f);
        flame.cubicTo(fx + fw * 0.26f, fy + fh * 0.12f,
                      fx + fw * 0.46f, fy,
                      fx + fw * 0.5f,  fy);
        flame.cubicTo(fx + fw * 0.54f, fy,
                      fx + fw * 0.74f, fy + fh * 0.12f,
                      fx + fw * 0.68f, fy + fh * 0.32f);
        flame.cubicTo(fx + fw * 0.92f, fy + fh * 0.48f,
                      fx + fw,         fy + fh * 0.72f,
                      fx + fw * 0.5f,  fy + fh);
        flame.closeSubPath();

        juce::ColourGradient flameGrad(
            juce::Colour(0xffFFE066), fx + fw * 0.5f, fy,
            juce::Colour(0xffFF4500), fx + fw * 0.5f, fy + fh,
            false);
        flameGrad.addColour(0.45, juce::Colour(0xffFF8C00));
        g.setGradientFill(flameGrad);
        g.fillPath(flame);
    }

    // ── Streak number ─────────────────────────────────────────────────────────
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(48.0f).withStyle("Bold"));
    g.setColour(juce::Colours::white);
    g.drawText(juce::String(streak),
               juce::Rectangle<int>(gridCX - 60, numY, 120, 58),
               juce::Justification::centred);

    // "DAY STREAK" subtitle
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f));
    g.setColour(juce::Colour(0xff71717a));
    g.drawText(streak == 1 ? "DAY STREAK" : "DAY STREAK",
               juce::Rectangle<int>(gridCX - 80, subtitleY, 160, 16),
               juce::Justification::centred);

    // ── Heatmap section title ─────────────────────────────────────────────────
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(8.5f));
    g.setColour(juce::Colour(0xff3f3f46));
    g.drawText("LAST 56 DAYS", gridX, htitleY, GRID_W, 14, juce::Justification::left);

    // ── Day-of-week labels ────────────────────────────────────────────────────
    const char* dayLabels[] = { "M", "T", "W", "T", "F", "S", "S" };
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(8.0f));
    g.setColour(juce::Colour(0xff52525b));
    for (int row = 0; row < ROWS; ++row)
        g.drawText(dayLabels[row],
                   gridX - labelW - 4, heatY + row * STEP,
                   labelW, CELL,
                   juce::Justification::centredRight);

    // ── Heatmap cells ─────────────────────────────────────────────────────────
    auto now = juce::Time::getCurrentTime();

    // Convert today to Mon-based index: Mon=0 … Sun=6
    // juce getDayOfWeek(): 0=Sun, 1=Mon … 6=Sat
    const int dow       = now.getDayOfWeek();                          // 0=Sun
    const int todayRow  = (dow == 0) ? 6 : (dow - 1);                 // Mon-based

    for (int col = 0; col < COLS; ++col)
    {
        for (int row = 0; row < ROWS; ++row)
        {
            // daysAgo = 0 → today (col=COLS-1, row=todayRow)
            const int daysAgo = (COLS - 1 - col) * ROWS + (todayRow - row);

            auto cellRect = juce::Rectangle<float>(
                (float)(gridX + col * STEP),
                (float)(heatY  + row * STEP),
                (float)CELL,
                (float)CELL);

            if (daysAgo < 0)
            {
                // Future date – draw a very dark placeholder
                g.setColour(juce::Colour(0xff0d0d0f));
                g.fillRoundedRectangle(cellRect, 2.0f);
                continue;
            }

            auto day     = now - juce::RelativeTime::days(daysAgo);
            auto dateStr = day.formatted("%Y-%m-%d");

            auto it      = dayMap.find(dateStr);
            int  minutes = (it != dayMap.end()) ? it->second : 0;

            g.setColour(heatColour(minutes));
            g.fillRoundedRectangle(cellRect, 2.0f);

            // Subtle inner border
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.drawRoundedRectangle(cellRect, 2.0f, 0.5f);
        }
    }

    // Colour legend (right of grid, small)
    {
        const int legendX = gridX + GRID_W + 8;
        const int legendY = heatY + GRID_H - 4 * (CELL + 2) - 2;
        const int cs      = CELL - 2;
        const juce::Colour cols[] = {
            heatColour(0), heatColour(5), heatColour(30),
            heatColour(90), heatColour(150)
        };
        for (int i = 0; i < 5; ++i)
        {
            g.setColour(cols[i]);
            g.fillRoundedRectangle((float)legendX, (float)(legendY + i * (cs + 2)),
                                   (float)cs, (float)cs, 2.0f);
        }
    }

    // ── Stats row ─────────────────────────────────────────────────────────────
    juce::String avgStr;
    if (avg < 60.0)
        avgStr = juce::String((int)std::round(avg)) + " min/day avg";
    else
        avgStr = juce::String(avg / 60.0, 1) + " hr/day avg";

    juce::String statsLine = juce::String(total) + " total sessions   |   " + avgStr;

    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f));
    g.setColour(juce::Colour(0xff52525b));
    g.drawText(statsLine,
               juce::Rectangle<int>(0, statsY, w, 18),
               juce::Justification::centred);
}
