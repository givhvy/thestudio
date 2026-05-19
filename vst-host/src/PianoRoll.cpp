#include "PianoRoll.h"
#include "PluginHost.h"
#include "Theme.h"
#include <algorithm>

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
    g.drawText(titleText, 22, 0, 300, HEADER_H, juce::Justification::centredLeft);

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
    
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
    g.drawText("Snap: 1/16  •  Ctrl+Scroll: Zoom", w - 220, 0, 210, HEADER_H, juce::Justification::centredRight);
    
    // ── Ruler ───────────────────────────────────────────────────
    auto ruler = juce::Rectangle<int>(KEY_W, HEADER_H, w - KEY_W, RULER_H);
    g.setColour(juce::Colour(0xff141417));
    g.fillRect(ruler);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(ruler.getBottom() - 1, (float)KEY_W, (float)w);
    
    int sw = stepW();
    int firstStep = scrollX_ / sw;
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
    for (int s = firstStep; s < firstStep + 200; ++s)
    {
        int x = ruler.getX() + s * sw - scrollX_;
        if (x > w) break;
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
        
        bool black = isBlackKey(pitch);
        g.setColour(black ? juce::Colour(0xff0a0a0c) : juce::Colour(0xff111114));
        g.fillRect(grid.getX(), y, grid.getWidth(), KEY_H);
        
        // C-row highlight (octave separator)
        if (pitch % 12 == 0)
        {
            g.setColour(juce::Colour(0xff1a1a1e));
            g.fillRect(grid.getX(), y, grid.getWidth(), KEY_H);
        }
        
        // Row separator
        g.setColour(juce::Colour(0xff050507));
        g.drawHorizontalLine(y + KEY_H - 1, (float)grid.getX(), (float)grid.getRight());
    }
    
    // Vertical bar/beat lines
    for (int s = firstStep; s < firstStep + 200; ++s)
    {
        int x = grid.getX() + s * sw - scrollX_;
        if (x > w) break;
        if (x < grid.getX()) continue;
        
        bool isBar = (s % 16 == 0);
        bool isBeat = (s % 4 == 0);
        
        if (isBar) g.setColour(juce::Colour(0xff333339));
        else if (isBeat) g.setColour(juce::Colour(0xff1a1a1e));
        else g.setColour(juce::Colour(0xff111114));
        
        g.drawVerticalLine(x, (float)grid.getY(), (float)grid.getBottom());
    }
    
    // ── Notes ───────────────────────────────────────────────────
    for (size_t i = 0; i < notes_.size(); ++i)
    {
        auto r = getNoteRect(notes_[i]).toFloat();
        if (!r.intersects(grid.toFloat())) continue;
        
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
        
        bool black = isBlackKey(pitch);
        if (black)
        {
            // Black key (overlay)
            g.setColour(juce::Colour(0xff1a1a1c));
            g.fillRect(kb.getX(), y, kb.getWidth() - 14, KEY_H);
        }
        
        // Row separator
        g.setColour(juce::Colour(0xffbababa));
        g.drawHorizontalLine(y + KEY_H - 1, (float)kb.getX(), (float)kb.getRight());
        
        // C label
        if (pitch % 12 == 0)
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

    const bool ctrl  = e.mods.isCtrlDown();
    const bool right = e.mods.isRightButtonDown();
    int existing = findNoteAt(e.x, e.y);

    // Ctrl + RMB drag (or Ctrl+LMB on empty space) → box select
    if (ctrl && (right || existing < 0))
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

        // Ctrl+click → toggle this note in the selection
        if (ctrl)
        {
            if (selectedNotes_.count(existing)) selectedNotes_.erase(existing);
            else                                selectedNotes_.insert(existing);
            repaint();
            return;
        }

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
            notes_[idx].pitch     = juce::jlimit(LOWEST_NOTE, HIGHEST_NOTE,
                                                  dragStartSelected_[k].second - dyRows);
        }
    }
    else
    {
        notes_[draggingIdx_].startStep = juce::jmax(0, dragStartStep_ + dxSteps);
        notes_[draggingIdx_].pitch     = juce::jlimit(LOWEST_NOTE, HIGHEST_NOTE,
                                                       dragStartPitch_ - dyRows);
    }
    repaint();
}

void PianoRoll::mouseUp(const juce::MouseEvent&)
{
    const bool wasDraggingPlayhead = draggingPlayhead_;
    const bool wasEraseDragging = eraseDragging_;
    draggingPlayhead_ = false;
    eraseDragging_ = false;
    velocityDragging_ = false;
    velocityDragNote_ = -1;
    draggingIdx_  = -1;
    resizing_     = false;
    boxSelecting_ = false;
    boxRect_      = {};
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

bool PianoRoll::keyPressed(const juce::KeyPress& key)
{
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
    if (key == juce::KeyPress('a', juce::ModifierKeys::ctrlModifier, 0))
    {
        selectedNotes_.clear();
        for (int i = 0; i < (int)notes_.size(); ++i) selectedNotes_.insert(i);
        repaint();
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
        auto grid = menu.reduced(22, 66).withTrimmedBottom(38);
        const int cols = grid.getWidth() >= 720 ? 3 : (grid.getWidth() >= 460 ? 2 : 1);
        const int gap = 10;
        const int rowH = grid.getHeight() < 300 ? 44 : 48;
        const int rows = (kMidiChoiceCount + cols - 1) / cols;
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

    auto grid = menu.reduced(22, 66).withTrimmedBottom(38);
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

    const int item = row * cols + col;
    return (item >= 0 && item < kMidiChoiceCount) ? item : -1;
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

    auto grid = r.reduced(22, 66).withTrimmedBottom(38);
    const int cols = grid.getWidth() >= 720 ? 3 : (grid.getWidth() >= 460 ? 2 : 1);
    const int gap = 10;
    const int rowH = grid.getHeight() < 300 ? 44 : 48;
    const int colW = (grid.getWidth() - gap * (cols - 1)) / cols;
    const int rows = (kMidiChoiceCount + cols - 1) / cols;
    const int contentH = rows * rowH + juce::jmax(0, rows - 1) * gap;
    const int maxScroll = juce::jmax(0, contentH - grid.getHeight());
    midiMoodMenuScrollY_ = juce::jlimit(0, maxScroll, midiMoodMenuScrollY_);

    g.saveState();
    g.reduceClipRegion(grid);
    for (int i = 0; i < kMidiChoiceCount; ++i)
    {
        const int col = i % cols;
        const int rowIndex = i / cols;
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
    if (isPlaying_) repaint();  // drives the smooth interpolation
}
