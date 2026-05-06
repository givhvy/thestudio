#include "Mixer.h"
#include "PluginHost.h"
#include "Theme.h"

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
    
    auto btnX = juce::Rectangle<float>((float)rx - 16, 6, 14, 14);
    g.setColour(Theme::text5);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
    g.drawText("X", btnX.toNearestInt(), juce::Justification::centred);
    rx -= 22;
    
    auto btnRoute = juce::Rectangle<float>((float)rx - 44, 5, 42, 16);
    rx = (int)btnRoute.getX() - 4;
    g.setColour(Theme::bg7);
    g.fillRoundedRectangle(btnRoute, 2.0f);
    g.setColour(Theme::bg8);
    g.drawRoundedRectangle(btnRoute, 2.0f, 1.0f);
    g.setColour(Theme::text4);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
    g.drawText("Route", btnRoute.toNearestInt(), juce::Justification::centred);
    
    auto btnWide = juce::Rectangle<float>((float)rx - 38, 5, 36, 16);
    g.setColour(Theme::bg7);
    g.fillRoundedRectangle(btnWide, 2.0f);
    g.setColour(Theme::bg8);
    g.drawRoundedRectangle(btnWide, 2.0f, 1.0f);
    g.setColour(Theme::text4);
    g.drawText("Wide", btnWide.toNearestInt(), juce::Justification::centred);
    
    // ── Channel Strips ──────────────────────────────────────
    int stripsAreaWidth = getWidth() - DETAIL_PANEL_WIDTH;
    
    for (size_t i = 0; i < tracks_.size(); ++i)
    {
        auto& tr = tracks_[i];
        auto stripRect = getStripRect((int)i);
        if (stripRect.getX() >= stripsAreaWidth) break;
        
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
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.0f));
        g.drawText(tr.name, stripRect.getX() + 2, sy, stripRect.getWidth() - 4, 12, 
                   juce::Justification::centred);
        sy += 14;
        
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
                (float)faderRect.getX() - 12 + ch * 5, (float)faderRect.getY(),
                4.0f, (float)faderRect.getHeight()
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
            (float)faderRect.getCentreX() - 6, handleY - 4, 12.0f, 8.0f
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
        
        // Reverb send (HORIZONTAL slider, 30px wide, purple)
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
    
    // ── Detail Panel (right side, 180px) ─────────────────────
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
    
    // 8 plugin slots
    int slotY = detailRect.getY() + 46;
    for (int s = 0; s < 8; ++s)
    {
        auto slotRect = juce::Rectangle<int>(detailRect.getX(), slotY, detailRect.getWidth(), 22);
        g.setColour(juce::Colour(0xff1a1a1c));
        g.drawHorizontalLine(slotRect.getBottom() - 1, (float)slotRect.getX(), (float)slotRect.getRight());
        
        g.setColour(Theme::text7);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
        g.drawText("> Slot " + juce::String(s + 1) + " (empty)",
                   slotRect.reduced(8, 0), juce::Justification::centredLeft);
        
        // Status dot
        auto dot = juce::Rectangle<float>((float)slotRect.getRight() - 16, 
                                            (float)slotRect.getCentreY() - 4, 8.0f, 8.0f);
        g.setColour(Theme::bg7);
        g.fillEllipse(dot);
        g.setColour(Theme::bg8);
        g.drawEllipse(dot, 1.0f);
        
        slotY += 22;
    }
    
    // Bottom: Out 1 — Out 2
    g.setColour(Theme::bg7);
    g.drawHorizontalLine(detailRect.getBottom() - 24, (float)detailRect.getX(), (float)detailRect.getRight());
    g.setColour(Theme::text6);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
    g.drawText("Out 1 — Out 2", detailRect.getX() + 8, detailRect.getBottom() - 22,
               detailRect.getWidth() - 16, 14, juce::Justification::centredLeft);
}

void Mixer::resized() {}

juce::Rectangle<int> Mixer::getStripRect(int idx) const
{
    return juce::Rectangle<int>(idx * STRIP_WIDTH, HEADER_HEIGHT, STRIP_WIDTH, getHeight() - HEADER_HEIGHT);
}

juce::Rectangle<int> Mixer::getPanKnobRect(int idx) const
{
    auto strip = getStripRect(idx);
    int knobSize = 28;
    return juce::Rectangle<int>(strip.getCentreX() - knobSize / 2, strip.getY() + 30, knobSize, knobSize);
}

juce::Rectangle<int> Mixer::getFaderRect(int idx) const
{
    auto strip = getStripRect(idx);
    return juce::Rectangle<int>(strip.getCentreX() - 8, strip.getY() + 80, 16, FADER_HEIGHT);
}

juce::Rectangle<int> Mixer::getReverbRect(int idx) const
{
    auto strip = getStripRect(idx);
    auto fader = getFaderRect(idx);
    return juce::Rectangle<int>(strip.getCentreX() - 15, fader.getBottom() + 4, 30, 4);
}

juce::Rectangle<int> Mixer::getMuteRect(int idx) const
{
    auto strip = getStripRect(idx);
    auto fader = getFaderRect(idx);
    int btnSize = 18;
    int y = fader.getBottom() + 36;
    return juce::Rectangle<int>(strip.getCentreX() - btnSize - 1, y, btnSize, 14);
}

juce::Rectangle<int> Mixer::getSoloRect(int idx) const
{
    auto strip = getStripRect(idx);
    auto fader = getFaderRect(idx);
    int btnSize = 18;
    int y = fader.getBottom() + 36;
    return juce::Rectangle<int>(strip.getCentreX() + 1, y, btnSize, 14);
}

void Mixer::mouseDown(const juce::MouseEvent& e)
{
    int trackIdx = e.x / STRIP_WIDTH;
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
        return;
    }
    
    if (getSoloRect(trackIdx).contains(e.x, e.y))
    {
        track.solo = !track.solo;
        repaint();
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
    }
    else if (dragTarget_ == DragTarget::ReverbSend)
    {
        int delta = e.x - dragStartY_;
        track.reverbSend = juce::jlimit(0.0f, 1.0f, dragStartValue_ + delta / 60.0f);
        pluginHost_.setSynthReverbWetLevel(track.reverbSend);
        pluginHost_.setSynthReverbEnabled(true);
        repaint();
    }
    else if (dragTarget_ == DragTarget::Pan)
    {
        int delta = dragStartY_ - e.y;
        track.pan = juce::jlimit(-1.0f, 1.0f, dragStartValue_ + delta / 100.0f);
        repaint();
    }
}
