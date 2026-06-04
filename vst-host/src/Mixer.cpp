#include "Mixer.h"
#include "PluginHost.h"
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>

namespace
{
float gainToDb(float gain)
{
    if (gain <= 0.000001f)
        return -90.0f;
    return 20.0f * std::log10(gain);
}

juce::String formatDb(float db)
{
    if (db <= -89.0f)
        return "-inf dB";
    return (db > 0.0f ? "+" : "") + juce::String(db, 1) + " dB";
}

enum class MixRole { Kick, Bass, Snare, Hat, Perc, Loop, Instrument, Master, Other };

MixRole classifyTrackName(juce::String name)
{
    name = name.toLowerCase();
    if (name.contains("master")) return MixRole::Master;
    if (name.contains("kick")) return MixRole::Kick;
    if (name.contains("808") || name.contains("sub bass") || name.contains("subbass") || name.contains("bass")) return MixRole::Bass;
    if (name.contains("snare") || name.contains("clap") || name.contains("rim")) return MixRole::Snare;
    if (name.contains("hihat") || name.contains("hi hat") || name.contains("hat") || name.contains("ride") || name.contains("openhat") || name.contains("open hat")) return MixRole::Hat;
    if (name.contains("perc") || name.contains("vox")) return MixRole::Perc;
    if (name.contains("loop") || name.contains("sample")) return MixRole::Loop;
    if (name.contains("piano") || name.contains("guitar") || name.contains("keys") || name.contains("melody") || name.contains("lead") || name.contains("pad")) return MixRole::Instrument;
    return MixRole::Other;
}

std::array<float, 6> roleFrequencyProfile(MixRole role)
{
    switch (role)
    {
        case MixRole::Kick:       return { 0.86f, 0.64f, 0.35f, 0.22f, 0.32f, 0.10f };
        case MixRole::Bass:       return { 0.92f, 0.70f, 0.25f, 0.12f, 0.08f, 0.03f };
        case MixRole::Snare:      return { 0.10f, 0.25f, 0.58f, 0.58f, 0.68f, 0.30f };
        case MixRole::Hat:        return { 0.02f, 0.06f, 0.18f, 0.44f, 0.84f, 0.92f };
        case MixRole::Perc:       return { 0.06f, 0.18f, 0.40f, 0.55f, 0.62f, 0.42f };
        case MixRole::Loop:       return { 0.22f, 0.45f, 0.62f, 0.64f, 0.52f, 0.42f };
        case MixRole::Instrument: return { 0.15f, 0.38f, 0.60f, 0.68f, 0.58f, 0.48f };
        case MixRole::Master:     return { 0.60f, 0.58f, 0.55f, 0.54f, 0.54f, 0.50f };
        case MixRole::Other:
        default:                  return { 0.20f, 0.35f, 0.45f, 0.48f, 0.45f, 0.35f };
    }
}

float profileScore(const std::array<float, 6>& current, const std::array<float, 6>& target)
{
    float err = 0.0f;
    for (size_t i = 0; i < current.size(); ++i)
        err += std::abs(current[i] - target[i]);
    return juce::jlimit(0.0f, 100.0f, 100.0f - (err / (float)current.size()) * 125.0f);
}
}

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
    pushTrackControlsToHost();
    startTimerHz(30);
}

Mixer::~Mixer() = default;

void Mixer::syncFromChannelRack(const std::vector<juce::String>& channelNames,
                                const juce::StringArray& stripNumbers)
{
    const int insertCount = (int)channelNames.size();
    const int targetSize = insertCount + 1; // inserts + master
    if (targetSize <= 0)
        return;

    Track master;
    if (!tracks_.empty())
        master = std::move(tracks_.back());
    master.name = "Master";

    // Preserve insert tracks and their FX chains, but always keep the real
    // master as the final track.
    if (!tracks_.empty())
        tracks_.pop_back();

    if ((int)tracks_.size() > insertCount)
        tracks_.resize(insertCount);
    else if ((int)tracks_.size() < insertCount)
    {
        const int toAdd = insertCount - (int)tracks_.size();
        for (int i = 0; i < toAdd; ++i)
        {
            Track t;
            t.name = "Track " + juce::String(tracks_.size() + 1);
            tracks_.push_back(std::move(t));
        }
    }

    // Rename insert tracks to match channel rack
    for (int i = 0; i < insertCount && i < (int)tracks_.size(); ++i)
    {
        if (!channelNames[i].isEmpty())
            tracks_[i].name = channelNames[i];
        tracks_[i].stripNumber = (i < stripNumbers.size() && stripNumbers[i].isNotEmpty())
            ? stripNumbers[i]
            : juce::String(i + 1);
    }

    tracks_.push_back(std::move(master));

    pluginHost_.setMasterTrackIdx((int)tracks_.size() - 1);
    pushTrackControlsToHost();

    // Re-resolve sidechain track indices after the track list changed.
    if (sidechainOn_)
    {
        auto find = [this](std::initializer_list<const char*> needles) -> int {
            for (const char* needle : needles)
                for (int i = 0; i < (int)tracks_.size(); ++i)
                    if (tracks_[(size_t)i].name.toLowerCase().contains(needle)) return i;
            return -1;
        };
        const int k = find({ "kick" });
        const int s = find({ "808", "sub bass", "subbass", "bass", "sub" });
        if (k >= 0 && s >= 0 && k != s)
            pluginHost_.setSidechain(true, k, s, 0.7f, 4.0f, 190.0f);
        else
        { sidechainOn_ = false; pluginHost_.setSidechain(false, -1, -1); }
    }

    // Clamp selection
    if (selectedStrip_ >= (int)tracks_.size())
        selectedStrip_ = (int)tracks_.size() - 1;

    repaint();
    if (onTracksChanged) onTracksChanged();
}

void Mixer::paint(juce::Graphics& g)
{
    // Mixer background
    if (Theme::aeroMode)
        Theme::drawAeroPanel(g, getLocalBounds().toFloat());
    else
        g.fillAll(Theme::bg3);

    // ── Header ──────────────────────────────────────────────
    auto headerRect = juce::Rectangle<int>(0, 0, getWidth(), HEADER_HEIGHT);
    if (Theme::aeroMode)
        Theme::drawAeroGloss(g, headerRect.toFloat(), 0.6f);
    else
    {
        g.setColour(Theme::bg1);
        g.fillRect(headerRect);
    }
    g.setColour(Theme::bg7);
    g.drawHorizontalLine(HEADER_HEIGHT - 1, 0.0f, (float)getWidth());
    
    g.setColour(Theme::text4);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
    g.drawText("MIXER", 12, 0, 100, HEADER_HEIGHT, juce::Justification::centredLeft);

    btnAutoMixRect_ = juce::Rectangle<float>(82.0f, 5.0f, 66.0f, 16.0f);
    juce::ColourGradient autoGrad(hoveredHeaderBtn_ == 3 ? Theme::orange2.withAlpha(0.95f) : juce::Colour(0xff2a2a2e),
                                  0.0f, btnAutoMixRect_.getY(),
                                  hoveredHeaderBtn_ == 3 ? Theme::orange4.withAlpha(0.95f) : juce::Colour(0xff18181b),
                                  0.0f, btnAutoMixRect_.getBottom(), false);
    g.setGradientFill(autoGrad);
    g.fillRoundedRectangle(btnAutoMixRect_, 3.0f);
    g.setColour(hoveredHeaderBtn_ == 3 ? Theme::orange1 : Theme::bg8);
    g.drawRoundedRectangle(btnAutoMixRect_, 3.0f, 1.0f);
    g.setColour(hoveredHeaderBtn_ == 3 ? juce::Colours::black : Theme::text4);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
    g.drawText("AUTOMIX", btnAutoMixRect_.toNearestInt(), juce::Justification::centred);

    btnAutoFreqRect_ = juce::Rectangle<float>(btnAutoMixRect_.getRight() + 6.0f, 5.0f, 78.0f, 16.0f);
    const bool freqGood = profileScore(getCurrentFrequencyProfile(), getTargetFrequencyProfile(autoFreqGenre_)) >= 82.0f;
    juce::ColourGradient freqGrad((frequencyMixReady_ || hoveredHeaderBtn_ == 5) ? Theme::orange2.withAlpha(0.95f) : juce::Colour(0xff2a2a2e),
                                  0.0f, btnAutoFreqRect_.getY(),
                                  (frequencyMixReady_ || hoveredHeaderBtn_ == 5) ? Theme::orange4.withAlpha(0.95f) : juce::Colour(0xff18181b),
                                  0.0f, btnAutoFreqRect_.getBottom(), false);
    g.setGradientFill(freqGrad);
    g.fillRoundedRectangle(btnAutoFreqRect_, 3.0f);
    g.setColour((frequencyMixReady_ || hoveredHeaderBtn_ == 5) ? Theme::orange1 : Theme::bg8);
    g.drawRoundedRectangle(btnAutoFreqRect_, 3.0f, 1.0f);
    g.setColour((frequencyMixReady_ || hoveredHeaderBtn_ == 5) ? juce::Colours::black : Theme::text4);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
    g.drawText(freqGood ? "FREQ READY" : "AUTO FREQ", btnAutoFreqRect_.toNearestInt(), juce::Justification::centred);

    // SIDECHAIN toggle (right after AUTOMIX)
    btnSidechainRect_ = juce::Rectangle<float>(btnAutoFreqRect_.getRight() + 6.0f, 5.0f, 80.0f, 16.0f);
    const bool scActive = sidechainOn_;
    juce::ColourGradient scGrad(
        (scActive || hoveredHeaderBtn_ == 4) ? Theme::orange2.withAlpha(0.95f) : juce::Colour(0xff2a2a2e),
        0.0f, btnSidechainRect_.getY(),
        (scActive || hoveredHeaderBtn_ == 4) ? Theme::orange4.withAlpha(0.95f) : juce::Colour(0xff18181b),
        0.0f, btnSidechainRect_.getBottom(), false);
    g.setGradientFill(scGrad);
    g.fillRoundedRectangle(btnSidechainRect_, 3.0f);
    g.setColour((scActive || hoveredHeaderBtn_ == 4) ? Theme::orange1 : Theme::bg8);
    g.drawRoundedRectangle(btnSidechainRect_, 3.0f, 1.0f);
    g.setColour((scActive || hoveredHeaderBtn_ == 4) ? juce::Colours::black : Theme::text4);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
    g.drawText(scActive ? "SIDECHAIN \xE2\x97\x8F" : "SIDECHAIN",
               btnSidechainRect_.toNearestInt(), juce::Justification::centred);

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
        juce::String num = isMaster ? "M"
                                    : (tr.stripNumber.isNotEmpty() ? tr.stripNumber : juce::String((int)i + 1));
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
        const float level = juce::jlimit(0.0f, 1.0f, pluginHost_.getTrackLevel((int)i));
        for (int ch = 0; ch < 2; ++ch)
        {
            auto vuRect = juce::Rectangle<float>(
                (float)faderRect.getX() - 16 + ch * 6, (float)faderRect.getY(),
                5.0f, (float)faderRect.getHeight()
            );
            // Background (sunken)
            g.setColour(Theme::bg1);
            g.fillRoundedRectangle(vuRect, 2.0f);

            const float meterH = vuRect.getHeight() * level;
            auto meterFill = juce::Rectangle<float>(vuRect.getX(),
                                                    vuRect.getBottom() - meterH,
                                                    vuRect.getWidth(),
                                                    meterH);
            g.setColour(isMaster ? Theme::orange2 : Theme::green2);
            g.fillRoundedRectangle(meterFill, 2.0f);
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

        g.setColour(isMaster ? Theme::orange2 : Theme::text5);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.0f).withStyle("Bold"));
        g.drawText(formatDb(gainToDb(tr.volume)),
                   stripRect.getX(), sy, stripRect.getWidth(), 10, juce::Justification::centred);
        sy += 11;

        const float peakDb = gainToDb(juce::jlimit(0.000001f, 1.0f, level));
        g.setColour(level > 0.92f ? juce::Colour(0xffef4444) : Theme::text6);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(7.5f));
        g.drawText(formatDb(peakDb),
                   stripRect.getX(), sy, stripRect.getWidth(), 9, juce::Justification::centred);
        sy += 9;
        
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
    
    // ── Detail Panel (right side) ─────────────────────────────
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
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
        g.drawText(tracks_[selectedStrip_].name, detailRect.getX() + 10, detailRect.getY() + 8,
                   detailRect.getWidth() - 20, 16, juce::Justification::centredLeft);
    }
    g.setColour(Theme::bg7);
    g.drawHorizontalLine(detailRect.getY() + 30, (float)detailRect.getX(), (float)detailRect.getRight());
    
    // FX Chain label
    g.setColour(Theme::text5);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
    g.drawText("FX Chain", detailRect.getX() + 10, detailRect.getY() + 36,
               detailRect.getWidth() - 20, 16, juce::Justification::centredLeft);
    
    // 8 plugin slots — show actual plugin name when assigned
    const Track* selTrack = (selectedStrip_ >= 0 && selectedStrip_ < (int)tracks_.size())
                                ? &tracks_[selectedStrip_] : nullptr;
    for (int s = 0; s < FX_SLOT_COUNT; ++s)
    {
        auto slotRect = getFxSlotRect(s);
        g.setColour(juce::Colour(0xff1a1a1c));
        g.drawHorizontalLine(slotRect.getBottom() - 1, (float)slotRect.getX(), (float)slotRect.getRight());

        bool filled = (selTrack && s < (int)selTrack->fxSlots.size());
        const bool enabled = filled && selTrack->fxSlots[(size_t)s].enabled;
        juce::String label;
        if (filled)
            label = "> " + selTrack->fxSlots[(size_t)s].displayName
                  + (selTrack->fxSlots[(size_t)s].isWasm ? "  [W]" : "")
                  + (enabled ? "" : "  [OFF]");
        else
            label = "> Slot " + juce::String(s + 1) + " (empty)";

        g.setColour(filled ? (enabled ? Theme::text3 : Theme::text5) : Theme::text5);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle((filled && enabled) ? "Bold" : "Regular"));
        g.drawText(label, slotRect.reduced(10, 0), juce::Justification::centredLeft);

        // Status dot — lit orange if plugin loaded
        auto dot = getFxSlotPowerRect(s).toFloat();
        g.setColour(enabled ? Theme::orange2 : Theme::bg7);
        g.fillEllipse(dot);
        g.setColour((filled && !enabled) ? Theme::text6 : Theme::bg8);
        g.drawEllipse(dot, 1.0f);
    }

    drawFrequencyMixPanel(g, juce::Rectangle<int>(detailRect.getX() + 10,
                                                  detailRect.getY() + 54 + FX_SLOT_COUNT * FX_SLOT_H + 12,
                                                  detailRect.getWidth() - 20,
                                                  132));
    
    // Bottom: Out 1 / Out 2
    g.setColour(Theme::bg7);
    g.drawHorizontalLine(detailRect.getBottom() - 28, (float)detailRect.getX(), (float)detailRect.getRight());
    g.setColour(Theme::text5);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
    juce::String outText = "Out 1 / Out 2";
    if (selectedStrip_ >= 0 && selectedStrip_ < (int) tracks_.size())
    {
        int rt = tracks_[selectedStrip_].routeTo;
        if (rt < 0) rt = (int) tracks_.size() - 1; // master
        if (rt >= 0 && rt < (int) tracks_.size() && rt != selectedStrip_)
            outText = "Out → " + tracks_[rt].name;
    }
    g.drawText(outText, detailRect.getX() + 10, detailRect.getBottom() - 24,
               detailRect.getWidth() - 20, 16, juce::Justification::centredLeft);
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
    if (btnAutoMixRect_.contains(p))
    {
        showAutoMixMenu();
        return;
    }
    if (btnAutoFreqRect_.contains(p))
    {
        showAutoFrequencyMenu();
        return;
    }
    if (btnSidechainRect_.contains(p))
    {
        toggleAutoSidechain();
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
            if (filled && getFxSlotPowerRect(s).expanded(4).contains(e.x, e.y))
            {
                auto& slot = track.fxSlots[(size_t)s];
                slot.enabled = !slot.enabled;
                pluginHost_.setFxSlotBypassed(slot.pluginSlotId, !slot.enabled);
                repaint();
                if (onTracksChanged) onTracksChanged();
                return;
            }

            if (e.mods.isRightButtonDown())
            {
                if (filled)
                {
                    juce::PopupMenu m;
                    m.addItem(1, "Create automation clip");
                    m.addSeparator();
                    m.addItem(2, "Remove effect");
                    const int trackIdx = selectedStrip_;
                    const int slotIdx = s;
                    m.showMenuAsync(juce::PopupMenu::Options{}
                        .withTargetComponent(this)
                        .withTargetScreenArea(getFxSlotRect(s)),
                        [this, trackIdx, slotIdx](int choice)
                        {
                            if (trackIdx < 0 || trackIdx >= (int)tracks_.size())
                                return;
                            auto& track = tracks_[(size_t)trackIdx];
                            if (slotIdx < 0 || slotIdx >= (int)track.fxSlots.size())
                                return;

                            if (choice == 1)
                            {
                                const auto& slot = track.fxSlots[(size_t)slotIdx];
                                if (onCreateFxAutomation)
                                    onCreateFxAutomation(trackIdx, slot.pluginSlotId, slot.displayName);
                            }
                            else if (choice == 2)
                            {
                                removeFxFromTrack(trackIdx, slotIdx);
                                repaint();
                                if (onTracksChanged) onTracksChanged();
                            }
                        });
                }
            }
            else if (filled)
            {
                int pid = track.fxSlots[s].pluginSlotId;
                if (pid >= 0)
                    pluginHost_.showEditor(pid, true);
                else if (pid < 0)
                    pluginHost_.showNativeEffectEditor(pid);
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
        pushTrackControlsToHost();
        repaint();
        if (onTracksChanged) onTracksChanged();
        return;
    }
    
    if (getSoloRect(trackIdx).contains(e.x, e.y))
    {
        track.solo = !track.solo;
        pushTrackControlsToHost();
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
        pushTrackControlsToHost();
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
        pushTrackControlsToHost();
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
    else if (btnAutoMixRect_.contains(p)) newHover = 3;
    else if (btnSidechainRect_.contains(p)) newHover = 4;
    else if (btnAutoFreqRect_.contains(p)) newHover = 5;

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

void Mixer::timerCallback()
{
    // Only repaint the meters when actually on screen. When the mixer is hidden
    // (e.g. playing in the Playlist view) this 30 Hz repaint is pure waste that
    // competes with the message-thread sequencer clock.
    if (isShowing())
        repaint();
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
    int slotY = detail.getY() + 54 + slotIdx * FX_SLOT_H;
    return juce::Rectangle<int>(detail.getX(), slotY, detail.getWidth(), FX_SLOT_H);
}

juce::Rectangle<int> Mixer::getFxSlotPowerRect(int slotIdx) const
{
    auto slotRect = getFxSlotRect(slotIdx);
    return juce::Rectangle<int>(slotRect.getRight() - 18, slotRect.getCentreY() - 5, 10, 10);
}

void Mixer::setTrackVolume(int i, float v)
{
    if (i < 0 || i >= (int)tracks_.size()) return;
    tracks_[i].volume = juce::jlimit(0.0f, 1.0f, v);
    pushTrackControlsToHost();
    repaint();
    if (onTracksChanged) onTracksChanged();
}

void Mixer::setFxSlotEnabledById(int pluginSlotId, bool enabled)
{
    bool changed = false;
    for (auto& track : tracks_)
    {
        for (auto& slot : track.fxSlots)
        {
            if (slot.pluginSlotId != pluginSlotId || slot.enabled == enabled)
                continue;

            slot.enabled = enabled;
            pluginHost_.setFxSlotBypassed(pluginSlotId, !enabled);
            changed = true;
        }
    }

    if (changed)
    {
        repaint();
        if (onTracksChanged) onTracksChanged();
    }
}

void Mixer::pushTrackControlsToHost()
{
    std::vector<PluginHost::TrackControl> controls;
    controls.reserve(tracks_.size());
    for (const auto& t : tracks_)
    {
        PluginHost::TrackControl c;
        c.volume = t.volume;
        c.pan = t.pan;
        c.muted = t.muted;
        c.solo = t.solo;
        controls.push_back(c);
    }
    pluginHost_.setTrackControls(std::move(controls));
}

// Push the ordered list of plugin slot ids for trackIdx down to PluginHost
// so the audio engine routes that track's voices through them.
static void pushTrackChain(PluginHost& host, int trackIdx, const std::vector<Mixer::FxSlot>& slots)
{
    std::vector<int> ids;
    for (auto& s : slots)
    {
        if (s.pluginSlotId != 0) ids.push_back(s.pluginSlotId);
        host.setFxSlotBypassed(s.pluginSlotId, !s.enabled);
    }
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
    fs.enabled = true;
    slots.push_back(fs);
    pluginHost_.setFxSlotBypassed(pluginSlotId, false);
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
    if (pid != 0) pluginHost_.unloadPlugin(pid);
    slots.erase(slots.begin() + slotIdx);
    pushTrackChain(pluginHost_, trackIdx, slots);
}

void Mixer::openPluginPickerForTrack(int trackIdx)
{
    if (trackIdx < 0 || trackIdx >= (int)tracks_.size()) return;

    // ── Build the popup menu ─────────────────────────────────────
    juce::PopupMenu menu;
    juce::PopupMenu native, effects, instruments;
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

    native.addItem(9101, "Stratum Reverb  [Native]");
    native.addItem(9102, "Stratum Delay  [Native]");
    native.addItem(9103, "Stratum Parametric EQ  [Native]");
    native.addItem(9104, "Stratum Soft Clipper  [Native]");

    menu.addSubMenu("Stratum Native", native);
    if (instruments.getNumItems() > 0) menu.addSubMenu("Instruments", instruments);
    if (effects.getNumItems() > 0)     menu.addSubMenu("Effects",     effects);
    if (types.isEmpty())
        menu.addItem(juce::PopupMenu::Item("(no cached plugins - use re-scan once)").setEnabled(false));
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

            if (chosen >= 9101 && chosen <= 9104)
            {
                juce::String type = "reverb";
                juce::String name = "Stratum Reverb";
                if (chosen == 9102) { type = "delay"; name = "Stratum Delay"; }
                if (chosen == 9103) { type = "parametric-eq"; name = "Stratum Parametric EQ"; }
                if (chosen == 9104) { type = "soft-clipper"; name = "Stratum Soft Clipper"; }
                const int effectId = pluginHost_.createNativeEffect(type);
                addFxToTrack(trackR, effectId, name, true);
                return;
            }

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

void Mixer::toggleAutoSidechain()
{
    // Find the kick track (source) and the 808/bass track (target) by name.
    auto findTrack = [this](std::initializer_list<const char*> needles) -> int
    {
        for (const char* needle : needles)
            for (int i = 0; i < (int)tracks_.size(); ++i)
                if (tracks_[(size_t)i].name.toLowerCase().contains(needle))
                    return i;
        return -1;
    };

    const int kickIdx = findTrack({ "kick" });
    const int subIdx  = findTrack({ "808", "sub bass", "subbass", "bass", "sub" });

    if (kickIdx < 0 || subIdx < 0 || kickIdx == subIdx)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Auto Sidechain",
            "Couldn't find both a \"kick\" track and an \"808\"/\"bass\" track to link.\n"
            "Rename your channels so one contains \"kick\" and another contains \"808\" or \"bass\".");
        sidechainOn_ = false;
        pluginHost_.setSidechain(false, -1, -1);
        repaint();
        return;
    }

    sidechainOn_ = !sidechainOn_;
    // Punchy defaults for trap/RnB 808 ducking.
    pluginHost_.setSidechain(sidechainOn_, kickIdx, subIdx,
                             0.7f /*depth*/, 4.0f /*attack ms*/, 190.0f /*release ms*/);

    if (sidechainOn_)
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Auto Sidechain ON",
            "\"" + tracks_[(size_t)subIdx].name + "\" now ducks under \""
            + tracks_[(size_t)kickIdx].name + "\".");
    repaint();
}

// ─── Project I/O ─────────────────────────────────────────────────────
void Mixer::showAutoMixMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader("AutoMix target balance");
    m.addItem(1, "Boom Bap");
    m.addItem(2, "R&B");
    m.addItem(3, "Trap / Hip Hop");
    m.addItem(4, "Drake / Gunna");
    m.addItem(5, "DaBaby / Club Rap");
    m.addItem(6, "Lo-Fi");
    m.addItem(7, "Drill");
    m.showMenuAsync(juce::PopupMenu::Options{}
        .withTargetComponent(this)
        .withTargetScreenArea(btnAutoMixRect_.toNearestInt()),
        [this](int choice)
        {
            switch (choice)
            {
                case 1: applyAutoMixPreset("boom bap"); break;
                case 2: applyAutoMixPreset("rnb"); break;
                case 3: applyAutoMixPreset("trap"); break;
                case 4: applyAutoMixPreset("drake gunna"); break;
                case 5: applyAutoMixPreset("dababy"); break;
                case 6: applyAutoMixPreset("lofi"); break;
                case 7: applyAutoMixPreset("drill"); break;
                default: break;
            }
        });
}

void Mixer::showAutoFrequencyMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader("Auto frequency mix");
    m.addItem(1, "Boom Bap", true, autoFreqGenre_.contains("boom"));
    m.addItem(2, "Trap / Hip Hop", true, autoFreqGenre_.contains("trap"));
    m.addItem(3, "R&B", true, autoFreqGenre_.contains("rnb"));
    m.addItem(4, "Drake / Gunna", true, autoFreqGenre_.contains("drake"));
    m.addItem(5, "Lo-Fi", true, autoFreqGenre_.contains("lofi"));
    m.addSeparator();
    m.addItem(9, "Re-run current correction");
    m.showMenuAsync(juce::PopupMenu::Options{}
        .withTargetComponent(this)
        .withTargetScreenArea(btnAutoFreqRect_.toNearestInt()),
        [this](int choice)
        {
            switch (choice)
            {
                case 1: applyAutoFrequencyMix("boom bap"); break;
                case 2: applyAutoFrequencyMix("trap"); break;
                case 3: applyAutoFrequencyMix("rnb"); break;
                case 4: applyAutoFrequencyMix("drake gunna"); break;
                case 5: applyAutoFrequencyMix("lofi"); break;
                case 9: applyAutoFrequencyMix(autoFreqGenre_); break;
                default: break;
            }
        });
}

void Mixer::applyAutoMixPreset(const juce::String& genre)
{
    autoFreqGenre_ = genre;
    auto targetDbFor = [genre](juce::String name)
    {
        name = name.toLowerCase();
        const bool boom = genre.contains("boom");
        const bool rnb = genre.contains("rnb");
        const bool trap = genre.contains("trap") || genre.contains("drake") || genre.contains("gunna") || genre.contains("dababy") || genre.contains("drill");
        const bool lofi = genre.contains("lofi");

        if (name.contains("master")) return -3.0f;
        if (name.contains("808") || name.contains("bass")) return trap ? -7.0f : (rnb ? -9.0f : -8.0f);
        if (name.contains("kick")) return boom ? -6.0f : (trap ? -5.5f : -7.0f);
        if (name.contains("snare") || name.contains("clap")) return boom ? -8.0f : (rnb ? -10.0f : -8.5f);
        if (name.contains("hihat") || name.contains("hi hat") || name.contains("hat")) return boom ? -15.0f : (trap ? -12.0f : -16.0f);
        if (name.contains("openhat") || name.contains("open hat")) return boom ? -17.0f : -14.0f;
        if (name.contains("ride") || name.contains("perc") || name.contains("rim")) return lofi ? -16.0f : -14.0f;
        if (name.contains("loop") || name.contains("sample") || name.contains("melody") || name.contains("piano") || name.contains("guitar")) return rnb ? -11.0f : -12.5f;
        return -12.0f;
    };

    for (auto& track : tracks_)
    {
        const float db = targetDbFor(track.name);
        track.volume = juce::jlimit(0.0f, 1.0f, std::pow(10.0f, db / 20.0f));
    }

    pushTrackControlsToHost();
    repaint();
    if (onTracksChanged) onTracksChanged();
}

int Mixer::ensureAutoEqForTrack(int trackIdx)
{
    if (trackIdx < 0 || trackIdx >= (int)tracks_.size())
        return 0;

    auto& slots = tracks_[(size_t)trackIdx].fxSlots;
    for (auto& slot : slots)
    {
        if (slot.pluginSlotId < 0
            && pluginHost_.getNativeEffectType(slot.pluginSlotId) == "parametric-eq"
            && slot.displayName.containsIgnoreCase("Auto Frequency EQ"))
            return slot.pluginSlotId;
    }

    for (auto& slot : slots)
    {
        if (slot.pluginSlotId < 0
            && pluginHost_.getNativeEffectType(slot.pluginSlotId) == "parametric-eq")
        {
            slot.displayName = "Auto Frequency EQ";
            slot.enabled = true;
            pluginHost_.setFxSlotBypassed(slot.pluginSlotId, false);
            pushTrackChain(pluginHost_, trackIdx, slots);
            return slot.pluginSlotId;
        }
    }

    if ((int)slots.size() >= FX_SLOT_COUNT)
        return 0;

    const int effectId = pluginHost_.createNativeEffect("parametric-eq");
    if (effectId >= 0)
        return 0;

    addFxToTrack(trackIdx, effectId, "Auto Frequency EQ", true);
    return effectId;
}

void Mixer::applyAutoFrequencyMix(const juce::String& genre)
{
    autoFreqGenre_ = genre.isNotEmpty() ? genre : "boom bap";

    auto setEq = [this](int effectId,
                        float hp, float lowFreq, float lowGain,
                        float bodyFreq, float bodyGain,
                        float midFreq, float midGain,
                        float presFreq, float presGain,
                        float airFreq, float airGain,
                        float lp)
    {
        pluginHost_.setNativeEffectParam(effectId, "eq0Freq", hp);
        pluginHost_.setNativeEffectParam(effectId, "eq0Q", 0.72f);
        pluginHost_.setNativeEffectParam(effectId, "eq1Freq", lowFreq);
        pluginHost_.setNativeEffectParam(effectId, "eq1Gain", lowGain);
        pluginHost_.setNativeEffectParam(effectId, "eq2Freq", bodyFreq);
        pluginHost_.setNativeEffectParam(effectId, "eq2Gain", bodyGain);
        pluginHost_.setNativeEffectParam(effectId, "eq2Q", 1.05f);
        pluginHost_.setNativeEffectParam(effectId, "eq3Freq", midFreq);
        pluginHost_.setNativeEffectParam(effectId, "eq3Gain", midGain);
        pluginHost_.setNativeEffectParam(effectId, "eq3Q", 1.15f);
        pluginHost_.setNativeEffectParam(effectId, "eq4Freq", presFreq);
        pluginHost_.setNativeEffectParam(effectId, "eq4Gain", presGain);
        pluginHost_.setNativeEffectParam(effectId, "eq4Q", 1.25f);
        pluginHost_.setNativeEffectParam(effectId, "eq5Freq", airFreq);
        pluginHost_.setNativeEffectParam(effectId, "eq5Gain", airGain);
        pluginHost_.setNativeEffectParam(effectId, "eq6Freq", lp);
        pluginHost_.setNativeEffectParam(effectId, "eq6Q", 0.72f);
    };

    const bool boom = autoFreqGenre_.contains("boom");
    const bool trap = autoFreqGenre_.contains("trap") || autoFreqGenre_.contains("drake") || autoFreqGenre_.contains("gunna");
    const bool rnb = autoFreqGenre_.contains("rnb");
    const bool lofi = autoFreqGenre_.contains("lofi");

    int corrected = 0;
    for (int i = 0; i < (int)tracks_.size(); ++i)
    {
        if (i == (int)tracks_.size() - 1)
            continue;

        const auto role = classifyTrackName(tracks_[(size_t)i].name);
        if (role == MixRole::Other)
            continue;

        const int eq = ensureAutoEqForTrack(i);
        if (eq == 0)
            continue;

        switch (role)
        {
            case MixRole::Kick:
                setEq(eq, 28.0f, 72.0f, boom ? 1.8f : 2.4f, 240.0f, -2.8f, 820.0f, -1.2f,
                      3100.0f, trap ? 2.8f : 1.8f, 8500.0f, -2.5f, 13000.0f);
                break;
            case MixRole::Bass:
                setEq(eq, 24.0f, trap ? 58.0f : 72.0f, trap ? 3.2f : 2.2f, 210.0f, -2.5f,
                      760.0f, -2.8f, 2600.0f, -3.5f, 8200.0f, -7.0f, 7600.0f);
                break;
            case MixRole::Snare:
                setEq(eq, 115.0f, 185.0f, -0.8f, 360.0f, boom ? -1.2f : -2.2f, 1050.0f, 1.2f,
                      3300.0f, boom ? 2.4f : 3.3f, 9200.0f, rnb ? 1.8f : 0.8f, 16500.0f);
                break;
            case MixRole::Hat:
                setEq(eq, boom ? 340.0f : 420.0f, 520.0f, -5.0f, 1400.0f, -2.2f, 3800.0f, 1.4f,
                      7900.0f, trap ? 3.4f : 2.2f, 11200.0f, rnb ? 2.4f : 1.2f, 18500.0f);
                break;
            case MixRole::Perc:
                setEq(eq, 150.0f, 260.0f, -1.8f, 620.0f, -0.8f, 1800.0f, 1.0f,
                      5200.0f, 2.0f, 9600.0f, 0.8f, 17000.0f);
                break;
            case MixRole::Loop:
            case MixRole::Instrument:
                setEq(eq, trap ? 120.0f : (boom ? 70.0f : 90.0f), 150.0f, -1.2f, 320.0f, -2.4f,
                      980.0f, lofi ? -0.8f : -1.4f, 2800.0f, rnb ? 1.7f : 0.8f,
                      9600.0f, lofi ? -2.5f : (rnb ? 1.2f : -0.7f), lofi ? 12500.0f : 17000.0f);
                break;
            default:
                continue;
        }

        ++corrected;
    }

    frequencyMixReady_ = corrected > 0;
    repaint();
    if (onTracksChanged) onTracksChanged();
}

std::array<float, 6> Mixer::getTargetFrequencyProfile(const juce::String& genre) const
{
    const auto g = genre.toLowerCase();
    if (g.contains("trap") || g.contains("drake") || g.contains("gunna"))
        return { 0.88f, 0.64f, 0.42f, 0.48f, 0.60f, 0.58f };
    if (g.contains("rnb"))
        return { 0.70f, 0.56f, 0.50f, 0.55f, 0.61f, 0.62f };
    if (g.contains("lofi"))
        return { 0.66f, 0.62f, 0.58f, 0.48f, 0.42f, 0.32f };
    return { 0.76f, 0.68f, 0.56f, 0.47f, 0.52f, 0.38f };
}

std::array<float, 6> Mixer::getCurrentFrequencyProfile() const
{
    std::array<float, 6> sum {};
    float total = 0.0001f;
    for (int i = 0; i < (int)tracks_.size(); ++i)
    {
        if (i == (int)tracks_.size() - 1 || tracks_[(size_t)i].muted)
            continue;

        const auto roleProfile = roleFrequencyProfile(classifyTrackName(tracks_[(size_t)i].name));
        const float level = juce::jlimit(0.0f, 1.2f, tracks_[(size_t)i].volume);
        for (size_t b = 0; b < sum.size(); ++b)
            sum[b] += roleProfile[b] * level;
        total += level;
    }

    for (auto& v : sum)
        v = juce::jlimit(0.0f, 1.0f, v / total);
    return sum;
}

void Mixer::drawFrequencyMixPanel(juce::Graphics& g, juce::Rectangle<int> panel)
{
    if (panel.getBottom() > getHeight() - 34)
        return;

    const auto current = getCurrentFrequencyProfile();
    const auto target = getTargetFrequencyProfile(autoFreqGenre_);
    const float score = profileScore(current, target);

    g.setColour(juce::Colour(0xff151518));
    g.fillRoundedRectangle(panel.toFloat(), 6.0f);
    g.setColour(score >= 82.0f ? Theme::orange2.withAlpha(0.85f) : Theme::bg8);
    g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 6.0f, 1.0f);

    auto title = panel.reduced(10, 8).removeFromTop(18);
    g.setColour(Theme::text4);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f).withStyle("Bold"));
    g.drawText("FREQUENCY MIX  " + autoFreqGenre_.toUpperCase(), title, juce::Justification::centredLeft, true);
    g.setColour(score >= 82.0f ? Theme::orange2 : juce::Colour(0xfffacc15));
    g.drawText(juce::String((int)std::round(score)) + "%", title, juce::Justification::centredRight, true);

    static const char* labels[] = { "SUB", "LOW", "BODY", "MID", "PRES", "AIR" };
    auto graph = panel.reduced(10, 8).withTrimmedTop(24).withTrimmedBottom(20);
    const int gap = 6;
    const int colW = (graph.getWidth() - gap * 5) / 6;
    for (int i = 0; i < 6; ++i)
    {
        auto col = juce::Rectangle<int>(graph.getX() + i * (colW + gap), graph.getY(), colW, graph.getHeight());
        g.setColour(juce::Colour(0xff09090b));
        g.fillRoundedRectangle(col.toFloat(), 3.0f);

        const int targetY = col.getBottom() - (int)std::round(target[(size_t)i] * col.getHeight());
        g.setColour(Theme::text6.withAlpha(0.75f));
        g.drawHorizontalLine(targetY, (float)col.getX(), (float)col.getRight());

        const int barH = (int)std::round(current[(size_t)i] * col.getHeight());
        auto bar = col.withTrimmedTop(col.getHeight() - barH);
        g.setColour(std::abs(current[(size_t)i] - target[(size_t)i]) < 0.12f ? Theme::orange2 : juce::Colour(0xff3b82f6));
        g.fillRoundedRectangle(bar.toFloat().reduced(3.0f, 0.0f), 3.0f);
    }

    auto labelRow = panel.reduced(10, 0).removeFromBottom(18);
    g.setColour(Theme::text6);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(7.5f).withStyle("Bold"));
    for (int i = 0; i < 6; ++i)
    {
        auto col = juce::Rectangle<int>(labelRow.getX() + i * (colW + gap), labelRow.getY(), colW, labelRow.getHeight());
        g.drawText(labels[i], col, juce::Justification::centred, true);
    }
}

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
    obj->setProperty("autoFreqGenre", autoFreqGenre_);
    obj->setProperty("frequencyMixReady", frequencyMixReady_);
    return juce::var(obj);
}

void Mixer::fromJson(const juce::var& v)
{
    if (!v.isObject()) return;
    auto* arr = v.getProperty("tracks", juce::var()).getArray();
    if (!arr) return;

    autoFreqGenre_ = v.getProperty("autoFreqGenre", autoFreqGenre_).toString();
    frequencyMixReady_ = (bool)v.getProperty("frequencyMixReady", false);

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
    pluginHost_.setMasterTrackIdx((int)tracks_.size() - 1);
    pushTrackControlsToHost();
    if (onTracksChanged) onTracksChanged();
    repaint();
}
