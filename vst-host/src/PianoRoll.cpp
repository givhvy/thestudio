#include "PianoRoll.h"
#include "PluginHost.h"
#include "Theme.h"
#include "Midi808ImportSettings.h"
#include <algorithm>
#include <limits>
#include <map>

struct MidiChoice
{
    const char* id;
    const char* label;
    const char* group;
};

static const MidiChoice kMidiChoices[] = {
    { "happy", "Happy Pop", "Genre" },
    { "sad", "Sad Piano", "Genre" },
    { "jazz", "Jazz Seventh", "Genre" },
    { "dark", "Dark Minor", "Genre" },
    { "chill", "Chill Keys", "Genre" },
    { "trap", "Trap Melody", "Genre" },
    { "rnb", "R&B Chords", "Genre" },
    { "epic", "Epic Score", "Genre" },
    { "boom_bap", "Boom Bap Soul", "Genre" },
    { "house", "House Piano", "Genre" },
    { "afrobeat", "Afrobeat Pluck", "Genre" },
    { "lofi", "Lo-Fi Dust", "Genre" },
    { "arp_up", "Arp Up", "Arpeggio" },
    { "arp_down", "Arp Down", "Arpeggio" },
    { "arp_bounce", "Arp Bounce", "Arpeggio" },
    { "arp_trance", "Arp Trance", "Arpeggio" },
    { "arp_jazz", "Arp Jazz", "Arpeggio" },
    { "arp_dark", "Arp Dark", "Arpeggio" },
    { "arp_neo_soul", "Neo Soul Arp", "Arpeggio" },
    { "chord_rnb", "R&B 7/9 Chords", "Chord Progression" },
    { "chord_neosoul", "Neo Soul Changes", "Chord Progression" },
    { "chord_jazz", "Jazz ii-V-I", "Chord Progression" },
    { "chord_gospel", "Gospel Passing", "Chord Progression" },
    { "chord_pop", "Pop Progression", "Chord Progression" },
    { "chord_dark", "Dark Minor Chords", "Chord Progression" },
    { "guitar_classical", "Classical Guitar", "Guitar" },
    { "guitar_spanish", "Spanish Guitar", "Guitar" },
    { "arp_guitar", "Guitar Arpeggio", "Guitar" },
    { "drill", "Drill Bells", "Genre" },
    { "detroit", "Detroit Stab", "Genre" },
    { "reggaeton", "Reggaeton Keys", "Genre" },
    { "funk", "Funk Clav", "Genre" },
    { "memphis", "Memphis Dark", "Genre" },
    { "random", "Random Good", "Utility" }
};

static constexpr int kMidiChoiceCount = (int)(sizeof(kMidiChoices) / sizeof(kMidiChoices[0]));

static int getGeneratedMidiBpmForMood(const juce::String& mood)
{
    if (mood == "trap" || mood == "drill") return 140;
    if (mood == "memphis" || mood == "phonk") return 150;
    if (mood == "rnb" || mood == "chill") return 75;
    if (mood == "boom_bap" || mood == "lofi") return 90;
    if (mood == "jazz") return 100;
    if (mood == "house") return 124;
    if (mood == "afrobeat") return 104;
    if (mood == "reggaeton") return 96;
    if (mood == "funk" || mood == "detroit") return 102;
    if (mood == "happy") return 120;
    if (mood == "sad") return 82;
    if (mood == "epic") return 110;
    if (mood == "arp_neo_soul") return 82;
    if (mood == "chord_rnb" || mood == "chord_neosoul" || mood == "chord_gospel") return 78;
    if (mood == "chord_jazz") return 100;
    if (mood == "chord_pop") return 120;
    if (mood == "chord_dark") return 86;
    if (mood == "arp_trance") return 128;
    if (mood == "guitar_classical") return 92;
    if (mood == "guitar_spanish") return 118;
    if (mood == "arp_guitar") return 108;
    if (mood.startsWith("arp_")) return 120;
    if (mood == "random") return 100;
    return 100;
}

PianoRoll::PianoRoll(PluginHost& pluginHost) : pluginHost_(pluginHost)
{
    setWantsKeyboardFocus(true);
    // Center scroll on middle C (60)
    scrollY_ = juce::jmax(0, (HIGHEST_NOTE - 60 - 12) * KEY_H);
    
    // Sample notes (C major scale starting at C4)
    const int seedPitches[] = { 60, 62, 64, 65, 67, 69, 71, 72 };
    for (int i = 0; i < 8; ++i)
        notes_.push_back({ seedPitches[i], i * 4, 4, 100 });
}

PianoRoll::~PianoRoll() = default;

bool PianoRoll::isBlackKey(int pitch)
{
    int n = pitch % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

juce::String PianoRoll::pitchName(int pitch)
{
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    int oct = pitch / 12 - 1;
    return juce::String(names[pitch % 12]) + juce::String(oct);
}

juce::Rectangle<int> PianoRoll::getKeyboardRect() const
{
    const int bottomTrim = getVelocityLaneRect().isEmpty() ? 0 : getVelocityLaneRect().getHeight();
    return juce::Rectangle<int>(0, HEADER_H + RULER_H, KEY_W, getHeight() - HEADER_H - RULER_H - bottomTrim);
}

juce::Rectangle<int> PianoRoll::getGridRect() const
{
    const int bottomTrim = getVelocityLaneRect().isEmpty() ? 0 : getVelocityLaneRect().getHeight();
    return juce::Rectangle<int>(KEY_W, HEADER_H + RULER_H, getWidth() - KEY_W, getHeight() - HEADER_H - RULER_H - bottomTrim);
}

juce::Rectangle<int> PianoRoll::getNoteRect(const Note& n) const
{
    auto grid = getGridRect();
    int sw = stepW();
    int x = grid.getX() + n.startStep * sw - scrollX_;
    int w = n.lengthSteps * sw;
    int y = grid.getY() + (HIGHEST_NOTE - n.pitch) * KEY_H - scrollY_;
    return juce::Rectangle<int>(x, y, w, KEY_H);
}

juce::Rectangle<int> PianoRoll::getGenerateMidiButtonRect() const
{
    return juce::Rectangle<int>(320, 4, 94, HEADER_H - 8);
}

juce::Rectangle<int> PianoRoll::getRealFeelButtonRect() const
{
    return juce::Rectangle<int>(420, 4, 86, HEADER_H - 8);
}

juce::Rectangle<int> PianoRoll::getStrumButtonRect() const
{
    return juce::Rectangle<int>(512, 4, 74, HEADER_H - 8);
}

juce::Rectangle<int> PianoRoll::getHumanizeButtonRect() const
{
    return juce::Rectangle<int>(592, 4, 92, HEADER_H - 8);
}

juce::Rectangle<int> PianoRoll::getPaste808ButtonRect() const
{
    return juce::Rectangle<int>(690, 4, 128, HEADER_H - 8);
}

juce::Rectangle<int> PianoRoll::getHiHatRollButtonRect() const
{
    return juce::Rectangle<int>(214, 4, 100, HEADER_H - 8);
}

juce::Rectangle<int> PianoRoll::getSnareRollButtonRect() const
{
    return juce::Rectangle<int>(214, 4, 100, HEADER_H - 8);
}

juce::Rectangle<int> PianoRoll::getCurrentMidiStyleButtonRect() const
{
    return juce::Rectangle<int>(690, 4, 118, HEADER_H - 8);
}

juce::Rectangle<int> PianoRoll::getVelocityLaneRect() const
{
    if (getHeight() < 320)
        return {};
    return juce::Rectangle<int>(KEY_W, getHeight() - 78, getWidth() - KEY_W, 78);
}

juce::Rectangle<int> PianoRoll::getVelocityBarRect(const Note& n) const
{
    auto lane = getVelocityLaneRect();
    if (lane.isEmpty())
        return {};

    const int sw = stepW();
    const int x = lane.getX() + n.startStep * sw - scrollX_;
    const int barW = juce::jmax(4, juce::jmin(sw * juce::jmax(1, n.lengthSteps) - 2, sw * 2));
    const int usableH = lane.getHeight() - 22;
    const int barH = juce::jlimit(3, usableH, (int)std::round(usableH * (n.velocity / 127.0f)));
    return { x, lane.getBottom() - 10 - barH, barW, barH };
}

int PianoRoll::xToStep(int x) const
{
    auto grid = getGridRect();
    int sw = stepW();
    return (x - grid.getX() + scrollX_) / sw;
}

int PianoRoll::yToPitch(int y) const
{
    auto grid = getGridRect();
    int row = (y - grid.getY() + scrollY_) / KEY_H;
    return HIGHEST_NOTE - row;
}

int PianoRoll::findNoteAt(int x, int y) const
{
    for (int i = (int)notes_.size() - 1; i >= 0; --i)
    {
        if (getNoteRect(notes_[i]).contains(x, y)) return i;
    }
    return -1;
}

int PianoRoll::findVelocityBarAt(int x, int y) const
{
    auto lane = getVelocityLaneRect();
    if (lane.isEmpty() || !lane.contains(x, y))
        return -1;

    for (int i = (int)notes_.size() - 1; i >= 0; --i)
    {
        auto bar = getVelocityBarRect(notes_[(size_t)i]).expanded(3, 8);
        if (bar.contains(x, y))
            return i;
    }

    const int step = xToStep(x);
    int best = -1;
    int bestDistance = 999999;
    for (int i = 0; i < (int)notes_.size(); ++i)
    {
        const int d = std::abs(notes_[(size_t)i].startStep - step);
        if (d < bestDistance)
        {
            best = i;
            bestDistance = d;
        }
    }
    return bestDistance <= 2 ? best : -1;
}

void PianoRoll::setVelocityFromPoint(int noteIndex, int y)
{
    if (noteIndex < 0 || noteIndex >= (int)notes_.size())
        return;

    auto lane = getVelocityLaneRect();
    if (lane.isEmpty())
        return;

    const int usableH = lane.getHeight() - 22;
    const int bottom = lane.getBottom() - 10;
    const int v = juce::jlimit(1, 127, (int)std::round(((bottom - y) / (float)usableH) * 127.0f));

    if (selectedNotes_.count(noteIndex) > 0)
    {
        for (int idx : selectedNotes_)
            if (idx >= 0 && idx < (int)notes_.size())
                notes_[(size_t)idx].velocity = v;
    }
    else
    {
        notes_[(size_t)noteIndex].velocity = v;
    }

    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::paint(juce::Graphics& g)
{
    int w = getWidth();
    int h = getHeight();
    
    g.fillAll(juce::Colour(0xff09090b));
    
    // ── Header ──────────────────────────────────────────────────
    auto header = juce::Rectangle<int>(0, 0, w, HEADER_H);
    g.setColour(juce::Colour(0xff141417));
    g.fillRect(header);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(HEADER_H - 1, 0.0f, (float)w);
    
    auto dot = juce::Rectangle<float>(10, 10, 6, 6);
    Theme::drawGlowLED(g, dot, Theme::orange2, true);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
    juce::String titleText = channelName_.isEmpty() ? "PIANO ROLL" : channelName_ + " - PIANO ROLL";
    g.drawText(titleText, 22, 0, 190, HEADER_H, juce::Justification::centredLeft);

    const bool showHiHatRoll = shouldShowHiHatRollButton();
    const bool showSnareRoll = shouldShowSnareRollButton();
    if (showHiHatRoll || showSnareRoll)
    {
        auto rollRect = (showHiHatRoll ? getHiHatRollButtonRect() : getSnareRollButtonRect()).toFloat();
        juce::ColourGradient rollGrad(Theme::accentBright, rollRect.getX(), rollRect.getY(),
                                      Theme::accent.darker(0.35f), rollRect.getX(), rollRect.getBottom(), false);
        g.setGradientFill(rollGrad);
        g.fillRoundedRectangle(rollRect, 4.0f);
        g.setColour(juce::Colours::black.withAlpha(0.70f));
        g.drawRoundedRectangle(rollRect.reduced(0.5f), 4.0f, 1.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
        g.drawText(showHiHatRoll ? "HI HAT ROLL" : "SNARE ROLL",
                   rollRect.toNearestInt(), juce::Justification::centred);
    }

    auto genRect = getGenerateMidiButtonRect().toFloat();
    juce::ColourGradient genGrad(juce::Colour(0xfff97316), genRect.getX(), genRect.getY(),
                                 juce::Colour(0xff9a3412), genRect.getX(), genRect.getBottom(), false);
    g.setGradientFill(genGrad);
    g.fillRoundedRectangle(genRect, 4.0f);
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawRoundedRectangle(genRect.reduced(0.5f), 4.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
    g.drawText("GEN MIDI", genRect.toNearestInt(), juce::Justification::centred);

    auto feelRect = getRealFeelButtonRect().toFloat();
    juce::Colour feelTop = realFeelEnabled_ ? Theme::accent : juce::Colour(0xff27272a);
    juce::Colour feelBot = realFeelEnabled_ ? Theme::accent.darker(0.35f) : juce::Colour(0xff141417);
    juce::ColourGradient feelGrad(feelTop, feelRect.getX(), feelRect.getY(),
                                  feelBot, feelRect.getX(), feelRect.getBottom(), false);
    g.setGradientFill(feelGrad);
    g.fillRoundedRectangle(feelRect, 4.0f);
    g.setColour(realFeelEnabled_ ? juce::Colours::black.withAlpha(0.75f) : juce::Colours::black);
    g.drawRoundedRectangle(feelRect.reduced(0.5f), 4.0f, 1.0f);
    g.setColour(realFeelEnabled_ ? juce::Colours::black : Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
    g.drawText("REAL FEEL", feelRect.toNearestInt(), juce::Justification::centred);

    auto strumRect = getStrumButtonRect().toFloat();
    juce::ColourGradient strumGrad(juce::Colour(0xff27272a), strumRect.getX(), strumRect.getY(),
                                   juce::Colour(0xff141417), strumRect.getX(), strumRect.getBottom(), false);
    g.setGradientFill(strumGrad);
    g.fillRoundedRectangle(strumRect, 4.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(strumRect.reduced(0.5f), 4.0f, 1.0f);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
    g.drawText("STRUM", strumRect.toNearestInt(), juce::Justification::centred);

    auto humanizeRect = getHumanizeButtonRect().toFloat();
    juce::ColourGradient humanizeGrad(juce::Colour(0xff27272a), humanizeRect.getX(), humanizeRect.getY(),
                                      juce::Colour(0xff141417), humanizeRect.getX(), humanizeRect.getBottom(), false);
    g.setGradientFill(humanizeGrad);
    g.fillRoundedRectangle(humanizeRect, 4.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(humanizeRect.reduced(0.5f), 4.0f, 1.0f);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
    g.drawText("HUMANIZE", humanizeRect.toNearestInt(), juce::Justification::centred);

    if (isKickChannel_)
    {
        auto pasteRect = getPaste808ButtonRect().toFloat();
        juce::ColourGradient pasteGrad(juce::Colour(0xff2a2a2e), pasteRect.getX(), pasteRect.getY(),
                                       juce::Colour(0xff141417), pasteRect.getX(), pasteRect.getBottom(), false);
        g.setGradientFill(pasteGrad);
        g.fillRoundedRectangle(pasteRect, 4.0f);
        g.setColour(Theme::accentBright.withAlpha(0.7f));
        g.drawRoundedRectangle(pasteRect, 4.0f, 1.0f);
        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
        g.drawText("PASTE 808 MIDI", pasteRect.toNearestInt(), juce::Justification::centred);
    }

    if (currentGeneratedMidiMood_.isNotEmpty())
    {
        auto styleRect = getCurrentMidiStyleButtonRect().toFloat();
        juce::ColourGradient styleGrad(Theme::accentBright, styleRect.getX(), styleRect.getY(),
                                       Theme::accent.darker(0.35f), styleRect.getX(), styleRect.getBottom(), false);
        g.setGradientFill(styleGrad);
        g.fillRoundedRectangle(styleRect, 4.0f);
        g.setColour(juce::Colours::black.withAlpha(0.70f));
        g.drawRoundedRectangle(styleRect.reduced(0.5f), 4.0f, 1.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
        const int variant = midiMoodVariant_[currentGeneratedMidiMood_] + 1;
        g.drawText(getMidiChoiceLabel(currentGeneratedMidiMood_).toUpperCase() + " v" + juce::String(variant),
                   styleRect.toNearestInt(), juce::Justification::centred);
    }

    if (is808Channel_ && draggingMidiOver_)
    {
        auto grid = getGridRect().toFloat().reduced(18.0f);
        g.setColour(juce::Colour(0xff4a260d).withAlpha(0.72f));
        g.fillRoundedRectangle(grid, 8.0f);
        g.setColour(Theme::accentBright);
        g.drawRoundedRectangle(grid, 8.0f, 1.4f);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(15.0f).withStyle("Bold"));
        const auto& importSettings = Midi808ImportSettings::get();
        juce::String dropHint = "Drop MIDI";
        if (importSettings.lowestNotesOnly && importSettings.foldToC4C6)
            dropHint += " - lowest notes, C4-C6";
        else if (importSettings.lowestNotesOnly)
            dropHint += " - lowest notes only";
        else if (importSettings.foldToC4C6)
            dropHint += " - fold to C4-C6";
        g.drawText(dropHint, grid.toNearestInt(), juce::Justification::centred, true);
    }
    
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
    g.drawText("Ctrl+Up/Down: octave  Shift+Up/Down: pitch  Ctrl+L: quantize bar  Ctrl+C/V: copy/paste", w - 620, 0, 610, HEADER_H,
               juce::Justification::centredRight);
    
    // ── Ruler ───────────────────────────────────────────────────
    auto ruler = juce::Rectangle<int>(KEY_W, HEADER_H, w - KEY_W, RULER_H);
    g.setColour(juce::Colour(0xff141417));
    g.fillRect(ruler);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(ruler.getBottom() - 1, (float)KEY_W, (float)w);
    
    int sw = juce::jmax(1, stepW());
    int firstStep = scrollX_ / sw;
    const int rulerLastStep = firstStep + ruler.getWidth() / sw + 3;
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
    for (int s = firstStep; s < rulerLastStep; ++s)
    {
        int x = ruler.getX() + s * sw - scrollX_;
        if (x > ruler.getRight()) break;
        if (x < ruler.getX()) continue;
        
        // Beat (every 4 steps) = brighter; bar (every 16 steps) = brightest
        bool isBar = (s % 16 == 0);
        bool isBeat = (s % 4 == 0);
        
        if (isBar)
        {
            g.setColour(Theme::zinc300);
            g.drawText(juce::String(s / 16 + 1), x + 2, ruler.getY(), 30, RULER_H,
                        juce::Justification::centredLeft);
            g.setColour(juce::Colour(0xff333339));
            g.drawVerticalLine(x, (float)ruler.getY() + 14, (float)ruler.getBottom());
        }
        else if (isBeat)
        {
            g.setColour(juce::Colour(0xff222226));
            g.drawVerticalLine(x, (float)ruler.getY() + 16, (float)ruler.getBottom());
        }
    }
    
    // ── Grid ────────────────────────────────────────────────────
    auto grid = getGridRect();
    g.saveState();
    g.reduceClipRegion(grid);
    
    // Grid background per row (dark/darker for white/black keys)
    int firstRow = scrollY_ / KEY_H;
    for (int r = firstRow; r < firstRow + grid.getHeight() / KEY_H + 2; ++r)
    {
        int pitch = HIGHEST_NOTE - r;
        if (pitch < LOWEST_NOTE) break;
        int y = grid.getY() + r * KEY_H - scrollY_;
        const int drumLane = drumLaneTopPitch_ - pitch;
        const bool isDrumLane = drumLane >= 0 && drumLane < drumLaneNames_.size();
        
        bool black = isBlackKey(pitch);
        g.setColour(isDrumLane ? (drumLane % 2 == 0 ? juce::Colour(0xff141416) : juce::Colour(0xff0f0f12))
                               : (black ? juce::Colour(0xff0a0a0c) : juce::Colour(0xff111114)));
        g.fillRect(grid.getX(), y, grid.getWidth(), KEY_H);
        
        // C-row highlight (octave separator)
        if (!isDrumLane && pitch % 12 == 0)
        {
            g.setColour(juce::Colour(0xff1a1a1e));
            g.fillRect(grid.getX(), y, grid.getWidth(), KEY_H);
        }
        
        // Row separator
        g.setColour(juce::Colour(0xff050507));
        g.drawHorizontalLine(y + KEY_H - 1, (float)grid.getX(), (float)grid.getRight());
    }
    
    // Vertical bar/beat lines
    const int gridLastStep = firstStep + grid.getWidth() / sw + 3;
    for (int s = firstStep; s < gridLastStep; ++s)
    {
        int x = grid.getX() + s * sw - scrollX_;
        if (x > grid.getRight()) break;
        if (x < grid.getX()) continue;
        
        bool isBar = (s % 16 == 0);
        bool isBeat = (s % 4 == 0);
        
        if (isBar) g.setColour(juce::Colour(0xff333339));
        else if (isBeat) g.setColour(juce::Colour(0xff1a1a1e));
        else g.setColour(juce::Colour(0xff111114));
        
        g.drawVerticalLine(x, (float)grid.getY(), (float)grid.getBottom());
    }
    
    // ── Notes ───────────────────────────────────────────────────
    // Cull notes outside the current repaint region so the targeted
    // playhead-strip repaint during playback skips off-strip notes.
    const auto noteClipBounds = g.getClipBounds().toFloat();
    for (size_t i = 0; i < notes_.size(); ++i)
    {
        auto r = getNoteRect(notes_[i]).toFloat();
        if (!r.intersects(grid.toFloat())) continue;
        if (!r.intersects(noteClipBounds)) continue;

        // Body gradient (orange)
        juce::ColourGradient nGrad(Theme::orange1, 0.0f, r.getY(),
                                     Theme::orange3, 0.0f, r.getBottom(), false);
        g.setGradientFill(nGrad);
        g.fillRoundedRectangle(r.reduced(0.5f), 2.0f);
        
        // Top highlight
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawHorizontalLine((int)r.getY() + 1, r.getX() + 2, r.getRight() - 2);
        
        // Border (sky-blue if selected, deep-orange otherwise)
        const bool sel = selectedNotes_.count((int)i) > 0;
        g.setColour(sel ? juce::Colour(0xff60a5fa) : juce::Colour(0xff431407));
        g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, sel ? 1.8f : 1.0f);

        // Note name label (FL Studio style) when the block is wide enough
        if (r.getWidth() >= 22.0f)
        {
            g.setFont(juce::Font(10.0f));
            g.setColour(juce::Colours::white.withAlpha(0.95f));
            const int drumLane = drumLaneTopPitch_ - notes_[i].pitch;
            const juce::String label = drumLane >= 0 && drumLane < drumLaneNames_.size()
                ? drumLaneNames_[drumLane]
                : pitchName(notes_[i].pitch);
            g.drawText(label,
                       r.reduced(4.0f, 0.0f).toNearestInt(),
                       juce::Justification::centredLeft,
                       true);
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

    // ── Playhead (smooth interpolated cursor) ───────────────────
    float phase = 0.0f;
    if (isPlaying_ && stepMs_ > 1.0)
    {
        double elapsed = juce::Time::getMillisecondCounterHiRes() - lastTickMs_;
        phase = (float) juce::jlimit(0.0, 1.0, elapsed / stepMs_);
    }
    int playheadX = -1;
    if (playStep_ >= 0)
    {
        int sw = stepW();
        float fx = (float)(grid.getX() + (playStep_ + phase) * sw - scrollX_);
        playheadX = (int)fx;

        if (playheadX >= grid.getX() && playheadX <= grid.getRight())
        {
            // Soft glow
            g.setColour(juce::Colour(0xfff97316).withAlpha(0.25f));
            g.fillRect(playheadX - 2, grid.getY(), 4, grid.getHeight());
            // Crisp line
            g.setColour(juce::Colour(0xffff9a3d));
            g.drawVerticalLine(playheadX, (float)grid.getY(), (float)grid.getBottom());
        }
    }

    g.restoreState();

    // ── Playhead tick in the top ruler ──────────────────────────
    if (playheadX >= grid.getX() && playheadX <= grid.getRight())
    {
        juce::Path tri;
        tri.addTriangle((float)playheadX - 5, (float)(HEADER_H + RULER_H) - 8,
                        (float)playheadX + 5, (float)(HEADER_H + RULER_H) - 8,
                        (float)playheadX,     (float)(HEADER_H + RULER_H) - 1);
        g.setColour(juce::Colour(0xffff9a3d));
        g.fillPath(tri);
    }
    
    // ── Keyboard (left) ─────────────────────────────────────────
    auto kb = getKeyboardRect();
    g.saveState();
    g.reduceClipRegion(kb);
    
    // White keys background
    g.setColour(juce::Colour(0xfff0f0ee));
    g.fillRect(kb);
    
    for (int r = firstRow; r < firstRow + kb.getHeight() / KEY_H + 2; ++r)
    {
        int pitch = HIGHEST_NOTE - r;
        if (pitch < LOWEST_NOTE) break;
        int y = kb.getY() + r * KEY_H - scrollY_;
        const int drumLane = drumLaneTopPitch_ - pitch;
        const bool isDrumLane = drumLane >= 0 && drumLane < drumLaneNames_.size();
        
        bool black = isBlackKey(pitch);
        if (isDrumLane)
        {
            g.setColour(drumLane % 2 == 0 ? juce::Colour(0xff232326) : juce::Colour(0xff1a1a1d));
            g.fillRect(kb.getX(), y, kb.getWidth(), KEY_H);
            g.setColour(Theme::zinc100);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
            g.drawText(drumLaneNames_[drumLane], kb.getX() + 4, y, kb.getWidth() - 8, KEY_H,
                       juce::Justification::centredLeft, true);
        }
        else if (black)
        {
            // Black key (overlay)
            g.setColour(juce::Colour(0xff1a1a1c));
            g.fillRect(kb.getX(), y, kb.getWidth() - 14, KEY_H);
        }
        
        // Row separator
        g.setColour(juce::Colour(0xffbababa));
        g.drawHorizontalLine(y + KEY_H - 1, (float)kb.getX(), (float)kb.getRight());
        
        // C label
        if (!isDrumLane && pitch % 12 == 0)
        {
            g.setColour(juce::Colour(0xff444444));
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
            g.drawText(pitchName(pitch), kb.getX() + 4, y, kb.getWidth() - 8, KEY_H,
                        juce::Justification::centredRight);
        }
    }
    
    // Right border on keyboard
    g.setColour(juce::Colours::black);
    g.drawVerticalLine(kb.getRight() - 1, (float)kb.getY(), (float)kb.getBottom());
    g.restoreState();

    auto velLane = getVelocityLaneRect();
    if (!velLane.isEmpty())
    {
        g.setColour(juce::Colour(0xff111114));
        g.fillRect(velLane);
        g.setColour(juce::Colour(0xff050507));
        g.drawHorizontalLine(velLane.getY(), (float)velLane.getX(), (float)velLane.getRight());
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
        g.drawText("VELOCITY", velLane.getX() + 8, velLane.getY() + 5, 74, 14, juce::Justification::centredLeft);

        auto laneClip = velLane.reduced(0, 18).withTrimmedBottom(4);
        g.saveState();
        g.reduceClipRegion(laneClip);
        for (size_t i = 0; i < notes_.size(); ++i)
        {
            auto bar = getVelocityBarRect(notes_[i]).toFloat();
            if (!bar.intersects(laneClip.toFloat())) continue;

            const bool sel = selectedNotes_.count((int)i) > 0;
            juce::ColourGradient vg(sel ? Theme::accentBright : Theme::orange1, bar.getX(), bar.getY(),
                                    sel ? Theme::accent : Theme::orange3, bar.getX(), bar.getBottom(), false);
            g.setGradientFill(vg);
            g.fillRoundedRectangle(bar, 2.0f);
            g.setColour(sel ? juce::Colours::white.withAlpha(0.45f) : juce::Colours::black.withAlpha(0.5f));
            g.drawRoundedRectangle(bar.reduced(0.5f), 2.0f, 1.0f);
        }
        g.restoreState();
    }

    drawGenerateMidiMenu(g);
}

void PianoRoll::resized() {}

void PianoRoll::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    boxSelecting_ = false;
    eraseDragging_ = false;
    velocityDragging_ = false;
    velocityDragNote_ = -1;
    draggingPlayhead_ = false;

    if (midiMoodMenuOpen_)
    {
        auto menu = getGenerateMidiMenuRect();
        juce::Rectangle<int> allTab(menu.getX() + 22, menu.getY() + 64, 128, 28);
        juce::Rectangle<int> chordTab(allTab.getRight() + 8, allTab.getY(), 168, allTab.getHeight());
        if (allTab.contains(e.x, e.y) || chordTab.contains(e.x, e.y))
        {
            midiMoodMenuTab_ = chordTab.contains(e.x, e.y) ? 1 : 0;
            midiMoodMenuHover_ = -1;
            midiMoodMenuScrollY_ = 0;
            repaint();
            return;
        }

        const int item = getGenerateMidiMenuItemAt(e.x, e.y);
        if (item >= 0)
        {
            generateMidiForMood(kMidiChoices[item].id, e.mods.isMiddleButtonDown());
            midiMoodMenuOpen_ = false;
            midiMoodMenuHover_ = -1;
            repaint();
            return;
        }

        if (!getGenerateMidiMenuRect().contains(e.x, e.y))
        {
            midiMoodMenuOpen_ = false;
            midiMoodMenuHover_ = -1;
            repaint();
            return;
        }
    }

    if (getGenerateMidiButtonRect().contains(e.x, e.y))
    {
        showGenerateMidiMenu();
        return;
    }

    if (getRealFeelButtonRect().contains(e.x, e.y))
    {
        realFeelEnabled_ = !realFeelEnabled_;
        if (onRealFeelChanged)
            onRealFeelChanged(realFeelEnabled_);
        repaint();
        return;
    }

    if (getStrumButtonRect().contains(e.x, e.y))
    {
        applyStrumToSelection();
        return;
    }

    if (getHumanizeButtonRect().contains(e.x, e.y))
    {
        humanizeSelection();
        return;
    }

    if (isKickChannel_ && getPaste808ButtonRect().contains(e.x, e.y))
    {
        if (onPasteFrom808Requested)
            onPasteFrom808Requested();
        return;
    }

    if (shouldShowHiHatRollButton() && getHiHatRollButtonRect().contains(e.x, e.y))
    {
        showDrumRollMenu(true);
        return;
    }

    if (shouldShowSnareRollButton() && getSnareRollButtonRect().contains(e.x, e.y))
    {
        showDrumRollMenu(false);
        return;
    }

    if (currentGeneratedMidiMood_.isNotEmpty() && getCurrentMidiStyleButtonRect().contains(e.x, e.y))
    {
        generateMidiForMood(currentGeneratedMidiMood_, true);
        return;
    }

    auto velocityLane = getVelocityLaneRect();
    if (!velocityLane.isEmpty() && velocityLane.contains(e.x, e.y))
    {
        const int noteIndex = findVelocityBarAt(e.x, e.y);
        if (noteIndex >= 0)
        {
            velocityDragging_ = true;
            velocityDragNote_ = noteIndex;
            if (!selectedNotes_.count(noteIndex) && !e.mods.isShiftDown())
            {
                selectedNotes_.clear();
                selectedNotes_.insert(noteIndex);
            }
            else if (e.mods.isShiftDown())
            {
                selectedNotes_.insert(noteIndex);
            }
            setVelocityFromPoint(noteIndex, e.y);
        }
        return;
    }

    auto grid = getGridRect();
    const bool onRuler = e.y >= HEADER_H && e.y < HEADER_H + RULER_H && e.x >= grid.getX();
    const bool nearPlayhead = playStep_ >= 0 && std::abs(e.x - (grid.getX() + playStep_ * stepW() - scrollX_)) <= 6 && e.x >= grid.getX();
    if (onRuler || nearPlayhead)
    {
        draggingPlayhead_ = true;
        playStep_ = xToStep(e.x);
        lastTickMs_ = juce::Time::getMillisecondCounterHiRes();
        if (onPlayheadSeek) onPlayheadSeek(playStep_);
        repaint();
        return;
    }

    if (!grid.contains(e.x, e.y)) return;

    grabKeyboardFocus();

    const bool ctrl  = e.mods.isCtrlDown();
    const bool right = e.mods.isRightButtonDown();
    int existing = findNoteAt(e.x, e.y);

    // Ctrl + LMB drag → marquee select (Shift adds to selection)
    if (ctrl && !right)
    {
        boxSelecting_ = true;
        boxStart_     = e.getPosition();
        boxRect_      = juce::Rectangle<int>(boxStart_, boxStart_);
        if (!e.mods.isShiftDown()) selectedNotes_.clear();
        repaint();
        return;
    }

    // Plain right-click -> delete the note under cursor. Keep erasing while the
    // right button is dragged across more notes.
    if (right)
    {
        eraseDragging_ = true;
        eraseNoteAt(e.x, e.y);
        return;
    }

    if (existing >= 0)
    {
        if (onAuditionNote)
            onAuditionNote(notes_[existing].pitch, notes_[existing].lengthSteps, notes_[existing].velocity);

        // Plain click on an unselected note clears the selection
        if (!selectedNotes_.count(existing))
            selectedNotes_.clear();

        // Drag existing note. Right-edge "handle" is the last 35% of the
        // note width (clamped 4..10 px) so very short notes are still
        // resizable but you can still grab the body to move.
        draggingIdx_ = existing;
        auto r = getNoteRect(notes_[existing]);
        const int edge = juce::jlimit(4, 10, (int)(r.getWidth() * 0.35f));
        resizing_ = (e.x >= r.getRight() - edge);
        setMouseCursor(resizing_ ? juce::MouseCursor::LeftRightResizeCursor
                                  : juce::MouseCursor::DraggingHandCursor);
        dragStart_     = e.getPosition();
        dragStartStep_ = notes_[existing].startStep;
        dragStartPitch_= notes_[existing].pitch;
        dragStartLen_  = notes_[existing].lengthSteps;

        // Snapshot positions of every selected note for synchronous drag
        dragStartSelected_.clear();
        dragStartSelectedIds_.clear();
        if (selectedNotes_.count(existing))
        {
            for (int idx : selectedNotes_)
            {
                if (idx < 0 || idx >= (int)notes_.size()) continue;
                dragStartSelectedIds_.push_back(idx);
                dragStartSelected_.emplace_back(notes_[idx].startStep, notes_[idx].pitch);
            }
        }
        return;
    }

    // Empty grid click → create a new note
    int step  = xToStep(e.x);
    int pitch = yToPitch(e.y);
    if (step < 0 || pitch < 0) return;
    if (!drumLaneNames_.isEmpty())
    {
        const int lane = drumLaneTopPitch_ - pitch;
        if (lane < 0 || lane >= drumLaneNames_.size())
            return;
        pitch = drumLaneTopPitch_ - lane;
    }

    selectedNotes_.clear();
    notes_.push_back({ pitch, step, 4, 100 });
    draggingIdx_ = (int)notes_.size() - 1;
    resizing_      = true;
    dragStart_     = e.getPosition();
    dragStartStep_ = step;
    dragStartPitch_= pitch;
    dragStartLen_  = 4;

    if (onAuditionNote)
        onAuditionNote(pitch, 4, 100);
    else
        pluginHost_.playSynthTone(440.0 * std::pow(2.0, (pitch - 69) / 12.0), 0, 0.2, 0.6f);
    repaint();
}

void PianoRoll::mouseDrag(const juce::MouseEvent& e)
{
    if (velocityDragging_)
    {
        setVelocityFromPoint(velocityDragNote_, e.y);
        return;
    }

    if (eraseDragging_)
    {
        eraseNoteAt(e.x, e.y);
        return;
    }

    if (draggingPlayhead_)
    {
        playStep_ = xToStep(e.x);
        lastTickMs_ = juce::Time::getMillisecondCounterHiRes();
        if (onPlayheadSeek) onPlayheadSeek(playStep_);
        repaint();
        return;
    }

    // Box-select drag
    if (boxSelecting_)
    {
        boxRect_ = juce::Rectangle<int>(boxStart_, e.getPosition());
        std::set<int> hits;
        for (int i = 0; i < (int)notes_.size(); ++i)
            if (getNoteRect(notes_[i]).intersects(boxRect_))
                hits.insert(i);
        if (e.mods.isShiftDown())
            selectedNotes_.insert(hits.begin(), hits.end());
        else
            selectedNotes_ = hits;
        repaint();
        return;
    }

    if (draggingIdx_ < 0 || draggingIdx_ >= (int)notes_.size()) return;

    int sw = stepW();
    int dxSteps = (e.x - dragStart_.x) / sw;
    int dyRows  = (e.y - dragStart_.y) / KEY_H;

    if (resizing_)
    {
        notes_[draggingIdx_].lengthSteps = juce::jmax(1, dragStartLen_ + dxSteps);
    }
    else if (!dragStartSelectedIds_.empty())
    {
        // Move every selected note by the same delta
        for (size_t k = 0; k < dragStartSelectedIds_.size(); ++k)
        {
            int idx = dragStartSelectedIds_[k];
            if (idx < 0 || idx >= (int)notes_.size()) continue;
            notes_[idx].startStep = juce::jmax(0, dragStartSelected_[k].first  + dxSteps);
            int pitch = juce::jlimit(LOWEST_NOTE, HIGHEST_NOTE, dragStartSelected_[k].second - dyRows);
            if (!drumLaneNames_.isEmpty())
                pitch = juce::jlimit(drumLaneTopPitch_ - drumLaneNames_.size() + 1, drumLaneTopPitch_, pitch);
            notes_[idx].pitch = pitch;
        }
    }
    else
    {
        notes_[draggingIdx_].startStep = juce::jmax(0, dragStartStep_ + dxSteps);
        int pitch = juce::jlimit(LOWEST_NOTE, HIGHEST_NOTE, dragStartPitch_ - dyRows);
        if (!drumLaneNames_.isEmpty())
            pitch = juce::jlimit(drumLaneTopPitch_ - drumLaneNames_.size() + 1, drumLaneTopPitch_, pitch);
        notes_[draggingIdx_].pitch = pitch;
    }
    repaint();
}

void PianoRoll::mouseUp(const juce::MouseEvent&)
{
    const bool wasDraggingPlayhead = draggingPlayhead_;
    const bool wasEraseDragging = eraseDragging_;
    const bool wasBoxSelecting = boxSelecting_;
    const auto finalBox = boxRect_;
    const auto boxStart = boxStart_;

    draggingPlayhead_ = false;
    eraseDragging_ = false;
    velocityDragging_ = false;
    velocityDragNote_ = -1;
    draggingIdx_  = -1;
    resizing_     = false;
    boxSelecting_ = false;
    boxRect_      = {};

    if (wasBoxSelecting && std::abs(finalBox.getWidth()) < 6 && std::abs(finalBox.getHeight()) < 6)
    {
        const int idx = findNoteAt(boxStart.x, boxStart.y);
        if (idx >= 0)
        {
            if (selectedNotes_.count(idx))
                selectedNotes_.erase(idx);
            else
                selectedNotes_.insert(idx);
        }
    }
    dragStartSelected_.clear();
    dragStartSelectedIds_.clear();
    setMouseCursor(juce::MouseCursor::NormalCursor);

    if (!wasDraggingPlayhead && !wasEraseDragging && onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::mouseMove(const juce::MouseEvent& e)
{
    if (midiMoodMenuOpen_)
    {
        midiMoodMenuHover_ = getGenerateMidiMenuItemAt(e.x, e.y);
        setMouseCursor(midiMoodMenuHover_ >= 0 ? juce::MouseCursor::PointingHandCursor
                                               : juce::MouseCursor::NormalCursor);
        repaint();
        return;
    }

    auto velocityLane = getVelocityLaneRect();
    if (!velocityLane.isEmpty() && velocityLane.contains(e.x, e.y))
    {
        setMouseCursor(findVelocityBarAt(e.x, e.y) >= 0 ? juce::MouseCursor::UpDownResizeCursor
                                                        : juce::MouseCursor::NormalCursor);
        return;
    }

    if (getGenerateMidiButtonRect().contains(e.x, e.y)
        || getRealFeelButtonRect().contains(e.x, e.y)
        || getStrumButtonRect().contains(e.x, e.y)
        || getHumanizeButtonRect().contains(e.x, e.y)
        || (isKickChannel_ && getPaste808ButtonRect().contains(e.x, e.y))
        || (shouldShowHiHatRollButton() && getHiHatRollButtonRect().contains(e.x, e.y))
        || (shouldShowSnareRollButton() && getSnareRollButtonRect().contains(e.x, e.y))
        || (currentGeneratedMidiMood_.isNotEmpty() && getCurrentMidiStyleButtonRect().contains(e.x, e.y)))
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        return;
    }

    // Hover-feedback: show the horizontal resize cursor over the right edge
    // of a note, and a hand-grab cursor anywhere else on a note.
    int idx = findNoteAt(e.x, e.y);
    if (idx < 0) { setMouseCursor(juce::MouseCursor::NormalCursor); return; }

    auto r = getNoteRect(notes_[idx]);
    const int edge = juce::jlimit(4, 10, (int)(r.getWidth() * 0.35f));
    if (e.x >= r.getRight() - edge)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void PianoRoll::copySelectedNotes()
{
    noteClipboard_.clear();
    if (selectedNotes_.empty())
        return;

    int minStart = std::numeric_limits<int>::max();
    for (int idx : selectedNotes_)
    {
        if (idx >= 0 && idx < (int)notes_.size())
            minStart = juce::jmin(minStart, notes_[idx].startStep);
    }
    if (minStart == std::numeric_limits<int>::max())
        return;

    noteClipboardAnchorStep_ = minStart;
    for (int idx : selectedNotes_)
    {
        if (idx < 0 || idx >= (int)notes_.size())
            continue;

        const auto& n = notes_[(size_t)idx];
        noteClipboard_.push_back({ n.pitch, n.startStep - minStart, n.lengthSteps, n.velocity });
    }
}

void PianoRoll::pasteClipboardNotes()
{
    if (noteClipboard_.empty())
        return;

    const int pasteStart = playStep_ >= 0 ? playStep_ : noteClipboardAnchorStep_;
    selectedNotes_.clear();

    for (const auto& src : noteClipboard_)
    {
        Note n;
        n.pitch = juce::jlimit(0, 127, src.pitch);
        n.startStep = juce::jmax(0, pasteStart + src.startStep);
        n.lengthSteps = juce::jmax(1, src.lengthSteps);
        n.velocity = juce::jlimit(1, 127, src.velocity);
        notes_.push_back(n);
        selectedNotes_.insert((int)notes_.size() - 1);
    }

    if (onNotesChanged)
        onNotesChanged();
    repaint();
}

bool PianoRoll::keyPressed(const juce::KeyPress& key)
{
    if (key.getModifiers().isCtrlDown() && (key.getKeyCode() == 'c' || key.getKeyCode() == 'C'))
    {
        copySelectedNotes();
        return !noteClipboard_.empty();
    }
    if (key.getModifiers().isCtrlDown() && (key.getKeyCode() == 'v' || key.getKeyCode() == 'V'))
    {
        pasteClipboardNotes();
        return !noteClipboard_.empty();
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (selectedNotes_.empty()) return false;
        std::vector<int> toErase(selectedNotes_.begin(), selectedNotes_.end());
        std::sort(toErase.begin(), toErase.end(), std::greater<int>());
        for (int i : toErase)
            if (i >= 0 && i < (int)notes_.size())
                notes_.erase(notes_.begin() + i);
        selectedNotes_.clear();
        if (onNotesChanged) onNotesChanged();
        repaint();
        return true;
    }
    if (key.getModifiers().isCtrlDown()
        && (key.getKeyCode() == 'a' || key.getKeyCode() == 'A'))
    {
        selectedNotes_.clear();
        for (int i = 0; i < (int)notes_.size(); ++i)
            selectedNotes_.insert(i);
        repaint();
        return true;
    }
    if (key.getModifiers().isCtrlDown() && (key.getKeyCode() == 'l' || key.getKeyCode() == 'L'))
    {
        quantizeAllNotesToBar();
        return true;
    }
    if (key.getModifiers().isCtrlDown()
        && ! key.getModifiers().isShiftDown()
        && key.getKeyCode() == juce::KeyPress::upKey)
    {
        transposeAllNotesByOctaves(1);
        return true;
    }
    if (key.getModifiers().isCtrlDown()
        && ! key.getModifiers().isShiftDown()
        && key.getKeyCode() == juce::KeyPress::downKey)
    {
        transposeAllNotesByOctaves(-1);
        return true;
    }
    if (key.getModifiers().isShiftDown()
        && ! key.getModifiers().isCtrlDown()
        && key.getKeyCode() == juce::KeyPress::upKey)
    {
        transposeAllNotesBySemitones(1);
        return true;
    }
    if (key.getModifiers().isShiftDown()
        && ! key.getModifiers().isCtrlDown()
        && key.getKeyCode() == juce::KeyPress::downKey)
    {
        transposeAllNotesBySemitones(-1);
        return true;
    }
    if (key == juce::KeyPress('h'))
    {
        humanizeSelection();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        if (selectedNotes_.empty()) return false;
        selectedNotes_.clear();
        repaint();
        return true;
    }
    return false;
}

void PianoRoll::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (midiMoodMenuOpen_ && getGenerateMidiMenuRect().contains(e.x, e.y))
    {
        auto menu = getGenerateMidiMenuRect();
        auto grid = menu.reduced(22, 102).withTrimmedBottom(38);
        const int cols = grid.getWidth() >= 720 ? 3 : (grid.getWidth() >= 460 ? 2 : 1);
        const int gap = 10;
        const int rowH = grid.getHeight() < 300 ? 44 : 48;
        std::vector<int> visible;
        for (int i = 0; i < kMidiChoiceCount; ++i)
            if (midiMoodMenuTab_ == 0 || juce::String(kMidiChoices[i].group) == "Chord Progression")
                visible.push_back(i);
        const int rows = ((int)visible.size() + cols - 1) / cols;
        const int contentH = rows * rowH + juce::jmax(0, rows - 1) * gap;
        const int maxScroll = juce::jmax(0, contentH - grid.getHeight());
        midiMoodMenuScrollY_ = juce::jlimit(0, maxScroll, midiMoodMenuScrollY_ - (int)(wheel.deltaY * 130.0f));
        midiMoodMenuHover_ = getGenerateMidiMenuItemAt(e.x, e.y);
        repaint();
        return;
    }

    if (e.mods.isCtrlDown())
    {
        zoomX_ = juce::jlimit(0.25f, 8.0f, zoomX_ + wheel.deltaY * 0.15f);
    }
    else if (e.mods.isShiftDown())
    {
        scrollX_ -= (int)(wheel.deltaY * 80);
        scrollX_ = juce::jmax(0, scrollX_);
    }
    else
    {
        scrollY_ -= (int)(wheel.deltaY * 60);
        int total = (HIGHEST_NOTE - LOWEST_NOTE + 1) * KEY_H;
        int avail = getHeight() - HEADER_H - RULER_H;
        scrollY_ = juce::jlimit(0, juce::jmax(0, total - avail), scrollY_);
    }
    repaint();
}

void PianoRoll::setNotes(const std::vector<PianoRollNote>& notes)
{
    notes_.clear();
    for (const auto& n : notes)
        notes_.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
    repaint();
}

std::vector<PianoRollNote> PianoRoll::getNotes() const
{
    std::vector<PianoRollNote> result;
    for (const auto& n : notes_)
        result.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
    return result;
}

void PianoRoll::setChannelName(const juce::String& name)
{
    channelName_ = name;
    repaint();
}

void PianoRoll::setChannelContext(bool isKickChannel, bool is808Channel)
{
    isKickChannel_ = isKickChannel;
    is808Channel_ = is808Channel;
    draggingMidiOver_ = false;
    drumLaneNames_.clear();
    repaint();
}

void PianoRoll::setDrumLaneNames(const juce::StringArray& laneNames, int topPitch)
{
    drumLaneNames_ = laneNames;
    drumLaneTopPitch_ = juce::jlimit(LOWEST_NOTE, HIGHEST_NOTE, topPitch);
    scrollY_ = juce::jlimit(0,
        juce::jmax(0, (HIGHEST_NOTE - LOWEST_NOTE + 1) * KEY_H - (getHeight() - HEADER_H - RULER_H)),
        juce::jmax(0, (HIGHEST_NOTE - drumLaneTopPitch_) * KEY_H));
    repaint();
}

bool PianoRoll::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (!is808Channel_)
        return false;

    for (const auto& path : files)
    {
        const auto ext = juce::File(path).getFileExtension();
        if (ext.equalsIgnoreCase(".mid") || ext.equalsIgnoreCase(".midi"))
            return true;
    }
    return false;
}

void PianoRoll::fileDragEnter(const juce::StringArray& files, int, int)
{
    draggingMidiOver_ = isInterestedInFileDrag(files);
    repaint();
}

void PianoRoll::fileDragExit(const juce::StringArray&)
{
    draggingMidiOver_ = false;
    repaint();
}

void PianoRoll::filesDropped(const juce::StringArray& files, int, int)
{
    draggingMidiOver_ = false;
    if (!is808Channel_)
    {
        repaint();
        return;
    }

    for (const auto& path : files)
    {
        const juce::File file(path);
        const auto ext = file.getFileExtension();
        if (file.existsAsFile() && (ext.equalsIgnoreCase(".mid") || ext.equalsIgnoreCase(".midi")))
        {
            importLowestNotesFromMidi(file);
            break;
        }
    }
    repaint();
}

bool PianoRoll::importLowestNotesFromMidi(const juce::File& file)
{
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return false;

    juce::MidiFile midi;
    if (!midi.readFrom(stream))
        return false;

    const auto& importSettings = Midi808ImportSettings::get();
    const int ppq = midi.getTimeFormat();
    const double ticksPerStep = ppq > 0 ? ((double)ppq / 4.0) : 1.0;
    struct LowestAtStep { int pitch = 128; int velocity = 100; int lengthSteps = 1; };
    std::map<int, LowestAtStep> lowest;
    std::vector<Note> importedNotes;

    for (int track = 0; track < midi.getNumTracks(); ++track)
    {
        auto* sequence = const_cast<juce::MidiMessageSequence*>(midi.getTrack(track));
        if (!sequence)
            continue;
        sequence->updateMatchedPairs();

        for (int i = 0; i < sequence->getNumEvents(); ++i)
        {
            const auto* event = sequence->getEventPointer(i);
            const auto& msg = event->message;
            if (!msg.isNoteOn())
                continue;

            const int step = juce::jmax(0, (int)std::llround(msg.getTimeStamp() / ticksPerStep));
            int lengthSteps = 1;
            if (event->noteOffObject != nullptr)
            {
                const double endTick = event->noteOffObject->message.getTimeStamp();
                lengthSteps = juce::jmax(1, (int)std::llround((endTick - msg.getTimeStamp()) / ticksPerStep));
            }

            const int velocity = juce::jlimit(1, 127, (int)std::round(msg.getVelocity() * 127.0f));
            if (importSettings.lowestNotesOnly)
            {
                auto& slot = lowest[step];
                if (msg.getNoteNumber() < slot.pitch)
                {
                    slot.pitch = msg.getNoteNumber();
                    slot.velocity = velocity;
                    slot.lengthSteps = lengthSteps;
                }
            }
            else
            {
                importedNotes.push_back({ msg.getNoteNumber(), step, lengthSteps, velocity });
            }
        }
    }

    if (importSettings.lowestNotesOnly)
    {
        if (lowest.empty())
            return false;

        importedNotes.clear();
        for (const auto& [step, item] : lowest)
        {
            if (item.pitch > 127)
                continue;
            importedNotes.push_back({ item.pitch, step, juce::jmax(1, item.lengthSteps), item.velocity });
        }
    }

    if (importedNotes.empty())
        return false;

    notes_.clear();
    selectedNotes_.clear();
    int maxStep = 16;
    int previousPitch = -1;
    for (auto& item : importedNotes)
    {
        item.pitch = importSettings.applyPitch(item.pitch, previousPitch);
        previousPitch = item.pitch;
        item.pitch = juce::jlimit(LOWEST_NOTE, HIGHEST_NOTE, item.pitch);
        notes_.push_back(item);
        maxStep = juce::jmax(maxStep, item.startStep + juce::jmax(1, item.lengthSteps));
    }

    playStep_ = 0;
    scrollX_ = 0;
    scrollY_ = juce::jlimit(0,
        juce::jmax(0, (HIGHEST_NOTE - LOWEST_NOTE + 1) * KEY_H - (getHeight() - HEADER_H - RULER_H)),
        (HIGHEST_NOTE - 48) * KEY_H);
    if (onNotesChanged)
        onNotesChanged();
    repaint();
    return true;
}

void PianoRoll::setPlayhead(int currentStep, bool playing, double bpm)
{
    const int loopLength = getLoopLengthSteps();
    playStep_  = (loopLength > 0 && currentStep >= 0) ? (currentStep % loopLength) : currentStep;
    isPlaying_ = playing;
    stepMs_    = (bpm > 1.0) ? (60000.0 / bpm / 4.0) : 166.67;
    lastTickMs_ = juce::Time::getMillisecondCounterHiRes();

    if (playing)
    {
        if (!isTimerRunning()) startTimerHz(60);
    }
    else
    {
        stopTimer();
    }
    repaint();
}

void PianoRoll::eraseNoteAt(int x, int y)
{
    const int existing = findNoteAt(x, y);
    if (existing < 0)
        return;

    notes_.erase(notes_.begin() + existing);
    std::set<int> next;
    for (int idx : selectedNotes_)
    {
        if (idx == existing) continue;
        next.insert(idx > existing ? idx - 1 : idx);
    }
    selectedNotes_ = next;
    if (onNotesChanged) onNotesChanged();
    repaint();
}

int PianoRoll::getLoopLengthSteps() const
{
    int endStep = 0;
    for (const auto& n : notes_)
        endStep = juce::jmax(endStep, n.startStep + juce::jmax(1, n.lengthSteps));

    if (endStep <= 0)
        return 0;

    const int barSteps = 16;
    const int bars = juce::jmax(1, (endStep + barSteps - 1) / barSteps);
    if (bars <= 4) return 4 * barSteps;
    if (bars <= 8) return 8 * barSteps;
    return bars * barSteps;
}

bool PianoRoll::shouldShowHiHatRollButton() const
{
    const auto name = channelName_.toLowerCase();
    return name.contains("hi hat") || name.contains("hihat") || name.contains("hi-hat") || name.contains("hat");
}

bool PianoRoll::shouldShowSnareRollButton() const
{
    return channelName_.toLowerCase().contains("snare");
}

void PianoRoll::showDrumRollMenu(bool hiHatRoll)
{
    juce::PopupMenu menu;
    menu.addSectionHeader(hiHatRoll ? "Hi Hat Roll - 1 Bar" : "Snare Roll - 1 Bar");

    if (hiHatRoll)
    {
        menu.addItem(1, "1/8 Build");
        menu.addItem(2, "1/16 Straight");
        menu.addItem(3, "Trap Burst");
        menu.addItem(4, "Stutter End");
        menu.addItem(5, "Rising Velocity");
        menu.addItem(6, "Offbeat Bounce");
    }
    else
    {
        menu.addItem(1, "Pickup Roll");
        menu.addItem(2, "Two Beat Fill");
        menu.addItem(3, "Bounce Roll");
        menu.addItem(4, "Triplet Feel");
        menu.addItem(5, "Drill Roll");
        menu.addItem(6, "Rising Fill");
    }

    juce::Component::SafePointer<PianoRoll> safe(this);
    const auto anchor = hiHatRoll ? getHiHatRollButtonRect() : getSnareRollButtonRect();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(localAreaToGlobal(anchor)),
                       [safe, hiHatRoll](int result)
                       {
                           if (safe != nullptr && result > 0)
                               safe->applyDrumRollVariant(hiHatRoll, result);
                       });
}

void PianoRoll::applyDrumRollVariant(bool hiHatRoll, int variantId)
{
    int pitch = 60;
    if (!notes_.empty())
        pitch = notes_.front().pitch;

    notes_.erase(std::remove_if(notes_.begin(), notes_.end(),
                                [](const Note& n) { return n.startStep >= 0 && n.startStep < 16; }),
                 notes_.end());

    auto add = [this, pitch](int step, int velocity, int length = 1)
    {
        notes_.push_back({ pitch, juce::jlimit(0, 15, step), juce::jmax(1, length), juce::jlimit(1, 127, velocity) });
    };

    if (hiHatRoll)
    {
        switch (variantId)
        {
            case 1:
                for (int s : { 0, 2, 4, 6, 8, 10, 12, 14 }) add(s, 92);
                break;
            case 2:
                for (int s = 0; s < 16; ++s) add(s, (s % 4 == 0) ? 98 : 78);
                break;
            case 3:
                for (int s : { 0, 2, 4, 6, 8, 10, 12, 13, 14, 15 }) add(s, s >= 12 ? 112 : 86);
                break;
            case 4:
                for (int s : { 0, 4, 8, 10, 12, 13, 14, 15 }) add(s, s >= 12 ? 118 : 90);
                break;
            case 5:
                for (int s = 0; s < 16; ++s) add(s, 48 + s * 5);
                break;
            case 6:
            default:
                for (int s : { 2, 6, 10, 12, 13, 14, 15 }) add(s, s >= 12 ? 110 : 88);
                break;
        }
    }
    else
    {
        switch (variantId)
        {
            case 1:
                for (int s : { 12, 13, 14, 15 }) add(s, 86 + (s - 12) * 9);
                break;
            case 2:
                for (int s : { 8, 10, 12, 13, 14, 15 }) add(s, s >= 12 ? 112 : 94);
                break;
            case 3:
                for (int s : { 4, 7, 10, 12, 14, 15 }) add(s, s >= 12 ? 110 : 92);
                break;
            case 4:
                for (int s : { 8, 11, 13, 15 }) add(s, 106);
                break;
            case 5:
                for (int s : { 10, 11, 12, 14, 15 }) add(s, s >= 14 ? 120 : 96);
                break;
            case 6:
            default:
                for (int s : { 8, 9, 10, 12, 13, 14, 15 }) add(s, 72 + s * 3);
                break;
        }
    }

    std::sort(notes_.begin(), notes_.end(), [](const Note& a, const Note& b)
    {
        if (a.startStep != b.startStep) return a.startStep < b.startStep;
        return a.pitch < b.pitch;
    });

    selectedNotes_.clear();
    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::applyStrumToSelection()
{
    std::vector<int> targets;
    if (!selectedNotes_.empty())
    {
        for (int idx : selectedNotes_)
            if (idx >= 0 && idx < (int)notes_.size())
                targets.push_back(idx);
    }
    else
    {
        targets.reserve(notes_.size());
        for (int i = 0; i < (int)notes_.size(); ++i)
            targets.push_back(i);
    }

    if (targets.size() < 2)
        return;

    std::map<int, std::vector<int>> byStartStep;
    for (int idx : targets)
        byStartStep[notes_[(size_t)idx].startStep].push_back(idx);

    bool changed = false;
    for (auto& entry : byStartStep)
    {
        auto& chord = entry.second;
        if (chord.size() < 2)
            continue;

        std::sort(chord.begin(), chord.end(), [this](int a, int b)
        {
            return notes_[(size_t)a].pitch < notes_[(size_t)b].pitch;
        });

        const int originalStart = entry.first;
        for (int order = 0; order < (int)chord.size(); ++order)
        {
            auto& n = notes_[(size_t)chord[(size_t)order]];
            n.startStep = juce::jmax(0, originalStart + order);
            n.velocity = juce::jlimit(1, 127, n.velocity - order * 4);
        }
        changed = true;
    }

    if (changed && onNotesChanged)
        onNotesChanged();
    repaint();
}

void PianoRoll::transposeSelectedNotesByOctaves(int octaveDelta)
{
    if (octaveDelta == 0 || selectedNotes_.empty())
        return;

    const int semitones = octaveDelta * 12;
    bool changed = false;

    for (int idx : selectedNotes_)
    {
        if (idx < 0 || idx >= (int)notes_.size())
            continue;

        const int newPitch = notes_[(size_t)idx].pitch + semitones;
        if (newPitch < LOWEST_NOTE || newPitch > HIGHEST_NOTE)
            continue;

        notes_[(size_t)idx].pitch = newPitch;
        changed = true;
    }

    if (changed)
    {
        if (onNotesChanged)
            onNotesChanged();
        repaint();
    }
}

void PianoRoll::transposeAllNotesByOctaves(int octaveDelta)
{
    if (octaveDelta == 0 || notes_.empty())
        return;

    const int semitones = octaveDelta * 12;
    bool changed = false;

    for (auto& n : notes_)
    {
        const int newPitch = n.pitch + semitones;
        if (newPitch < LOWEST_NOTE || newPitch > HIGHEST_NOTE)
            continue;

        n.pitch = newPitch;
        changed = true;
    }

    if (changed)
    {
        if (onNotesChanged)
            onNotesChanged();
        repaint();
    }
}

void PianoRoll::transposeAllNotesBySemitones(int semitoneDelta)
{
    if (semitoneDelta == 0 || notes_.empty())
        return;

    bool changed = false;
    for (auto& n : notes_)
    {
        const int newPitch = n.pitch + semitoneDelta;
        if (newPitch < LOWEST_NOTE || newPitch > HIGHEST_NOTE)
            continue;

        n.pitch = newPitch;
        changed = true;
    }

    if (changed)
    {
        if (onNotesChanged)
            onNotesChanged();
        repaint();
    }
}

void PianoRoll::quantizeAllNotesToBar()
{
    if (notes_.empty())
        return;

    constexpr int barSteps = 16;
    bool changed = false;

    for (auto& n : notes_)
    {
        const int snappedStart = juce::jmax(0, ((n.startStep + barSteps / 2) / barSteps) * barSteps);
        if (snappedStart != n.startStep)
        {
            n.startStep = snappedStart;
            changed = true;
        }
    }

    if (changed)
    {
        if (onNotesChanged)
            onNotesChanged();
        repaint();
    }
}

void PianoRoll::humanizeSelection()
{
    std::vector<int> targets;
    if (!selectedNotes_.empty())
    {
        for (int idx : selectedNotes_)
            if (idx >= 0 && idx < (int)notes_.size())
                targets.push_back(idx);
    }
    else
    {
        targets.reserve(notes_.size());
        for (int i = 0; i < (int)notes_.size(); ++i)
            targets.push_back(i);
    }

    if (targets.empty())
        return;

    juce::Random rng((int)juce::Time::getMillisecondCounter());
    for (int idx : targets)
    {
        auto& n = notes_[(size_t)idx];
        n.velocity = juce::jlimit(1, 127, n.velocity + rng.nextInt(25) - 12);

        if (rng.nextFloat() < 0.30f)
            n.lengthSteps = juce::jmax(1, n.lengthSteps + rng.nextInt(3) - 1);

        // Timing is stored on the 1/16 grid, so keep this sparse and subtle.
        if (rng.nextFloat() < 0.20f)
            n.startStep = juce::jmax(0, n.startStep + (rng.nextBool() ? 1 : -1));
    }

    if (onNotesChanged)
        onNotesChanged();
    repaint();
}

void PianoRoll::showGenerateMidiMenu()
{
    midiMoodMenuOpen_ = !midiMoodMenuOpen_;
    midiMoodMenuHover_ = -1;
    midiMoodMenuScrollY_ = 0;
    repaint();
}

juce::String PianoRoll::getMidiChoiceLabel(const juce::String& mood) const
{
    for (int i = 0; i < kMidiChoiceCount; ++i)
        if (mood == kMidiChoices[i].id)
            return kMidiChoices[i].label;
    return mood;
}

juce::Rectangle<int> PianoRoll::getGenerateMidiMenuRect() const
{
    const int marginX = getWidth() < 760 ? 14 : 80;
    const int modalW = juce::jlimit(320, 1180, getWidth() - marginX * 2);
    const int maxH = juce::jmax(240, getHeight() - HEADER_H - 30);
    const int modalH = juce::jlimit(260, 620, maxH);
    return { (getWidth() - modalW) / 2, HEADER_H + 10, modalW, modalH };
}

int PianoRoll::getGenerateMidiMenuItemAt(int x, int y) const
{
    auto menu = getGenerateMidiMenuRect();
    if (!menu.contains(x, y))
        return -1;

    auto grid = menu.reduced(22, 102).withTrimmedBottom(38);
    if (!grid.contains(x, y))
        return -1;

    const int cols = grid.getWidth() >= 720 ? 3 : (grid.getWidth() >= 460 ? 2 : 1);
    const int gap = 10;
    const int rowH = grid.getHeight() < 300 ? 44 : 48;
    const int colW = (grid.getWidth() - gap * (cols - 1)) / cols;
    const int localX = x - grid.getX();
    const int localY = y - grid.getY() + midiMoodMenuScrollY_;
    const int col = localX / (colW + gap);
    const int row = localY / (rowH + gap);
    if (col < 0 || col >= cols)
        return -1;
    if (localX - col * (colW + gap) >= colW)
        return -1;
    if (localY - row * (rowH + gap) >= rowH)
        return -1;

    std::vector<int> visible;
    for (int i = 0; i < kMidiChoiceCount; ++i)
        if (midiMoodMenuTab_ == 0 || juce::String(kMidiChoices[i].group) == "Chord Progression")
            visible.push_back(i);

    const int item = row * cols + col;
    return (item >= 0 && item < (int)visible.size()) ? visible[(size_t)item] : -1;
}

void PianoRoll::drawGenerateMidiMenu(juce::Graphics& g)
{
    if (!midiMoodMenuOpen_)
        return;

    auto r = getGenerateMidiMenuRect();
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillRoundedRectangle(r.translated(3, 4).toFloat(), 7.0f);
    g.setColour(juce::Colour(0xff18181b));
    g.fillRoundedRectangle(r.toFloat(), 7.0f);
    g.setColour(juce::Colour(0xffe5e7eb));
    g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 7.0f, 1.0f);

    g.setColour(Theme::orange2);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(16.0f).withStyle("Bold"));
    g.drawText("Generate MIDI Library", r.getX() + 22, r.getY() + 16, r.getWidth() - 44, 22, juce::Justification::centredLeft);
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
    g.drawText("Left click loads a ready MIDI. Middle click cycles more variations for that genre.", r.getX() + 22, r.getY() + 40, r.getWidth() - 44, 18, juce::Justification::centredLeft);

    auto allTab = juce::Rectangle<int>(r.getX() + 22, r.getY() + 64, 128, 28);
    auto chordTab = juce::Rectangle<int>(allTab.getRight() + 8, allTab.getY(), 168, allTab.getHeight());
    auto drawTab = [&](juce::Rectangle<int> tr, const juce::String& label, bool active)
    {
        g.setColour(active ? Theme::accent : juce::Colour(0xff222226));
        g.fillRoundedRectangle(tr.toFloat(), 5.0f);
        g.setColour(juce::Colour(0xff09090b));
        g.drawRoundedRectangle(tr.toFloat().reduced(0.5f), 5.0f, 1.0f);
        g.setColour(active ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(label, tr, juce::Justification::centred);
    };
    drawTab(allTab, "ALL MIDI", midiMoodMenuTab_ == 0);
    drawTab(chordTab, "CHORD PROGRESSION", midiMoodMenuTab_ == 1);

    auto grid = r.reduced(22, 102).withTrimmedBottom(38);
    const int cols = grid.getWidth() >= 720 ? 3 : (grid.getWidth() >= 460 ? 2 : 1);
    const int gap = 10;
    const int rowH = grid.getHeight() < 300 ? 44 : 48;
    const int colW = (grid.getWidth() - gap * (cols - 1)) / cols;
    std::vector<int> visible;
    for (int i = 0; i < kMidiChoiceCount; ++i)
        if (midiMoodMenuTab_ == 0 || juce::String(kMidiChoices[i].group) == "Chord Progression")
            visible.push_back(i);
    const int rows = ((int)visible.size() + cols - 1) / cols;
    const int contentH = rows * rowH + juce::jmax(0, rows - 1) * gap;
    const int maxScroll = juce::jmax(0, contentH - grid.getHeight());
    midiMoodMenuScrollY_ = juce::jlimit(0, maxScroll, midiMoodMenuScrollY_);

    g.saveState();
    g.reduceClipRegion(grid);
    for (int vi = 0; vi < (int)visible.size(); ++vi)
    {
        const int i = visible[(size_t)vi];
        const int col = vi % cols;
        const int rowIndex = vi / cols;
        auto row = juce::Rectangle<int>(grid.getX() + col * (colW + gap),
                                        grid.getY() + rowIndex * (rowH + gap) - midiMoodMenuScrollY_,
                                        colW, rowH);
        if (row.getBottom() < grid.getY() || row.getY() > grid.getBottom())
            continue;

        if (i == midiMoodMenuHover_)
        {
            g.setColour(Theme::accent);
            g.fillRoundedRectangle(row.toFloat(), 5.0f);
        }
        else
        {
            juce::ColourGradient cg(juce::Colour(0xff222226), row.getX(), row.getY(),
                                    juce::Colour(0xff101014), row.getX(), row.getBottom(), false);
            g.setGradientFill(cg);
            g.fillRoundedRectangle(row.toFloat(), 5.0f);
        }
        g.setColour(juce::Colours::black.withAlpha(0.72f));
        g.drawRoundedRectangle(row.toFloat().reduced(0.5f), 5.0f, 1.0f);

        const int variant = midiMoodVariant_[kMidiChoices[i].id] + 1;
        const int bpm = getGeneratedMidiBpmForMood(kMidiChoices[i].id);
        g.setColour(i == midiMoodMenuHover_ ? juce::Colours::white : Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
        g.drawText(kMidiChoices[i].label, row.getX() + 14, row.getY() + 8, row.getWidth() - 70, 16, juce::Justification::centredLeft);
        g.setColour(i == midiMoodMenuHover_ ? juce::Colours::white.withAlpha(0.8f) : Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
        g.drawText(kMidiChoices[i].group, row.getX() + 14, row.getY() + 25, row.getWidth() - 100, 14, juce::Justification::centredLeft);
        g.drawText(juce::String(bpm) + " BPM  v" + juce::String(variant),
                   row.getRight() - 98, row.getY(), 84, row.getHeight(), juce::Justification::centredRight);
    }
    g.restoreState();

    if (maxScroll > 0)
    {
        const float thumbH = juce::jmax(28.0f, grid.getHeight() * (grid.getHeight() / (float)contentH));
        const float travel = (float)grid.getHeight() - thumbH;
        const float thumbY = grid.getY() + travel * (midiMoodMenuScrollY_ / (float)maxScroll);
        auto track = juce::Rectangle<float>((float)grid.getRight() + 7.0f, (float)grid.getY(), 4.0f, (float)grid.getHeight());
        g.setColour(juce::Colour(0xff27272a));
        g.fillRoundedRectangle(track, 2.0f);
        g.setColour(Theme::accent.withAlpha(0.85f));
        g.fillRoundedRectangle(track.withY(thumbY).withHeight(thumbH), 2.0f);
    }

    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
    g.drawText("Generated MIDI writes straight into this piano roll and is ready to play with Space.",
               r.reduced(18, 0).removeFromBottom(26),
               juce::Justification::centredLeft);
}

void PianoRoll::generateMidiForMood(const juce::String& mood, bool nextVariant)
{
    struct MoodPattern
    {
        int root = 60;
        int scale[7] = { 0, 2, 4, 5, 7, 9, 11 };
        int degrees[16] = { 0, 2, 4, 2, 5, 4, 2, 0, 3, 4, 5, 7, 5, 4, 2, 0 };
        int lengths[16] = { 2, 2, 2, 2, 4, 2, 2, 4, 2, 2, 2, 4, 2, 2, 2, 4 };
        int chordDegrees[4] = { 0, 5, 3, 4 };
        int velocity = 100;
        bool swing = false;
    };

    MoodPattern p;
    const int variantCount = 16;
    if (nextVariant)
        midiMoodVariant_[mood] = (midiMoodVariant_[mood] + 1) % variantCount;
    const int variant = mood == "random" ? juce::Random::getSystemRandom().nextInt(variantCount)
                                         : midiMoodVariant_[mood] % variantCount;
    currentGeneratedMidiMood_ = mood;
    if (onGeneratedMidiBpm)
        onGeneratedMidiBpm(getGeneratedMidiBpmForMood(mood));

    struct ChordSpec
    {
        int root;
        std::vector<int> tones;
    };

    auto addChord = [this](int root, const std::vector<int>& tones, int start, int len, int velocity)
    {
        for (size_t i = 0; i < tones.size(); ++i)
        {
            int pitch = root + tones[i];
            while (pitch < LOWEST_NOTE + 10) pitch += 12;
            while (pitch > HIGHEST_NOTE - 6) pitch -= 12;
            if (i >= 3 && pitch < root + 10) pitch += 12;
            pitch = juce::jlimit(LOWEST_NOTE, HIGHEST_NOTE, pitch);
            notes_.push_back({ pitch, start, len, juce::jlimit(45, 127, velocity - (int)i * 3) });
        }
    };

    auto addNote = [this](int pitch, int start, int len, int velocity)
    {
        while (pitch < LOWEST_NOTE + 8) pitch += 12;
        while (pitch > HIGHEST_NOTE - 2) pitch -= 12;
        notes_.push_back({ juce::jlimit(LOWEST_NOTE, HIGHEST_NOTE, pitch), start, len, juce::jlimit(40, 127, velocity) });
    };

    auto generateFromProgression = [&](const std::vector<ChordSpec>& chords, const std::vector<int>& motif, bool swing, bool sparse, bool addTopExtensions)
    {
        notes_.clear();
        selectedNotes_.clear();

        const int rootShiftSemis[] = { 0, 2, -3, 5, -5, 7, -7, 1, -1, 4, -4, 6, -6, 9, -9, 3 };
        const int shift = rootShiftSemis[variant % 16];
        const int rhythmShift = variant % 4;
        const int chordLen = sparse ? 12 : 16;

        for (int bar = 0; bar < 4; ++bar)
        {
            const auto& c = chords[(size_t)(bar % (int)chords.size())];
            const int chordStart = bar * 16;
            addChord(c.root + shift, c.tones, chordStart, chordLen, sparse ? 78 : 92);
            if (addTopExtensions && !c.tones.empty())
            {
                const int top = c.root + shift + c.tones.back() + 12;
                addNote(top, chordStart + 10 + rhythmShift, 2, 70);
            }
        }

        for (int i = 0; i < (int)motif.size(); ++i)
        {
            const int bar = i / 8;
            const auto& c = chords[(size_t)(bar % (int)chords.size())];
            const int tone = c.tones[(size_t)juce::jlimit(0, (int)c.tones.size() - 1, motif[(size_t)i] % (int)c.tones.size())];
            int step = bar * 16 + (i % 8) * 2;
            if (swing && (i % 2 == 1)) step += 1;
            if (sparse && (i + variant) % 3 == 0) continue;
            const int len = sparse ? ((i + variant) % 4 == 0 ? 4 : 2) : ((i + variant) % 5 == 0 ? 4 : 2);
            addNote(c.root + shift + tone + 12 + (((i + variant) % 7 == 0) ? 12 : 0), step, len, sparse ? 84 : 96);
        }

        playStep_ = 0;
        scrollX_ = 0;
        scrollY_ = juce::jlimit(0, juce::jmax(0, (HIGHEST_NOTE - LOWEST_NOTE + 1) * KEY_H - (getHeight() - HEADER_H - RULER_H)),
                                (HIGHEST_NOTE - 72) * KEY_H);
        if (onNotesChanged) onNotesChanged();
        repaint();
    };

    if (mood == "rnb")
    {
        // R&B/neo-soul: maj9, min9/min11, 13ths and altered dominants with close motion.
        std::vector<std::vector<ChordSpec>> banks = {
            { { 50, { 0, 3, 10, 14, 17 } }, { 43, { 0, 4, 10, 14, 21 } }, { 48, { 0, 4, 11, 14, 19 } }, { 45, { 0, 4, 10, 13, 19 } } },
            { { 48, { 0, 4, 11, 14 } }, { 45, { 0, 3, 10, 14, 17 } }, { 50, { 0, 3, 10, 14 } }, { 43, { 0, 4, 10, 13, 21 } } },
            { { 41, { 0, 4, 11, 14, 21 } }, { 43, { 0, 4, 10, 14 } }, { 45, { 0, 3, 10, 14, 17 } }, { 48, { 0, 4, 11, 14 } } },
            { { 53, { 0, 4, 11, 14 } }, { 52, { 0, 3, 10, 14 } }, { 50, { 0, 3, 10, 14 } }, { 43, { 0, 4, 10, 13 } } }
        };
        std::vector<std::vector<int>> motifs = {
            { 4, 3, 1, 2, 4, 1, 0, 2, 3, 4, 2, 1, 4, 3, 1, 0, 2, 4, 3, 1, 0, 2, 4, 3, 4, 2, 1, 3, 4, 1, 0, 2 },
            { 1, 3, 4, 2, 0, 2, 3, 4, 2, 1, 0, 3, 4, 2, 1, 0, 3, 4, 2, 1, 4, 2, 0, 1, 2, 4, 3, 1, 0, 2, 3, 4 },
            { 3, 4, 1, 0, 2, 4, 3, 1, 4, 2, 1, 3, 0, 1, 2, 4, 2, 1, 0, 2, 4, 3, 1, 0, 1, 3, 4, 2, 1, 0, 2, 4 }
        };
        generateFromProgression(banks[(size_t)(variant % (int)banks.size())],
                                motifs[(size_t)((variant / (int)banks.size()) % (int)motifs.size())],
                                true, false, true);
        return;
    }

    if (mood == "chord_rnb" || mood == "chord_neosoul" || mood == "chord_jazz"
        || mood == "chord_gospel" || mood == "chord_pop" || mood == "chord_dark")
    {
        std::vector<std::vector<ChordSpec>> banks;
        bool addExtensions = true;
        bool sparse = false;
        bool cinematic = false;

        if (mood == "chord_neosoul" || mood == "chord_rnb")
        {
            banks = {
                { { 50, { 0, 3, 10, 14, 17 } }, { 43, { 0, 4, 10, 14, 21 } }, { 48, { 0, 4, 11, 14, 19 } }, { 45, { 0, 4, 10, 13, 19 } } },
                { { 48, { 0, 4, 11, 14 } }, { 45, { 0, 3, 10, 14, 17 } }, { 50, { 0, 3, 10, 14 } }, { 43, { 0, 4, 10, 13, 21 } } },
                { { 41, { 0, 4, 11, 14, 18 } }, { 46, { 0, 3, 10, 14, 17 } }, { 43, { 0, 4, 10, 14 } }, { 45, { 0, 4, 10, 13, 19 } } }
            };
        }
        else if (mood == "chord_jazz")
        {
            banks = {
                { { 50, { 0, 3, 7, 10, 14 } }, { 43, { 0, 4, 10, 14, 21 } }, { 48, { 0, 4, 7, 11, 14 } }, { 45, { 0, 4, 10, 13 } } },
                { { 48, { 0, 4, 7, 11, 14 } }, { 45, { 0, 4, 10, 13 } }, { 50, { 0, 3, 7, 10, 14 } }, { 43, { 0, 4, 10, 14 } } }
            };
        }
        else if (mood == "chord_gospel")
        {
            banks = {
                { { 48, { 0, 4, 7, 11, 14 } }, { 52, { 0, 3, 7, 10, 14 } }, { 53, { 0, 4, 7, 11, 14 } }, { 55, { 0, 4, 10, 14 } } },
                { { 45, { 0, 3, 10, 14 } }, { 48, { 0, 4, 11, 14 } }, { 50, { 0, 3, 10, 14 } }, { 43, { 0, 4, 10, 13 } } }
            };
        }
        else if (mood == "chord_dark")
        {
            banks = {
                { { 45, { 0, 3, 7, 10 } }, { 41, { 0, 3, 7 } }, { 48, { 0, 4, 7 } }, { 43, { 0, 3, 7, 10 } } },
                { { 44, { 0, 3, 7 } }, { 52, { 0, 3, 7 } }, { 51, { 0, 4, 7 } }, { 43, { 0, 3, 7, 10 } } }
            };
            addExtensions = false;
            sparse = true;
        }
        else
        {
            banks = {
                { { 48, { 0, 4, 7, 11, 14 } }, { 43, { 0, 4, 7, 10 } }, { 45, { 0, 3, 7, 10 } }, { 41, { 0, 4, 7, 11 } } },
                { { 48, { 0, 4, 7 } }, { 55, { 0, 4, 7 } }, { 45, { 0, 3, 7 } }, { 53, { 0, 4, 7 } } }
            };
            cinematic = true;
        }

        generateFromProgression(banks[(size_t)(variant % (int)banks.size())],
                                {},
                                addExtensions, sparse, cinematic);
        return;
    }

    if (mood == "arp_neo_soul")
    {
        notes_.clear();
        selectedNotes_.clear();
        struct NeoChord { int root; int tones[5]; };
        static const NeoChord chords[] = {
            { 50, { 0, 3, 10, 14, 17 } },
            { 43, { 0, 4, 10, 14, 21 } },
            { 48, { 0, 4, 11, 14, 19 } },
            { 45, { 0, 4, 10, 13, 19 } }
        };
        static const int patterns[][16] = {
            { 0,2,4,3, 1,3,4,2, 0,1,3,4, 2,4,3,1 },
            { 0,1,3,4, 2,4,1,3, 0,2,3,1, 4,3,1,2 },
            { 0,3,1,4, 2,1,3,4, 0,2,4,1, 3,4,2,1 }
        };
        const int pat = variant % 3;
        const int shift = ((variant / 3) % 5) - 2;
        for (int bar = 0; bar < 4; ++bar)
        {
            const auto& chord = chords[bar % 4];
            const int start = bar * 16;
            notes_.push_back({ chord.root + shift, start, 14, 86 });
            notes_.push_back({ chord.root + shift + 12, start + 8, 8, 70 });
            for (int i = 0; i < 16; ++i)
            {
                const int tone = chord.tones[patterns[pat][i] % 5];
                const int step = start + i + ((i % 4 == 3) ? 1 : 0);
                if (step < start + 16)
                    notes_.push_back({ chord.root + shift + tone + 12, step, (i % 5 == 0) ? 3 : 2, 78 + ((i + variant) % 18) });
            }
        }
        currentGeneratedMidiMood_ = mood;
        if (onGeneratedMidiBpm) onGeneratedMidiBpm(getGeneratedMidiBpmForMood(mood));
        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    if (mood == "chill")
    {
        // Chill keys: softer minor/maj7 colors and fewer top-line notes.
        std::vector<std::vector<ChordSpec>> banks = {
            { { 45, { 0, 3, 10, 14 } }, { 52, { 0, 3, 10, 14 } }, { 41, { 0, 4, 11, 14 } }, { 43, { 0, 4, 10, 14 } } },
            { { 48, { 0, 4, 11, 14 } }, { 43, { 0, 4, 10 } }, { 45, { 0, 3, 10, 14 } }, { 41, { 0, 4, 11 } } },
            { { 50, { 0, 3, 10, 14 } }, { 45, { 0, 3, 10, 14 } }, { 48, { 0, 4, 11, 14 } }, { 43, { 0, 4, 10 } } }
        };
        generateFromProgression(banks[(size_t)(variant % (int)banks.size())],
                                { 4, 2, 1, 0, 2, 3, 1, 0, 3, 4, 2, 1, 0, 2, 4, 3,
                                  1, 0, 2, 4, 3, 1, 0, 2, 4, 3, 2, 1, 0, 1, 2, 4 },
                                true, true, true);
        return;
    }

    if (mood == "jazz")
    {
        // Jazz: ii-V-I, I-VI-ii-V and bluesy turnarounds with 7/9/13 extensions.
        std::vector<std::vector<ChordSpec>> banks = {
            { { 50, { 0, 3, 7, 10, 14 } }, { 43, { 0, 4, 10, 14, 21 } }, { 48, { 0, 4, 7, 11, 14 } }, { 45, { 0, 4, 10, 13 } } },
            { { 48, { 0, 4, 7, 11, 14 } }, { 45, { 0, 4, 10, 13 } }, { 50, { 0, 3, 7, 10, 14 } }, { 43, { 0, 4, 10, 14 } } },
            { { 48, { 0, 4, 10, 14 } }, { 53, { 0, 4, 10, 14 } }, { 50, { 0, 3, 10, 14 } }, { 43, { 0, 4, 10, 13 } } },
            { { 45, { 0, 3, 6, 10 } }, { 50, { 0, 3, 7, 10 } }, { 43, { 0, 4, 10, 14 } }, { 48, { 0, 4, 11, 14 } } }
        };
        generateFromProgression(banks[(size_t)(variant % (int)banks.size())],
                                { 0, 2, 4, 3, 1, 4, 2, 0, 3, 4, 2, 1, 0, 2, 3, 4,
                                  4, 3, 2, 1, 0, 2, 4, 3, 1, 2, 3, 4, 2, 1, 0, 2 },
                                true, false, true);
        return;
    }

    if (mood == "boom_bap" || mood == "lofi")
    {
        // Boom bap/lo-fi: sample-style minor jazz loops, dusty 9ths and altered V chords.
        std::vector<std::vector<ChordSpec>> banks = {
            { { 48, { 0, 3, 10, 14, 19 } }, { 44, { 0, 4, 11, 14 } }, { 41, { 0, 3, 10, 14 } }, { 43, { 0, 4, 10, 13 } } },
            { { 45, { 0, 3, 10, 14 } }, { 48, { 0, 4, 11, 14 } }, { 43, { 0, 4, 10 } }, { 41, { 0, 3, 10, 14 } } },
            { { 50, { 0, 3, 10, 14 } }, { 45, { 0, 3, 10 } }, { 53, { 0, 4, 11 } }, { 43, { 0, 4, 10, 13 } } },
            { { 48, { 0, 3, 10 } }, { 55, { 0, 3, 10, 14 } }, { 53, { 0, 4, 11 } }, { 50, { 0, 3, 10 } } }
        };
        generateFromProgression(banks[(size_t)(variant % (int)banks.size())],
                                { 4, 2, 1, 0, 2, 4, 3, 1, 0, 1, 3, 4, 2, 1, 0, 2,
                                  3, 4, 2, 0, 1, 2, 4, 3, 1, 0, 2, 3, 4, 2, 1, 0 },
                                true, false, true);
        return;
    }

    if (mood == "trap" || mood == "dark" || mood == "drill" || mood == "memphis")
    {
        // Trap/drill/dark: sparse minor loops, repeated roots, tension tones, room for drums.
        std::vector<std::vector<ChordSpec>> banks = {
            { { 45, { 0, 3, 7, 10 } }, { 41, { 0, 3, 7 } }, { 48, { 0, 4, 7 } }, { 43, { 0, 3, 7, 10 } } },
            { { 44, { 0, 3, 7 } }, { 44, { 0, 3, 7, 10 } }, { 52, { 0, 3, 7 } }, { 51, { 0, 4, 7 } } },
            { { 50, { 0, 3, 7, 10 } }, { 46, { 0, 3, 7 } }, { 43, { 0, 4, 7 } }, { 45, { 0, 3, 7 } } },
            { { 41, { 0, 3, 7 } }, { 48, { 0, 4, 7 } }, { 43, { 0, 3, 7 } }, { 41, { 0, 3, 7, 10 } } }
        };
        generateFromProgression(banks[(size_t)(variant % (int)banks.size())],
                                { 0, 2, 1, 0, 3, 2, 0, 1, 2, 3, 1, 0, 2, 1, 0, 0,
                                  4, 3, 2, 0, 1, 0, 2, 3, 1, 0, 2, 4, 2, 1, 0, 0 },
                                false, true, false);
        return;
    }

    if (mood == "house")
    {
        // House piano: minor 7ths, bright maj7s and repeated stab-friendly shapes.
        std::vector<std::vector<ChordSpec>> banks = {
            { { 45, { 0, 3, 7, 10 } }, { 41, { 0, 4, 7, 11 } }, { 48, { 0, 4, 7, 11 } }, { 43, { 0, 4, 7, 10 } } },
            { { 48, { 0, 4, 7, 11 } }, { 43, { 0, 4, 7, 10 } }, { 45, { 0, 3, 7, 10 } }, { 41, { 0, 4, 7, 11 } } },
            { { 50, { 0, 3, 7, 10 } }, { 45, { 0, 3, 7, 10 } }, { 53, { 0, 4, 7, 11 } }, { 48, { 0, 4, 7, 10 } } }
        };
        generateFromProgression(banks[(size_t)(variant % (int)banks.size())],
                                { 0, 1, 2, 3, 2, 1, 3, 2, 0, 2, 3, 1, 2, 3, 1, 0,
                                  0, 1, 3, 2, 1, 2, 3, 2, 0, 3, 2, 1, 3, 2, 1, 0 },
                                false, false, false);
        return;
    }

    if (mood == "afrobeat" || mood == "reggaeton" || mood == "funk" || mood == "detroit")
    {
        // Dance/funk genres: syncopated diatonic progressions with dominant and 9th color.
        std::vector<std::vector<ChordSpec>> banks = {
            { { 48, { 0, 4, 7, 14 } }, { 53, { 0, 4, 7, 10 } }, { 45, { 0, 3, 7, 10 } }, { 55, { 0, 4, 7, 10 } } },
            { { 45, { 0, 3, 7, 10 } }, { 50, { 0, 3, 7, 10 } }, { 43, { 0, 4, 7, 10 } }, { 48, { 0, 4, 7, 14 } } },
            { { 48, { 0, 4, 7 } }, { 55, { 0, 4, 7, 10 } }, { 53, { 0, 4, 7, 10 } }, { 55, { 0, 4, 7, 10 } } },
            { { 50, { 0, 3, 10 } }, { 48, { 0, 4, 7, 14 } }, { 45, { 0, 3, 7, 10 } }, { 43, { 0, 4, 7, 10 } } }
        };
        generateFromProgression(banks[(size_t)(variant % (int)banks.size())],
                                { 0, 2, 3, 1, 2, 4, 3, 1, 0, 3, 2, 4, 1, 2, 3, 0,
                                  2, 4, 3, 1, 0, 2, 3, 1, 4, 3, 2, 1, 0, 2, 4, 3 },
                                true, mood == "detroit", false);
        return;
    }

    if (mood == "happy" || mood == "epic")
    {
        // Pop/cinematic: proven I-V-vi-IV / vi-IV-I-V shapes, colored but still direct.
        std::vector<std::vector<ChordSpec>> banks = {
            { { 48, { 0, 4, 7, 11, 14 } }, { 43, { 0, 4, 7, 10 } }, { 45, { 0, 3, 7, 10 } }, { 41, { 0, 4, 7, 11 } } },
            { { 45, { 0, 3, 7, 10 } }, { 41, { 0, 4, 7, 11 } }, { 48, { 0, 4, 7, 11 } }, { 43, { 0, 4, 7, 10 } } },
            { { 48, { 0, 4, 7 } }, { 55, { 0, 4, 7 } }, { 45, { 0, 3, 7 } }, { 53, { 0, 4, 7 } } }
        };
        generateFromProgression(banks[(size_t)(variant % (int)banks.size())],
                                { 0, 2, 4, 2, 3, 4, 2, 0, 1, 3, 4, 2, 4, 3, 1, 0,
                                  0, 1, 2, 4, 3, 2, 4, 1, 0, 2, 3, 4, 2, 1, 0, 2 },
                                false, false, mood == "epic");
        return;
    }

    if (mood == "sad")
    {
        // Sad minor: i9, iv9, bVImaj7 and dominant b9 resolutions.
        std::vector<std::vector<ChordSpec>> banks = {
            { { 45, { 0, 3, 10, 14 } }, { 50, { 0, 3, 10, 14 } }, { 41, { 0, 4, 11, 14 } }, { 40, { 0, 4, 10, 13 } } },
            { { 48, { 0, 3, 10, 14 } }, { 43, { 0, 3, 10 } }, { 44, { 0, 4, 11 } }, { 47, { 0, 4, 10, 13 } } },
            { { 45, { 0, 3, 10 } }, { 41, { 0, 4, 11 } }, { 50, { 0, 3, 10, 14 } }, { 43, { 0, 4, 10, 13 } } }
        };
        generateFromProgression(banks[(size_t)(variant % (int)banks.size())],
                                { 4, 2, 1, 0, 1, 2, 3, 1, 0, 2, 4, 3, 2, 1, 0, 0,
                                  3, 4, 2, 0, 1, 0, 2, 3, 4, 2, 1, 0, 2, 1, 0, 0 },
                                true, false, true);
        return;
    }

    if (mood == "guitar_classical" || mood == "guitar_spanish" || mood == "arp_guitar")
    {
        notes_.clear();
        selectedNotes_.clear();

        struct GuitarChord { int bass; int tones[5]; };
        const GuitarChord classical[] = {
            { 52, { 0, 7, 12, 16, 19 } },   // E minor shape
            { 45, { 0, 7, 12, 15, 19 } },   // A minor
            { 50, { 0, 7, 12, 17, 21 } },   // D sus/add
            { 47, { 0, 7, 11, 14, 19 } }    // G/B color
        };
        const GuitarChord spanish[] = {
            { 52, { 0, 7, 12, 15, 19 } },   // Em
            { 53, { 0, 7, 12, 16, 21 } },   // F
            { 55, { 0, 7, 11, 14, 19 } },   // G
            { 52, { 0, 7, 12, 16, 22 } }    // E7 tension
        };
        const GuitarChord* progression = mood == "guitar_spanish" ? spanish : classical;
        const int rootShiftSemis[] = { 0, -2, 3, 5, -5, 7, -7, 1, -1, 4, -4, 6, -6, 9, -9, 2 };
        const int shift = rootShiftSemis[variant % 16];
        const int patternA[] = { 0, 2, 4, 3, 2, 1, 3, 2, 0, 2, 4, 3, 2, 1, 3, 4 };
        const int patternB[] = { 0, 1, 2, 3, 4, 3, 2, 1, 0, 2, 1, 3, 2, 4, 3, 1 };
        const int* arp = (mood == "arp_guitar" || (variant % 2 == 1)) ? patternB : patternA;
        const int gate = mood == "guitar_spanish" ? 2 : 3;

        for (int bar = 0; bar < 4; ++bar)
        {
            const auto& chord = progression[bar % 4];
            const int start = bar * 16;
            const int bass = juce::jlimit(LOWEST_NOTE, HIGHEST_NOTE, chord.bass + shift);
            notes_.push_back({ bass, start, 16, 88 });
            notes_.push_back({ bass + 12, start + 8, 8, 70 });

            for (int i = 0; i < 16; ++i)
            {
                const int toneIndex = arp[i] % 5;
                int pitch = chord.bass + shift + chord.tones[toneIndex];
                while (pitch < 52) pitch += 12;
                while (pitch > 84) pitch -= 12;
                const int step = start + i + ((mood == "guitar_spanish" && (i % 4 == 3)) ? 1 : 0);
                if (step < start + 16)
                    notes_.push_back({ pitch, step, gate, 82 + ((i + variant) % 18) });
            }
        }

        playStep_ = 0;
        scrollX_ = 0;
        scrollY_ = juce::jlimit(0, juce::jmax(0, (HIGHEST_NOTE - LOWEST_NOTE + 1) * KEY_H - (getHeight() - HEADER_H - RULER_H)),
                                (HIGHEST_NOTE - 72) * KEY_H);
        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    if (mood.startsWith("arp_"))
    {
        notes_.clear();
        selectedNotes_.clear();

        int root = 60;
        int scale[7] = { 0, 2, 4, 5, 7, 9, 11 };
        int chordDegrees[4] = { 0, 5, 3, 4 };
        if (mood == "arp_dark")
        {
            root = 56;
            int s[] = { 0, 1, 3, 5, 7, 8, 10 };
            int c[] = { 0, 5, 4, 3 };
            std::copy(s, s + 7, scale);
            std::copy(c, c + 4, chordDegrees);
        }
        else if (mood == "arp_jazz")
        {
            root = 62;
            int s[] = { 0, 2, 3, 5, 7, 9, 10 };
            int c[] = { 0, 3, 5, 2 };
            std::copy(s, s + 7, scale);
            std::copy(c, c + 4, chordDegrees);
        }
        else if (mood == "arp_trance")
        {
            root = 57;
            int c[] = { 0, 5, 3, 4 };
            std::copy(c, c + 4, chordDegrees);
        }

        root = juce::jlimit(42, 72, root + ((variant % 7) - 3));
        const int gate = (mood == "arp_trance") ? 1 : ((variant % 3) + 1);
        for (int bar = 0; bar < 4; ++bar)
        {
            int degrees[] = { 0, 2, 4, 6 };
            if (mood == "arp_down")
                std::reverse(degrees, degrees + 4);
            for (int i = 0; i < 16; ++i)
            {
                int idx = i % 4;
                if (mood == "arp_bounce")
                    idx = (i % 8 < 4) ? i % 4 : 3 - (i % 4);
                const int chordRootDegree = chordDegrees[bar] % 7;
                const int degree = (chordRootDegree + degrees[idx]) % 7;
                int pitch = root + scale[degree] + (i >= 8 ? 12 : 0);
                if ((variant + i) % 11 == 0) pitch += 12;
                if (pitch > HIGHEST_NOTE) pitch -= 12;
                notes_.push_back({ pitch, bar * 16 + i, gate, 92 + (variant % 20) });
            }
        }

        playStep_ = 0;
        scrollX_ = 0;
        scrollY_ = juce::jlimit(0, juce::jmax(0, (HIGHEST_NOTE - LOWEST_NOTE + 1) * KEY_H - (getHeight() - HEADER_H - RULER_H)),
                                (HIGHEST_NOTE - (root + 12)) * KEY_H);
        if (onNotesChanged) onNotesChanged();
        repaint();
        return;
    }

    if (mood == "sad")
    {
        p.root = 57;
        int scale[] = { 0, 2, 3, 5, 7, 8, 10 };
        int deg[] = { 0, 2, 4, 5, 4, 2, 1, 0, 3, 4, 5, 4, 2, 1, 0, 0 };
        int chords[] = { 0, 5, 3, 4 };
        std::copy(scale, scale + 7, p.scale);
        std::copy(deg, deg + 16, p.degrees);
        std::copy(chords, chords + 4, p.chordDegrees);
        p.velocity = 88;
    }
    else if (mood == "jazz")
    {
        p.root = 62;
        int scale[] = { 0, 2, 3, 5, 7, 9, 10 };
        int deg[] = { 0, 2, 4, 6, 5, 4, 2, 1, 3, 5, 6, 4, 2, 4, 1, 0 };
        int chords[] = { 0, 3, 5, 2 };
        int lens[] = { 2, 1, 3, 2, 2, 2, 1, 4, 2, 1, 3, 2, 2, 2, 1, 4 };
        std::copy(scale, scale + 7, p.scale);
        std::copy(deg, deg + 16, p.degrees);
        std::copy(chords, chords + 4, p.chordDegrees);
        std::copy(lens, lens + 16, p.lengths);
        p.swing = true;
    }
    else if (mood == "dark" || mood == "trap")
    {
        p.root = 56;
        int scale[] = { 0, 1, 3, 5, 7, 8, 10 };
        int deg[] = { 0, 0, 2, 3, 4, 3, 2, 0, 5, 4, 3, 2, 1, 2, 0, 0 };
        int chords[] = { 0, 5, 4, 3 };
        int lens[] = { 4, 1, 1, 2, 4, 1, 1, 2, 2, 2, 1, 1, 4, 1, 1, 4 };
        std::copy(scale, scale + 7, p.scale);
        std::copy(deg, deg + 16, p.degrees);
        std::copy(chords, chords + 4, p.chordDegrees);
        std::copy(lens, lens + 16, p.lengths);
        p.velocity = mood == "trap" ? 108 : 94;
    }
    else if (mood == "chill" || mood == "rnb")
    {
        p.root = mood == "rnb" ? 61 : 60;
        int scale[] = { 0, 2, 3, 5, 7, 9, 10 };
        int deg[] = { 0, 2, 4, 5, 4, 2, 0, 1, 3, 4, 6, 5, 4, 2, 1, 0 };
        int chords[] = { 0, 4, 5, 3 };
        int lens[] = { 3, 1, 2, 2, 4, 2, 2, 2, 3, 1, 2, 2, 4, 2, 2, 4 };
        std::copy(scale, scale + 7, p.scale);
        std::copy(deg, deg + 16, p.degrees);
        std::copy(chords, chords + 4, p.chordDegrees);
        std::copy(lens, lens + 16, p.lengths);
        p.velocity = 82;
        p.swing = true;
    }
    else if (mood == "epic")
    {
        p.root = 55;
        int deg[] = { 0, 4, 5, 7, 4, 5, 7, 9, 7, 5, 4, 2, 4, 5, 7, 11 };
        int chords[] = { 0, 5, 3, 4 };
        std::copy(deg, deg + 16, p.degrees);
        std::copy(chords, chords + 4, p.chordDegrees);
        p.velocity = 112;
    }
    else if (mood == "boom_bap" || mood == "lofi")
    {
        p.root = mood == "lofi" ? 58 : 55;
        int scale[] = { 0, 2, 3, 5, 7, 8, 10 };
        int deg[] = { 0, 2, 4, 2, 1, 0, 5, 4, 3, 4, 2, 0, 1, 2, 4, 3 };
        int chords[] = { 0, 3, 4, 5 };
        int lens[] = { 4, 1, 2, 3, 2, 2, 4, 1, 3, 1, 2, 4, 2, 2, 2, 6 };
        std::copy(scale, scale + 7, p.scale);
        std::copy(deg, deg + 16, p.degrees);
        std::copy(chords, chords + 4, p.chordDegrees);
        std::copy(lens, lens + 16, p.lengths);
        p.velocity = mood == "lofi" ? 76 : 88;
        p.swing = true;
    }
    else if (mood == "house")
    {
        p.root = 60;
        int deg[] = { 0, 0, 4, 4, 5, 5, 4, 2, 3, 3, 5, 5, 4, 4, 2, 0 };
        int chords[] = { 0, 5, 3, 4 };
        int lens[] = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4 };
        std::copy(deg, deg + 16, p.degrees);
        std::copy(chords, chords + 4, p.chordDegrees);
        std::copy(lens, lens + 16, p.lengths);
        p.velocity = 112;
    }
    else if (mood == "afrobeat" || mood == "reggaeton")
    {
        p.root = mood == "reggaeton" ? 57 : 61;
        int scale[] = { 0, 2, 4, 7, 9, 12, 14 };
        int deg[] = { 0, 2, 4, 2, 5, 4, 2, 0, 3, 5, 4, 2, 1, 2, 4, 0 };
        int chords[] = { 0, 4, 3, 5 };
        int lens[] = { 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1, 4 };
        std::copy(scale, scale + 7, p.scale);
        std::copy(deg, deg + 16, p.degrees);
        std::copy(chords, chords + 4, p.chordDegrees);
        std::copy(lens, lens + 16, p.lengths);
        p.velocity = 96;
        p.swing = true;
    }
    else if (mood == "drill" || mood == "memphis")
    {
        p.root = mood == "memphis" ? 53 : 56;
        int scale[] = { 0, 1, 3, 5, 6, 7, 10 };
        int deg[] = { 0, 0, 2, 4, 3, 2, 1, 0, 5, 4, 3, 2, 0, 2, 1, 0 };
        int chords[] = { 0, 5, 4, 1 };
        std::copy(scale, scale + 7, p.scale);
        std::copy(deg, deg + 16, p.degrees);
        std::copy(chords, chords + 4, p.chordDegrees);
        p.velocity = 104;
    }
    else if (mood == "detroit" || mood == "funk")
    {
        p.root = mood == "funk" ? 60 : 58;
        int scale[] = { 0, 2, 3, 5, 7, 9, 10 };
        int deg[] = { 0, 2, 0, 4, 2, 5, 4, 2, 0, 3, 5, 3, 4, 2, 1, 0 };
        int chords[] = { 0, 4, 3, 5 };
        int lens[] = { 1, 1, 2, 1, 2, 1, 1, 3, 1, 1, 2, 1, 2, 1, 1, 3 };
        std::copy(scale, scale + 7, p.scale);
        std::copy(deg, deg + 16, p.degrees);
        std::copy(chords, chords + 4, p.chordDegrees);
        std::copy(lens, lens + 16, p.lengths);
        p.velocity = 106;
        p.swing = mood == "funk";
    }
    else if (mood == "random")
    {
        juce::Random rng;
        p.root = 48 + rng.nextInt(18);
        for (int i = 0; i < 16; ++i)
        {
            p.degrees[i] = rng.nextInt(7);
            p.lengths[i] = 1 + rng.nextInt(4);
        }
        for (int i = 0; i < 4; ++i)
            p.chordDegrees[i] = rng.nextInt(6);
        p.velocity = 86 + rng.nextInt(30);
    }

    if (mood != "random" && variant > 0)
    {
        static const int rotations[] = { 0, 3, 5, 7, 11 };
        static const int rootShift[] = { 0, 2, -3, 5, -5 };
        static const int chordSwaps[5][4] = {
            { 0, 5, 3, 4 },
            { 0, 3, 4, 5 },
            { 5, 0, 3, 4 },
            { 0, 4, 5, 3 },
            { 3, 4, 0, 5 }
        };

        const int variantShape = variant % 5;
        p.root = juce::jlimit(42, 72, p.root + rootShift[variantShape] + (variant / 5) * 2);

        int oldDeg[16];
        int oldLen[16];
        std::copy(p.degrees, p.degrees + 16, oldDeg);
        std::copy(p.lengths, p.lengths + 16, oldLen);
        const int rotation = rotations[variantShape];
        for (int i = 0; i < 16; ++i)
        {
            const int src = (i + rotation) % 16;
            p.degrees[i] = juce::jlimit(0, 6, (oldDeg[src] + ((i + variant) % 3 == 0 ? variant : 0)) % 7);
            p.lengths[i] = juce::jlimit(1, 8, oldLen[src] + ((i + variant) % 5 == 0 ? 1 : 0) - ((i + variant) % 7 == 0 ? 1 : 0));
        }

        for (int i = 0; i < 4; ++i)
            p.chordDegrees[i] = chordSwaps[variantShape][i];

        p.swing = p.swing || variantShape == 2 || variantShape == 4;
        p.velocity = juce::jlimit(55, 124, p.velocity + (variantShape - 2) * 4);
    }

    notes_.clear();
    selectedNotes_.clear();

    for (int bar = 0; bar < 4; ++bar)
    {
        const int chordRootDegree = p.chordDegrees[bar] % 7;
        const int chordRoot = p.root + p.scale[chordRootDegree] - 12;
        notes_.push_back({ chordRoot, bar * 16, 16, juce::jlimit(40, 127, p.velocity - 30) });
        notes_.push_back({ chordRoot + p.scale[(chordRootDegree + 2) % 7], bar * 16, 16, juce::jlimit(40, 127, p.velocity - 34) });
        notes_.push_back({ chordRoot + p.scale[(chordRootDegree + 4) % 7], bar * 16, 16, juce::jlimit(40, 127, p.velocity - 36) });
    }

    for (int i = 0; i < 16; ++i)
    {
        int step = i * 4;
        if (p.swing && (i % 2 == 1))
            step += 1;

        const int degree = juce::jlimit(0, 6, p.degrees[i] % 7);
        int octave = (i >= 8) ? 12 : 0;
        int pitch = p.root + p.scale[degree] + octave;
        if (pitch > HIGHEST_NOTE) pitch -= 12;
        const int len = juce::jlimit(1, 8, p.lengths[i]);
        notes_.push_back({ pitch, step, len, p.velocity });
    }

    playStep_ = 0;
    scrollX_ = 0;
    scrollY_ = juce::jlimit(0, juce::jmax(0, (HIGHEST_NOTE - LOWEST_NOTE + 1) * KEY_H - (getHeight() - HEADER_H - RULER_H)),
                            (HIGHEST_NOTE - (p.root + 12)) * KEY_H);

    if (onNotesChanged) onNotesChanged();
    repaint();
}

void PianoRoll::timerCallback()
{
    if (!isPlaying_) return;

    // Repaint only the playhead strip (old + new x) rather than the whole roll.
    float phase = 0.0f;
    if (stepMs_ > 1.0)
    {
        const double elapsed = juce::Time::getMillisecondCounterHiRes() - lastTickMs_;
        phase = (float) juce::jlimit(0.0, 1.0, elapsed / stepMs_);
    }
    auto grid = getGridRect();
    const int newX = (playStep_ >= 0)
                   ? (int)(grid.getX() + (playStep_ + phase) * stepW() - scrollX_)
                   : lastPlayheadX_;
    if (newX < 0) { repaint(); return; }

    const int pad = 9;
    const int lo = (lastPlayheadX_ < 0) ? newX : juce::jmin(lastPlayheadX_, newX);
    const int hi = (lastPlayheadX_ < 0) ? newX : juce::jmax(lastPlayheadX_, newX);
    repaint(lo - pad, 0, (hi - lo) + 2 * pad, getHeight());
    lastPlayheadX_ = newX;
}
