#include "Mixer.h"
#include "PluginHost.h"
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

Mixer::Mixer(PluginHost& pluginHost)
    : pluginHost_(pluginHost)
{
    const char* names[] = { "Kick", "Snare", "Hihat", "Clap", "Perc", "Bass", "Lead", "Pad",
                            "Track 9", "Track 10", "Track 11", "Track 12", "Track 13", "Reverb", "Master" };
    for (int i = 0; i < 15; ++i)
    {
        Track t;
        t.name = names[i];
        tracks_.push_back(t);
    }
    selectedStrip_ = (int)tracks_.size() - 1; // Master selected by default
    pluginHost_.setMasterTrackIdx((int)tracks_.size() - 1);
}

Mixer::~Mixer() = default;

void Mixer::paint(juce::Graphics& g)
{
    // Mixer background
    g.fillAll(Theme::bg3);
    
    // ── Header ──────────────────────────────────────────────
    auto headerRect = juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);
    g.setColour(Theme::bg1);
    g.fillRect(headerRect);
    g.setColour(Theme::bg7);
    g.drawHorizontalLine(HEADER_HEIGHT - 1, 0.0f, (float)getWidth());
    
    g.setColour(Theme::text4);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
    g.drawText("MIXER", 12, 0, 100, HEADER_HEIGHT, juce::Justification::centredLeft);
    
    // Wide / Route buttons in header (right side)
    int rx = getWidth() - 8;
    
    btnXRect_ = juce::Rectangle<float>((float)rx - 16, 6, 14, 14);
    if (hoveredHeaderBtn_ == 0)
    {
        g.setColour (juce::Colour(0x33ef4444));
        g.fillRoundedRectangle (btnXRect_.expanded (2.0f), 3.0f);
    }
    g.setColour(hoveredHeaderBtn_ == 0 ? juce::Colour(0xffef4444) : Theme::text5);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
    g.drawText("X", btnXRect_.toNearestInt(), juce::Justification::centred);
    rx -= 22;
    
    btnRouteRect_ = juce::Rectangle<float>((float)rx - 44, 5, 42, 16);
    rx = (int)btnRouteRect_.getX() - 4;
    g.setColour(hoveredHeaderBtn_ == 2 ? Theme::bg8.brighter (0.3f) : Theme::bg7);
    g.fillRoundedRectangle(btnRouteRect_, 2.0f);
    g.setColour(hoveredHeaderBtn_ == 2 ? Theme::orange2 : Theme::bg8);
    g.drawRoundedRectangle(btnRouteRect_, 2.0f, 1.0f);
    g.setColour(hoveredHeaderBtn_ == 2 ? Theme::orange2 : Theme::text4);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
    g.drawText("Route", btnRouteRect_.toNearestInt(), juce::Justification::centred);
    
    btnWideRect_ = juce::Rectangle<float>((float)rx - 38, 5, 36, 16);
    auto wideFill = wideMode_ ? Theme::orange2.withAlpha(0.25f)
                              : (hoveredHeaderBtn_ == 1 ? Theme::bg8.brighter (0.3f) : Theme::bg7);
    g.setColour(wideFill);
    g.fillRoundedRectangle(btnWideRect_, 2.0f);
    g.setColour((wideMode_ || hoveredHeaderBtn_ == 1) ? Theme::orange2 : Theme::bg8);
    g.drawRoundedRectangle(btnWideRect_, 2.0f, 1.0f);
    g.setColour((wideMode_ || hoveredHeaderBtn_ == 1) ? Theme::orange2 : Theme::text4);
    g.drawText("Wide", btnWideRect_.toNearestInt(), juce::Justification::centred);
    
    // ── Channel Strips ──────────────────────────────────────
    int detailW = wideMode_ ? 0 : DETAIL_PANEL_WIDTH;
    int stripsAreaWidth = getWidth() - detailW;
    
    for (size_t i = 0; i < tracks_.size(); ++i)
    {
        auto& tr = tracks_[i];
        auto stripRect = getStripRect((int)i);
        if (stripRect.getX() >= stripsAreaWidth) continue; // off-screen insert; master is at x=0
        
        bool isMaster = (i == tracks_.size() - 1);
        bool isSelected = ((int)i == selectedStrip_);
        
        // Strip background
        if (isSelected) g.setColour(Theme::bg6);
        else if (isMaster) g.setColour(juce::Colour(0xff16161a));
        else g.setColour(Theme::bg3);
        g.fillRect(stripRect);
        
        // Selected: orange top border (2px)
        if (isSelected)
        {
            g.setColour(Theme::orange2);
            g.fillRect(stripRect.getX(), stripRect.getY(), stripRect.getWidth(), 2);
        }
        
        // Right divider (1px)
        g.setColour(Theme::bg6);
        g.drawVerticalLine(stripRect.getRight() - 1, (float)stripRect.getY(), (float)stripRect.getBottom());
        
        int sy = stripRect.getY() + (isSelected ? 4 : 4);
        
        // Strip number (or "M" for master)
        g.setColour(isMaster ? Theme::orange2 : Theme::text6);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
        juce::String num = isMaster ? "M" : juce::String((int)i + 1);
        g.drawText(num, stripRect.getX(), sy, stripRect.getWidth(), 11, juce::Justification::centred);
        sy += 13;
        
        // Track name
        g.setColour(isSelected ? Theme::orange2 : Theme::text5);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(tr.name, stripRect.getX() + 2, sy, stripRect.getWidth() - 4, 14,
                   juce::Justification::centred);
        sy += 16;
        
        // Pan knob (28x28)
        auto panRect = getPanKnobRect((int)i);
        // Outer ring radial gradient
        juce::ColourGradient panGrad(juce::Colour(0xff3a3a3e), 
                                       panRect.getX() + panRect.getWidth() * 0.4f,
                                       panRect.getY() + panRect.getHeight() * 0.35f,
                                       juce::Colour(0xff1a1a1e), 
                                       panRect.getRight(), panRect.getBottom(), true);
        g.setGradientFill(panGrad);
        g.fillEllipse(panRect.toFloat());
        g.setColour(Theme::bg8);
        g.drawEllipse(panRect.toFloat(), 1.0f);
        
        // Pan indicator line
        float panAngle = (tr.pan) * 135.0f * juce::MathConstants<float>::pi / 180.0f;
        float lineX1 = panRect.getCentreX();
        float lineY1 = panRect.getCentreY();
        float lineX2 = lineX1 + std::sin(panAngle) * (panRect.getWidth() / 2 - 3);
        float lineY2 = lineY1 - std::cos(panAngle) * (panRect.getHeight() / 2 - 3);
        g.setColour(tr.pan == 0 ? Theme::blue : Theme::orange2);
        g.drawLine(lineX1, lineY1, lineX2, lineY2, 2.0f);
        
        sy += panRect.getHeight() + 2;
        
        // Pan readout
        g.setColour(Theme::text6);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.0f));
        juce::String panText = tr.pan > 0 ? ("R" + juce::String((int)(tr.pan * 100))) :
                                tr.pan < 0 ? ("L" + juce::String((int)(-tr.pan * 100))) : "C";
        g.drawText(panText, stripRect.getX(), sy, stripRect.getWidth(), 10, juce::Justification::centred);
        sy += 12;
        
        // VU Meter + Fader area
        auto faderRect = getFaderRect((int)i);
        
        // VU meters (2 thin bars on left)
        for (int ch = 0; ch < 2; ++ch)
        {
            auto vuRect = juce::Rectangle<float>(
                (float)faderRect.getX() - 16 + ch * 6, (float)faderRect.getY(),
                5.0f, (float)faderRect.getHeight()
            );
            // Background (sunken)
            g.setColour(Theme::bg1);
            g.fillRoundedRectangle(vuRect, 2.0f);
        }
        
        // Fader track (vertical)
        int faderTrackX = faderRect.getCentreX() - 1;
        g.setColour(Theme::bg1);
        g.fillRect(faderTrackX, faderRect.getY(), 2, faderRect.getHeight());
        
        // Fader fill (volume level)
        float fillH = faderRect.getHeight() * tr.volume;
        juce::Rectangle<float> fillRect(
            (float)faderTrackX, (float)faderRect.getBottom() - fillH,
            2.0f, fillH
        );
        g.setColour(isMaster ? Theme::orange2 : Theme::blue);
        g.fillRect(fillRect);
        
        // Fader handle
        float handleY = faderRect.getBottom() - fillH;
        auto handleRect = juce::Rectangle<float>(
            (float)faderRect.getCentreX() - 9, handleY - 6, 18.0f, 12.0f
        );
        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(handleRect.translated(0, 1), 2.0f);
        // Body
        juce::ColourGradient handleGrad(
            isMaster ? Theme::orange1 : juce::Colour(0xffeeeeee),
            handleRect.getX(), handleRect.getY(),
            isMaster ? Theme::orange3 : juce::Colour(0xff999999),
            handleRect.getX(), handleRect.getBottom(), false
        );
        g.setGradientFill(handleGrad);
        g.fillRoundedRectangle(handleRect, 2.0f);
        g.setColour(juce::Colour(0xff111111));
        g.drawRoundedRectangle(handleRect, 2.0f, 1.0f);
        
        sy = faderRect.getBottom() + 4;
        
        // Reverb send (HORIZONTAL slider, 44px wide, purple)
        auto reverbRect = getReverbRect((int)i);
        // Track
        g.setColour(Theme::bg1);
        g.fillRoundedRectangle(reverbRect.toFloat(), 2.0f);
        // Fill
        float reverbFill = reverbRect.getWidth() * tr.reverbSend;
        g.setColour(Theme::purple);
        g.fillRoundedRectangle(reverbRect.getX(), reverbRect.getY(), 
                                (int)reverbFill, reverbRect.getHeight(), 2.0f);
        // Handle
        float reverbHandleX = reverbRect.getX() + reverbFill;
        auto reverbHandle = juce::Rectangle<float>(
            reverbHandleX - 3, (float)reverbRect.getY() - 1, 6.0f, (float)reverbRect.getHeight() + 2
        );
        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(reverbHandle, 1.0f);
        
        sy = reverbRect.getBottom() + 2;
        
        // Reverb send readout
        g.setColour(Theme::text6);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(7.0f));
        g.drawText("Rvb: " + juce::String((int)(tr.reverbSend * 100)) + "%",
                   stripRect.getX(), sy, stripRect.getWidth(), 9, juce::Justification::centred);
        sy += 10;
        
        // Volume readout
        g.setColour(Theme::text6);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.0f));
        g.drawText(juce::String((int)(tr.volume * 100)) + "%",
                   stripRect.getX(), sy, stripRect.getWidth(), 10, juce::Justification::centred);
        sy += 12;
        
        // Mute / Solo buttons
        auto muteRect = getMuteRect((int)i).toFloat();
        auto soloRect = getSoloRect((int)i).toFloat();
        
        // Mute
        g.setColour(tr.muted ? Theme::yellow2 : Theme::bg7);
        g.fillRoundedRectangle(muteRect, 2.0f);
        g.setColour(Theme::bg8);
        g.drawRoundedRectangle(muteRect, 2.0f, 1.0f);
        g.setColour(tr.muted ? juce::Colours::black : Theme::text5);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.0f).withStyle("Bold"));
        g.drawText("M", muteRect.toNearestInt(), juce::Justification::centred);
        
        // Solo
        g.setColour(tr.solo ? Theme::green2 : Theme::bg7);
        g.fillRoundedRectangle(soloRect, 2.0f);
        g.setColour(Theme::bg8);
        g.drawRoundedRectangle(soloRect, 2.0f, 1.0f);
        g.setColour(tr.solo ? juce::Colours::black : Theme::text5);
        g.drawText("S", soloRect.toNearestInt(), juce::Justification::centred);
    }

    // Vertical separator between the pinned Master strip and the Insert strips.
    {
        int sepX = STRIP_WIDTH + MASTER_GAP / 2;
        g.setColour(Theme::bg8);
        g.drawVerticalLine(sepX, (float) HEADER_HEIGHT, (float) getHeight());
    }
    
    // ── Detail Panel (right side, 180px) ─────────────────────
    if (wideMode_) return; // wide mode hides the detail panel entirely
    auto detailRect = juce::Rectangle<int>(getWidth() - DETAIL_PANEL_WIDTH, HEADER_HEIGHT,
                                             DETAIL_PANEL_WIDTH, getHeight() - HEADER_HEIGHT);
    g.setColour(juce::Colour(0xff0f0f11));
    g.fillRect(detailRect);
    g.setColour(Theme::bg7);
    g.drawVerticalLine(detailRect.getX(), (float)detailRect.getY(), (float)detailRect.getBottom());
    
    // Selected track name header
    if (selectedStrip_ >= 0 && selectedStrip_ < (int)tracks_.size())
    {
        g.setColour(Theme::text4);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(tracks_[selectedStrip_].name, detailRect.getX() + 8, detailRect.getY() + 6,
                   detailRect.getWidth() - 16, 14, juce::Justification::centredLeft);
    }
    g.setColour(Theme::bg7);
    g.drawHorizontalLine(detailRect.getY() + 26, (float)detailRect.getX(), (float)detailRect.getRight());
    
    // FX Chain label
    g.setColour(Theme::text6);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
    g.drawText("FX Chain", detailRect.getX() + 8, detailRect.getY() + 30,
               detailRect.getWidth() - 16, 14, juce::Justification::centredLeft);
    
    // 8 plugin slots — show actual plugin name when assigned
    const Track* selTrack = (selectedStrip_ >= 0 && selectedStrip_ < (int)tracks_.size())
                                ? &tracks_[selectedStrip_] : nullptr;
    for (int s = 0; s < FX_SLOT_COUNT; ++s)
    {
        auto slotRect = getFxSlotRect(s);
        g.setColour(juce::Colour(0xff1a1a1c));
        g.drawHorizontalLine(slotRect.getBottom() - 1, (float)slotRect.getX(), (float)slotRect.getRight());

        bool filled = (selTrack && s < (int)selTrack->fxSlots.size());
        juce::String label;
        if (filled)
            label = "> " + selTrack->fxSlots[s].displayName + (selTrack->fxSlots[s].isWasm ? "  [W]" : "");
        else
            label = "> Slot " + juce::String(s + 1) + " (empty)";

        g.setColour(filled ? Theme::text3 : Theme::text7);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle(filled ? "Bold" : "Regular"));
        g.drawText(label, slotRect.reduced(8, 0), juce::Justification::centredLeft);

        // Status dot — lit orange if plugin loaded
        auto dot = juce::Rectangle<float>((float)slotRect.getRight() - 16,
                                            (float)slotRect.getCentreY() - 4, 8.0f, 8.0f);
        g.setColour(filled ? Theme::orange2 : Theme::bg7);
        g.fillEllipse(dot);
        g.setColour(Theme::bg8);
        g.drawEllipse(dot, 1.0f);
    }
    
    // Bottom: Out 1 — Out 2
    g.setColour(Theme::bg7);
    g.drawHorizontalLine(detailRect.getBottom() - 24, (float)detailRect.getX(), (float)detailRect.getRight());
    g.setColour(Theme::text6);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
    juce::String outText = "Out 1 — Out 2";
    if (selectedStrip_ >= 0 && selectedStrip_ < (int) tracks_.size())
    {
        int rt = tracks_[selectedStrip_].routeTo;
        if (rt < 0) rt = (int) tracks_.size() - 1; // master
        if (rt >= 0 && rt < (int) tracks_.size() && rt != selectedStrip_)
            outText = "Out → " + tracks_[rt].name;
    }
    g.drawText(outText, detailRect.getX() + 8, detailRect.getBottom() - 22,
               detailRect.getWidth() - 16, 14, juce::Justification::centredLeft);
}

void Mixer::resized() {}

juce::Rectangle<int> Mixer::getStripRect(int idx) const
{
    // FL-Studio layout: Master strip (last in array) is pinned to column 0
    // with a small gap; the rest are shifted right by one strip + gap.
    const int masterIdx = (int) tracks_.size() - 1;
    int col;
    if (idx == masterIdx)
        col = 0;
    else
        col = idx + 1; // 1..N-1 inserts shifted right by master column

    int x = col * STRIP_WIDTH;
    if (idx != masterIdx) x += MASTER_GAP; // visual gap after master
    return juce::Rectangle<int>(x, HEADER_HEIGHT, STRIP_WIDTH, getHeight() - HEADER_HEIGHT);
}

juce::Rectangle<int> Mixer::getPanKnobRect(int idx) const
{
    auto strip = getStripRect(idx);
    int knobSize = 38;
    return juce::Rectangle<int>(strip.getCentreX() - knobSize / 2, strip.getY() + 34, knobSize, knobSize);
}

juce::Rectangle<int> Mixer::getFaderRect(int idx) const
{
    auto strip = getStripRect(idx);
    return juce::Rectangle<int>(strip.getCentreX() - 11, strip.getY() + 96, 22, FADER_HEIGHT);
}

juce::Rectangle<int> Mixer::getReverbRect(int idx) const
{
    auto strip = getStripRect(idx);
    auto fader = getFaderRect(idx);
    return juce::Rectangle<int>(strip.getCentreX() - 22, fader.getBottom() + 6, 44, 5);
}

juce::Rectangle<int> Mixer::getMuteRect(int idx) const
{
    auto strip = getStripRect(idx);
    auto fader = getFaderRect(idx);
    int btnSize = 24;
    int y = fader.getBottom() + 44;
    return juce::Rectangle<int>(strip.getCentreX() - btnSize - 2, y, btnSize, 18);
}

juce::Rectangle<int> Mixer::getSoloRect(int idx) const
{
    auto strip = getStripRect(idx);
    auto fader = getFaderRect(idx);
    int btnSize = 24;
    int y = fader.getBottom() + 44;
    return juce::Rectangle<int>(strip.getCentreX() + 2, y, btnSize, 18);
}

void Mixer::mouseDown(const juce::MouseEvent& e)
{
    // ── Header buttons (X / Wide / Route) ────────────────────
    juce::Point<float> p ((float) e.x, (float) e.y);
    if (btnXRect_.contains (p))
    {
        if (onClose) onClose();
        return;
    }
    if (btnWideRect_.contains (p))
    {
        setWideMode (! wideMode_);
        return;
    }
    if (btnRouteRect_.contains (p))
    {
        if (selectedStrip_ < 0 || selectedStrip_ >= (int) tracks_.size()) return;
        juce::PopupMenu m;
        const int from = selectedStrip_;
        const int currentTo = tracks_[from].routeTo < 0 ? (int) tracks_.size() - 1 : tracks_[from].routeTo;
        for (int i = 0; i < (int) tracks_.size(); ++i)
        {
            if (i == from) continue; // can't route to self
            m.addItem (i + 1, tracks_[i].name, true, i == currentTo);
        }
        m.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent (this),
            [this, from] (int chosen)
            {
                if (chosen <= 0) return;
                tracks_[from].routeTo = chosen - 1;
                repaint();
                if (onTracksChanged) onTracksChanged();
            });
        return;
    }

    // ── Detail panel: FX slot interaction ─────────────────────
    auto detail = getDetailPanelRect();
    if (! wideMode_ && detail.contains(e.x, e.y))
    {
        for (int s = 0; s < FX_SLOT_COUNT; ++s)
        {
            if (!getFxSlotRect(s).contains(e.x, e.y)) continue;
            if (selectedStrip_ < 0 || selectedStrip_ >= (int)tracks_.size()) return;
            auto& track = tracks_[selectedStrip_];
            bool filled = (s < (int)track.fxSlots.size());
            if (e.mods.isRightButtonDown())
            {
                if (filled) { removeFxFromTrack(selectedStrip_, s); repaint(); }
            }
            else if (filled)
            {
                int pid = track.fxSlots[s].pluginSlotId;
                if (pid >= 0) pluginHost_.showEditor(pid, true);
            }
            else
            {
                openPluginPickerForTrack(selectedStrip_);
            }
            return;
        }
        return; // click in detail panel but not on a slot
    }

    // ── Channel strip area ────────────────────────────────────
    int stripsAreaWidth = getWidth() - (wideMode_ ? 0 : DETAIL_PANEL_WIDTH);
    if (e.x >= stripsAreaWidth) return;

    // Map click x → track index. Master is pinned to col 0; inserts start
    // after MASTER_GAP at col 1.
    int trackIdx = -1;
    if (e.x < STRIP_WIDTH)
    {
        trackIdx = (int) tracks_.size() - 1; // master
    }
    else if (e.x >= STRIP_WIDTH + MASTER_GAP)
    {
        int col = (e.x - STRIP_WIDTH - MASTER_GAP) / STRIP_WIDTH; // 0-based insert column
        trackIdx = col; // insert idx 0..N-2 maps directly
    }
    if (trackIdx < 0 || trackIdx >= (int)tracks_.size()) return;

    selectedStrip_ = trackIdx;
    
    auto& track = tracks_[trackIdx];
    
    if (getFaderRect(trackIdx).expanded(8, 0).contains(e.x, e.y))
    {
        draggingTrackIdx_ = trackIdx;
        dragTarget_ = DragTarget::Volume;
        dragStartY_ = e.y;
        dragStartValue_ = track.volume;
        return;
    }
    
    if (getReverbRect(trackIdx).expanded(0, 4).contains(e.x, e.y))
    {
        draggingTrackIdx_ = trackIdx;
        dragTarget_ = DragTarget::ReverbSend;
        dragStartY_ = e.x;  // horizontal drag
        dragStartValue_ = track.reverbSend;
        return;
    }
    
    if (getPanKnobRect(trackIdx).contains(e.x, e.y))
    {
        draggingTrackIdx_ = trackIdx;
        dragTarget_ = DragTarget::Pan;
        dragStartY_ = e.y;
        dragStartValue_ = track.pan;
        return;
    }
    
    if (getMuteRect(trackIdx).contains(e.x, e.y))
    {
        track.muted = !track.muted;
        repaint();
        if (onTracksChanged) onTracksChanged();
        return;
    }
    
    if (getSoloRect(trackIdx).contains(e.x, e.y))
    {
        track.solo = !track.solo;
        repaint();
        if (onTracksChanged) onTracksChanged();
        return;
    }
    
    repaint();
}

void Mixer::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingTrackIdx_ < 0 || draggingTrackIdx_ >= (int)tracks_.size()) return;
    
    auto& track = tracks_[draggingTrackIdx_];
    
    if (dragTarget_ == DragTarget::Volume)
    {
        int delta = dragStartY_ - e.y;
        track.volume = juce::jlimit(0.0f, 1.0f, dragStartValue_ + delta / 200.0f);
        repaint();
        if (onTracksChanged) onTracksChanged();
    }
    else if (dragTarget_ == DragTarget::ReverbSend)
    {
        int delta = e.x - dragStartY_;
        track.reverbSend = juce::jlimit(0.0f, 1.0f, dragStartValue_ + delta / 60.0f);
        pluginHost_.setSynthReverbWetLevel(track.reverbSend);
        pluginHost_.setSynthReverbEnabled(true);
        repaint();
        if (onTracksChanged) onTracksChanged();
    }
    else if (dragTarget_ == DragTarget::Pan)
    {
        int delta = dragStartY_ - e.y;
        track.pan = juce::jlimit(-1.0f, 1.0f, dragStartValue_ + delta / 100.0f);
        repaint();
        if (onTracksChanged) onTracksChanged();
    }
}

void Mixer::mouseMove(const juce::MouseEvent& e)
{
    juce::Point<float> p ((float) e.x, (float) e.y);
    int newHover = -1;
    if      (btnXRect_.contains (p))     newHover = 0;
    else if (btnWideRect_.contains (p))  newHover = 1;
    else if (btnRouteRect_.contains (p)) newHover = 2;

    if (newHover != hoveredHeaderBtn_)
    {
        hoveredHeaderBtn_ = newHover;
        setMouseCursor (newHover >= 0 ? juce::MouseCursor::PointingHandCursor
                                      : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void Mixer::mouseExit(const juce::MouseEvent&)
{
    if (hoveredHeaderBtn_ != -1)
    {
        hoveredHeaderBtn_ = -1;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

juce::Rectangle<int> Mixer::getDetailPanelRect() const
{
    if (wideMode_) return {};
    return juce::Rectangle<int>(getWidth() - DETAIL_PANEL_WIDTH, HEADER_HEIGHT,
                                 DETAIL_PANEL_WIDTH, getHeight() - HEADER_HEIGHT);
}

juce::Rectangle<int> Mixer::getFxSlotRect(int slotIdx) const
{
    auto detail = getDetailPanelRect();
    int slotY = detail.getY() + 46 + slotIdx * FX_SLOT_H;
    return juce::Rectangle<int>(detail.getX(), slotY, detail.getWidth(), FX_SLOT_H);
}

void Mixer::setTrackVolume(int i, float v)
{
    if (i < 0 || i >= (int)tracks_.size()) return;
    tracks_[i].volume = juce::jlimit(0.0f, 1.0f, v);
    repaint();
    if (onTracksChanged) onTracksChanged();
}

// Push the ordered list of plugin slot ids for trackIdx down to PluginHost
// so the audio engine routes that track's voices through them.
static void pushTrackChain(PluginHost& host, int trackIdx, const std::vector<Mixer::FxSlot>& slots)
{
    std::vector<int> ids;
    for (auto& s : slots)
        if (s.pluginSlotId >= 0) ids.push_back(s.pluginSlotId);
    host.setTrackChain(trackIdx, std::move(ids));
}

int Mixer::addFxToTrack(int trackIdx, int pluginSlotId, const juce::String& displayName, bool isWasm)
{
    if (trackIdx < 0 || trackIdx >= (int)tracks_.size()) return -1;
    auto& slots = tracks_[trackIdx].fxSlots;
    if ((int)slots.size() >= FX_SLOT_COUNT) return -1;
    FxSlot fs;
    fs.pluginSlotId = pluginSlotId;
    fs.displayName = displayName;
    fs.isWasm = isWasm;
    slots.push_back(fs);
    pushTrackChain(pluginHost_, trackIdx, slots);
    repaint();
    return (int)slots.size() - 1;
}

void Mixer::removeFxFromTrack(int trackIdx, int slotIdx)
{
    if (trackIdx < 0 || trackIdx >= (int)tracks_.size()) return;
    auto& slots = tracks_[trackIdx].fxSlots;
    if (slotIdx < 0 || slotIdx >= (int)slots.size()) return;
    int pid = slots[slotIdx].pluginSlotId;
    if (pid >= 0) pluginHost_.unloadPlugin(pid);
    slots.erase(slots.begin() + slotIdx);
    pushTrackChain(pluginHost_, trackIdx, slots);
}

void Mixer::openPluginPickerForTrack(int trackIdx)
{
    if (trackIdx < 0 || trackIdx >= (int)tracks_.size()) return;

    // Lazily scan default plugin folders the first time we need the picker.
    auto& known = pluginHost_.getKnownPluginList();
    if (known.getNumTypes() == 0)
        pluginHost_.scanDefaultLocations();

    // ── Build the popup menu ─────────────────────────────────────
    juce::PopupMenu menu;
    juce::PopupMenu effects, instruments;
    auto types = pluginHost_.getKnownPluginList().getTypes();

    // Sort alphabetically by display name
    std::sort(types.begin(), types.end(),
              [](const juce::PluginDescription& a, const juce::PluginDescription& b)
              { return a.name.compareIgnoreCase(b.name) < 0; });

    // Map menu IDs (1..N) → plugin index
    std::vector<juce::PluginDescription> indexed;
    int id = 1;
    for (const auto& d : types)
    {
        const auto label = d.name + "  [" + d.pluginFormatName + "]";
        if (d.isInstrument) instruments.addItem(id, label);
        else                effects.addItem(id, label);
        indexed.push_back(d);
        ++id;
    }

    if (instruments.getNumItems() > 0) menu.addSubMenu("Instruments", instruments);
    if (effects.getNumItems() > 0)     menu.addSubMenu("Effects",     effects);
    if (menu.getNumItems() == 0)
        menu.addItem(juce::PopupMenu::Item("(no plugins found - scan paths)").setEnabled(false));
    menu.addSeparator();
    menu.addItem(9001, "Browse for .vst3 / .dll...");
    menu.addItem(9002, "Re-scan plugin folders");

    // Anchor the menu just below the slot rect we clicked on
    auto trackR = trackIdx; // captured for the lambda
    auto screenArea = localAreaToGlobal(getFxSlotRect(0)); // fallback if no specific slot known
    // Use the empty-slot row (first one with index == current fxSlots.size())
    if (trackIdx >= 0 && trackIdx < (int)tracks_.size())
    {
        int s = (int)tracks_[trackIdx].fxSlots.size();
        if (s >= 0 && s < FX_SLOT_COUNT)
            screenArea = localAreaToGlobal(getFxSlotRect(s));
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options{}
            .withTargetScreenArea(screenArea)
            .withMinimumWidth(220)
            .withStandardItemHeight(26),
        [this, trackR, indexed](int chosen)
        {
            if (chosen <= 0) return;

            if (chosen == 9001)
            {
                // Old behaviour: pick a file
                fileChooser_ = std::make_unique<juce::FileChooser>(
                    "Load VST3 / VST plugin",
                    juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory),
                    "*.vst3;*.dll");
                fileChooser_->launchAsync(
                    juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this, trackR](const juce::FileChooser& fc)
                    {
                        auto file = fc.getResult();
                        if (!file.existsAsFile()) return;
                        juce::String err;
                        int slotId = pluginHost_.loadPlugin(file.getFullPathName(), err);
                        if (slotId < 0)
                        {
                            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                "Plugin load failed",
                                err.isNotEmpty() ? err : juce::String("Could not load ") + file.getFileName());
                            return;
                        }
                        addFxToTrack(trackR, slotId, file.getFileNameWithoutExtension(), false);
                        pluginHost_.showEditor(slotId, true);
                    });
                return;
            }

            if (chosen == 9002)
            {
                pluginHost_.scanDefaultLocations();
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                    "Plugin scan",
                    "Found " + juce::String(pluginHost_.getKnownPluginList().getNumTypes())
                        + " plugins.");
                return;
            }

            // Picked an entry: chosen is 1-based index into `indexed`
            int idx = chosen - 1;
            if (idx < 0 || idx >= (int)indexed.size()) return;

            juce::String err;
            int slotId = pluginHost_.loadPlugin(indexed[idx].fileOrIdentifier, err);
            if (slotId < 0)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Plugin load failed",
                    err.isNotEmpty() ? err : juce::String("Could not load ") + indexed[idx].name);
                return;
            }
            addFxToTrack(trackR, slotId, indexed[idx].name, false);
            pluginHost_.showEditor(slotId, true);
        });
}

// ─── Project I/O ─────────────────────────────────────────────────────
juce::var Mixer::toJson() const
{
    auto* obj = new juce::DynamicObject();
    juce::Array<juce::var> arr;
    for (const auto& t : tracks_)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("name",       t.name);
        o->setProperty("volume",     t.volume);
        o->setProperty("pan",        t.pan);
        o->setProperty("reverbSend", t.reverbSend);
        o->setProperty("muted",      t.muted);
        o->setProperty("solo",       t.solo);
        arr.add(juce::var(o));
    }
    obj->setProperty("tracks", arr);
    return juce::var(obj);
}

void Mixer::fromJson(const juce::var& v)
{
    if (!v.isObject()) return;
    auto* arr = v.getProperty("tracks", juce::var()).getArray();
    if (!arr) return;

    tracks_.clear();
    for (auto& tv : *arr)
    {
        Track t;
        t.name       = tv.getProperty("name", "Track").toString();
        t.volume     = (float)(double)tv.getProperty("volume",     0.8);
        t.pan        = (float)(double)tv.getProperty("pan",        0.0);
        t.reverbSend = (float)(double)tv.getProperty("reverbSend", 0.0);
        t.muted      = (bool)tv.getProperty("muted", false);
        t.solo       = (bool)tv.getProperty("solo",  false);
        tracks_.push_back(std::move(t));
    }
    selectedStrip_ = tracks_.empty() ? -1 : 0;
    if (onTracksChanged) onTracksChanged();
    repaint();
}
