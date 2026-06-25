#include "SamplePropsPanel.h"
#include "Theme.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

SamplePropsPanel::SamplePropsPanel(const juce::File& sampleFile,
                                   const juce::String& channelName,
                                   ChannelRack::Channel::SampleProps* props)
    : file_(sampleFile), name_(channelName), p_(props)
{
    setOpaque(true);
    setSize(540, 312);

    configureKnob(pitchSlider_,   pitchLabel_,   "PITCH");
    configureKnob(fadeInSlider_,  fadeInLabel_,  "FADE IN");
    configureKnob(fadeOutSlider_, fadeOutLabel_, "FADE OUT");

    pitchSlider_.setRange(-24.0, 24.0, 1.0);
    pitchSlider_.setTextValueSuffix(" st");
    fadeInSlider_.setRange(0.0, 500.0, 1.0);
    fadeInSlider_.setTextValueSuffix(" ms");
    fadeOutSlider_.setRange(0.0, 500.0, 1.0);
    fadeOutSlider_.setTextValueSuffix(" ms");

    if (p_)
    {
        pitchSlider_.setValue(p_->pitchSemis,  juce::dontSendNotification);
        fadeInSlider_.setValue(p_->fadeInMs,   juce::dontSendNotification);
        fadeOutSlider_.setValue(p_->fadeOutMs, juce::dontSendNotification);
    }

    pitchSlider_.onValueChange   = [this] { if (p_) p_->pitchSemis = (float)pitchSlider_.getValue();   fireChange(); };
    fadeInSlider_.onValueChange  = [this] { if (p_) p_->fadeInMs   = (float)fadeInSlider_.getValue();  fireChange(); };
    fadeOutSlider_.onValueChange = [this] { if (p_) p_->fadeOutMs  = (float)fadeOutSlider_.getValue(); fireChange(); };

    computePeaks();
}

void SamplePropsPanel::configureKnob(juce::Slider& s, juce::Label& l, const juce::String& text)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    s.setColour(juce::Slider::rotarySliderFillColourId, Theme::accent);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, Theme::zinc700);
    s.setColour(juce::Slider::thumbColourId, Theme::accentBright);
    s.setColour(juce::Slider::textBoxTextColourId, Theme::zinc200);
    s.setColour(juce::Slider::textBoxBackgroundColourId, Theme::zinc950);
    s.setColour(juce::Slider::textBoxOutlineColourId, Theme::zinc700);
    addAndMakeVisible(s);

    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, Theme::zinc400);
    l.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    addAndMakeVisible(l);
}

void SamplePropsPanel::computePeaks()
{
    peaks_.clear();
    if (!file_.existsAsFile()) return;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file_));
    if (!reader) return;

    const int N = 220;                          // number of waveform columns
    const auto len = (juce::int64)reader->lengthInSamples;
    if (len <= 0) return;
    const int block = (int)juce::jmax((juce::int64)1, len / N);
    juce::AudioBuffer<float> tmp((int)juce::jmax((unsigned)1, reader->numChannels), block);
    peaks_.reserve(N);

    for (int i = 0; i < N; ++i)
    {
        const juce::int64 start = (juce::int64)i * block;
        if (start >= len) break;
        const int toRead = (int)juce::jmin((juce::int64)block, len - start);
        reader->read(&tmp, 0, toRead, start, true, true);
        float peak = 0.0f;
        for (int ch = 0; ch < tmp.getNumChannels(); ++ch)
            peak = juce::jmax(peak, tmp.getMagnitude(ch, 0, toRead));
        peaks_.push_back(juce::jlimit(0.0f, 1.0f, peak));
    }
}

void SamplePropsPanel::fireChange()
{
    repaint();
    if (onChange) onChange();
}

void SamplePropsPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::zinc950);
    auto b = getLocalBounds().reduced(12);
    Theme::drawChassis(g, b.toFloat(), 10.0f);

    // ── Title ─────────────────────────────────────────────
    auto titleRow = b.removeFromTop(30).reduced(6, 0);
    g.setColour(Theme::accentBright);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
    g.drawText("SAMPLER", titleRow.removeFromLeft(78), juce::Justification::centredLeft);
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(15.0f).withStyle("Bold"));
    g.drawText(name_, titleRow, juce::Justification::centredLeft, true);

    // ── Waveform ──────────────────────────────────────────
    auto wave = waveRect_.toFloat();
    g.setColour(juce::Colour(0xff10242b));
    g.fillRoundedRectangle(wave, 5.0f);
    g.setColour(Theme::zinc700);
    g.drawRoundedRectangle(wave, 5.0f, 1.0f);

    const bool rev = p_ && p_->reverse;
    if (!peaks_.empty())
    {
        const float midY = wave.getCentreY();
        const float halfH = wave.getHeight() * 0.46f;
        const int   n = (int)peaks_.size();
        for (int i = 0; i < n; ++i)
        {
            const int si = rev ? (n - 1 - i) : i;
            const float x = wave.getX() + (wave.getWidth() * (i + 0.5f) / n);
            const float h = juce::jmax(1.0f, peaks_[(size_t)si] * halfH);
            // Fade shading: dim columns inside the fade regions.
            float a = 0.9f;
            if (p_)
            {
                const float frac = (float)i / juce::jmax(1, n - 1);
                if (p_->fadeInMs  > 0 && frac < p_->fadeInMs  / 600.0f) a *= 0.45f;
                if (p_->fadeOutMs > 0 && frac > 1.0f - p_->fadeOutMs / 600.0f) a *= 0.45f;
            }
            g.setColour(Theme::accent.withAlpha(a));
            g.drawLine(x, midY - h, x, midY + h, 1.4f);
        }
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.drawHorizontalLine((int)midY, wave.getX(), wave.getRight());
    }
    else
    {
        g.setColour(Theme::zinc600);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
        g.drawText("(no preview)", waveRect_, juce::Justification::centred);
    }
    if (rev)
    {
        g.setColour(Theme::accentBright.withAlpha(0.85f));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
        g.drawText("REVERSED", waveRect_.reduced(6, 4), juce::Justification::topRight);
    }

    // ── Toggle buttons ────────────────────────────────────
    auto drawToggle = [&g](juce::Rectangle<float> r, const juce::String& txt, bool on)
    {
        juce::ColourGradient grad(on ? Theme::accent.withAlpha(0.30f) : juce::Colour(0xff202024), r.getX(), r.getY(),
                                  on ? Theme::accent.withAlpha(0.16f) : juce::Colour(0xff111114), r.getX(), r.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(on ? Theme::accentBright : Theme::zinc700);
        g.drawRoundedRectangle(r, 5.0f, on ? 1.4f : 1.0f);
        g.setColour(on ? Theme::zinc100 : Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(txt, r, juce::Justification::centred);
    };
    drawToggle(revBtn_,     "REVERSE",   p_ && p_->reverse);
    drawToggle(normBtn_,    "NORMALIZE", p_ && p_->normalize);
    drawToggle(declickBtn_, "DECLICK",   p_ && p_->declick);
    drawToggle(pingBtn_,    "PING-PONG", p_ && p_->pingPong);

    // ── Play / audition ──────────────────────────────────
    g.setColour(Theme::accent.withAlpha(0.9f));
    g.fillRoundedRectangle(playBtn_, 6.0f);
    g.setColour(Theme::zinc950);
    juce::Path tri;
    auto pc = playBtn_.getCentre();
    tri.addTriangle(pc.x - 5.0f, pc.y - 7.0f, pc.x - 5.0f, pc.y + 7.0f, pc.x + 7.0f, pc.y);
    g.fillPath(tri);
}

void SamplePropsPanel::resized()
{
    auto area = getLocalBounds().reduced(12).reduced(6);
    area.removeFromTop(30);                      // title row

    waveRect_ = area.removeFromTop(96).reduced(2, 4);

    area.removeFromTop(8);
    auto knobRow = area.removeFromTop(96);
    const int kw = 84;
    pitchSlider_.setBounds(knobRow.removeFromLeft(kw));
    fadeInSlider_.setBounds(knobRow.removeFromLeft(kw));
    fadeOutSlider_.setBounds(knobRow.removeFromLeft(kw));
    pitchLabel_.setBounds(pitchSlider_.getBounds().removeFromTop(15));
    fadeInLabel_.setBounds(fadeInSlider_.getBounds().removeFromTop(15));
    fadeOutLabel_.setBounds(fadeOutSlider_.getBounds().removeFromTop(15));

    // Right side of the knob row: play button.
    playBtn_ = knobRow.removeFromRight(56).withSizeKeepingCentre(48, 48).toFloat();

    area.removeFromTop(6);
    auto toggles = area.removeFromTop(30);
    const int tw = (toggles.getWidth() - 18) / 4;
    revBtn_     = toggles.removeFromLeft(tw).toFloat();           toggles.removeFromLeft(6);
    normBtn_    = toggles.removeFromLeft(tw).toFloat();           toggles.removeFromLeft(6);
    declickBtn_ = toggles.removeFromLeft(tw).toFloat();           toggles.removeFromLeft(6);
    pingBtn_    = toggles.toFloat();
}

bool SamplePropsPanel::hitToggle(juce::Point<float> pos)
{
    if (!p_) return false;
    if (revBtn_.contains(pos))     { p_->reverse   = !p_->reverse;   return true; }
    if (normBtn_.contains(pos))    { p_->normalize = !p_->normalize; return true; }
    if (declickBtn_.contains(pos)) { p_->declick   = !p_->declick;   return true; }
    if (pingBtn_.contains(pos))    { p_->pingPong  = !p_->pingPong;  return true; }
    return false;
}

void SamplePropsPanel::mouseDown(const juce::MouseEvent& e)
{
    const auto pos = e.position;
    if (playBtn_.contains(pos))
    {
        if (onAudition) onAudition();
        return;
    }
    if (hitToggle(pos))
        fireChange();
}
