#include "PianoRoll.h"
#include "PluginHost.h"
#include "Theme.h"
#include <algorithm>

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

juce::Rectangle<int> PianoRoll::getGenerateMidiButtonRect() const
{
    return juce::Rectangle<int>(320, 4, 94, HEADER_H - 8);
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

    drawGenerateMidiMenu(g);
}

void PianoRoll::resized() {}

void PianoRoll::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    boxSelecting_ = false;
    draggingPlayhead_ = false;

    if (midiMoodMenuOpen_)
    {
        const int item = getGenerateMidiMenuItemAt(e.x, e.y);
        if (item >= 0)
        {
            static const juce::String moods[] = { "happy", "sad", "jazz", "dark", "chill", "trap", "rnb", "epic", "random" };
            generateMidiForMood(moods[item], e.mods.isMiddleButtonDown());
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

    // Plain right-click → delete the note under cursor
    if (right)
    {
        if (existing >= 0)
        {
            notes_.erase(notes_.begin() + existing);
            // Shift indices in selectedNotes_
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
    draggingPlayhead_ = false;
    draggingIdx_  = -1;
    resizing_     = false;
    boxSelecting_ = false;
    boxRect_      = {};
    dragStartSelected_.clear();
    dragStartSelectedIds_.clear();
    setMouseCursor(juce::MouseCursor::NormalCursor);

    if (!wasDraggingPlayhead && onNotesChanged) onNotesChanged();
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

void PianoRoll::showGenerateMidiMenu()
{
    midiMoodMenuOpen_ = !midiMoodMenuOpen_;
    midiMoodMenuHover_ = -1;
    repaint();
}

juce::Rectangle<int> PianoRoll::getGenerateMidiMenuRect() const
{
    auto button = getGenerateMidiButtonRect();
    return { button.getX(), button.getBottom() + 2, 306, 422 };
}

int PianoRoll::getGenerateMidiMenuItemAt(int x, int y) const
{
    auto menu = getGenerateMidiMenuRect();
    if (!menu.contains(x, y))
        return -1;

    const int itemY = y - menu.getY() - 54;
    if (itemY < 0)
        return -1;

    const int item = itemY / 39;
    return (item >= 0 && item < 9) ? item : -1;
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
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
    g.drawText("Generate MIDI Mood", r.getX() + 22, r.getY() + 18, r.getWidth() - 44, 18, juce::Justification::centredLeft);

    static const char* labels[] = { "Happy", "Sad", "Jazz", "Dark", "Chill", "Trap", "R&B", "Epic", "Random" };
    static const char* ids[] = { "happy", "sad", "jazz", "dark", "chill", "trap", "rnb", "epic", "random" };
    for (int i = 0; i < 9; ++i)
    {
        auto row = juce::Rectangle<int>(r.getX() + 1, r.getY() + 54 + i * 39, r.getWidth() - 2, 39);
        if (i == midiMoodMenuHover_)
        {
            g.setColour(Theme::accent);
            g.fillRect(row);
        }

        const int variant = midiMoodVariant_[ids[i]] + 1;
        g.setColour(i == midiMoodMenuHover_ ? juce::Colours::white : Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f));
        g.drawText(labels[i], row.getX() + 30, row.getY(), 130, row.getHeight(), juce::Justification::centredLeft);
        g.setColour(i == midiMoodMenuHover_ ? juce::Colours::white.withAlpha(0.8f) : Theme::zinc500);
        g.drawText("v" + juce::String(variant), row.getRight() - 56, row.getY(), 34, row.getHeight(), juce::Justification::centredRight);
    }

    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
    g.drawText("Left click loads. Middle click changes variation.", r.reduced(18, 0).removeFromBottom(26),
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
    const int variantCount = 5;
    if (nextVariant)
        midiMoodVariant_[mood] = (midiMoodVariant_[mood] + 1) % variantCount;
    const int variant = mood == "random" ? juce::Random::getSystemRandom().nextInt(variantCount)
                                         : midiMoodVariant_[mood] % variantCount;

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

        p.root = juce::jlimit(42, 72, p.root + rootShift[variant]);

        int oldDeg[16];
        int oldLen[16];
        std::copy(p.degrees, p.degrees + 16, oldDeg);
        std::copy(p.lengths, p.lengths + 16, oldLen);
        const int rotation = rotations[variant];
        for (int i = 0; i < 16; ++i)
        {
            const int src = (i + rotation) % 16;
            p.degrees[i] = juce::jlimit(0, 6, (oldDeg[src] + ((i + variant) % 3 == 0 ? variant : 0)) % 7);
            p.lengths[i] = juce::jlimit(1, 8, oldLen[src] + ((i + variant) % 5 == 0 ? 1 : 0) - ((i + variant) % 7 == 0 ? 1 : 0));
        }

        for (int i = 0; i < 4; ++i)
            p.chordDegrees[i] = chordSwaps[variant][i];

        p.swing = p.swing || variant == 2 || variant == 4;
        p.velocity = juce::jlimit(55, 124, p.velocity + (variant - 2) * 4);
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
