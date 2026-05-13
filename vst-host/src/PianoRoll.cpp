#include "PianoRoll.h"
#include "PluginHost.h"
#include "Theme.h"

PianoRoll::PianoRoll(PluginHost& pluginHost) : pluginHost_(pluginHost)
{
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
    return juce::Rectangle<int>(0, HEADER_H + RULER_H, KEY_W, getHeight() - HEADER_H - RULER_H);
}

juce::Rectangle<int> PianoRoll::getGridRect() const
{
    return juce::Rectangle<int>(KEY_W, HEADER_H + RULER_H, getWidth() - KEY_W, getHeight() - HEADER_H - RULER_H);
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
        
        // Border
        g.setColour(juce::Colour(0xff431407));
        g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);
    }

    // ── Playhead (smooth interpolated cursor) ───────────────────
    float phase = 0.0f;
    if (isPlaying_ && stepMs_ > 1.0)
    {
        double elapsed = juce::Time::getMillisecondCounterHiRes() - lastTickMs_;
        phase = (float) juce::jlimit(0.0, 1.0, elapsed / stepMs_);
    }
    int playheadX = -1;
    if (isPlaying_ && playStep_ >= 0)
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
    if (isPlaying_ && playheadX >= grid.getX() && playheadX <= grid.getRight())
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
}

void PianoRoll::resized() {}

void PianoRoll::mouseDown(const juce::MouseEvent& e)
{
    auto grid = getGridRect();
    if (!grid.contains(e.x, e.y)) return;
    
    int existing = findNoteAt(e.x, e.y);
    
    if (e.mods.isRightButtonDown() || e.mods.isCtrlDown())
    {
        // Right-click delete
        if (existing >= 0)
        {
            notes_.erase(notes_.begin() + existing);
            repaint();
        }
        return;
    }
    
    if (existing >= 0)
    {
        // Drag existing note
        draggingIdx_ = existing;
        auto r = getNoteRect(notes_[existing]);
        // If near right edge → resize
        resizing_ = (e.x > r.getRight() - 6);
        dragStart_ = e.getPosition();
        dragStartStep_ = notes_[existing].startStep;
        dragStartPitch_ = notes_[existing].pitch;
        dragStartLen_ = notes_[existing].lengthSteps;
    }
    else
    {
        // Create new note
        int step = xToStep(e.x);
        int pitch = yToPitch(e.y);
        if (step < 0 || pitch < 0) return;
        
        notes_.push_back({ pitch, step, 4, 100 });
        draggingIdx_ = (int)notes_.size() - 1;
        resizing_ = true;
        dragStart_ = e.getPosition();
        dragStartStep_ = step;
        dragStartPitch_ = pitch;
        dragStartLen_ = 4;
        
        // Audio preview
        pluginHost_.playSynthTone(440.0 * std::pow(2.0, (pitch - 69) / 12.0), 0, 0.2, 0.6f);
        
        repaint();
    }
}

void PianoRoll::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingIdx_ < 0 || draggingIdx_ >= (int)notes_.size()) return;
    
    int sw = stepW();
    int dxSteps = (e.x - dragStart_.x) / sw;
    int dyRows = (e.y - dragStart_.y) / KEY_H;
    
    if (resizing_)
    {
        notes_[draggingIdx_].lengthSteps = juce::jmax(1, dragStartLen_ + dxSteps);
    }
    else
    {
        notes_[draggingIdx_].startStep = juce::jmax(0, dragStartStep_ + dxSteps);
        notes_[draggingIdx_].pitch = juce::jlimit(LOWEST_NOTE, HIGHEST_NOTE, dragStartPitch_ - dyRows);
    }
    repaint();
}

void PianoRoll::mouseUp(const juce::MouseEvent&)
{
    draggingIdx_ = -1;
    resizing_ = false;
    
    if (onNotesChanged)
        onNotesChanged();
}

void PianoRoll::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
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
    playStep_  = currentStep;
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

void PianoRoll::timerCallback()
{
    if (isPlaying_) repaint();  // drives the smooth interpolation
}
