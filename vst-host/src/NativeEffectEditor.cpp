#include "NativeEffectEditor.h"
#include "Theme.h"
#include <cmath>

namespace
{
    juce::String formatFreq(float hz)
    {
        return hz >= 1000.0f ? juce::String(hz / 1000.0f, 1) + "k" : juce::String((int)std::round(hz));
    }

    struct EqPresetBand { float freq; float gain; float q; };
    using EqPresetCurve = std::array<EqPresetBand, 7>;

    void setEqBand(PluginHost& host, int effectId, int band, const EqPresetBand& value)
    {
        const auto idx = juce::String(band);
        host.setNativeEffectParam(effectId, "eq" + idx + "Freq", value.freq);
        host.setNativeEffectParam(effectId, "eq" + idx + "Gain", value.gain);
        host.setNativeEffectParam(effectId, "eq" + idx + "Q", value.q);
    }
}

NativeEffectEditor::NativeEffectEditor(PluginHost& host, int effectId)
    : host_(host), effectId_(effectId)
{
    type_ = host_.getNativeEffectType(effectId_);
    name_ = host_.getNativeEffectName(effectId_);
    setOpaque(true);
    setSize(type_ == "soft-clipper" ? 360 : 780, type_ == "soft-clipper" ? 250 : 455);

    if (type_ == "parametric-eq")
    {
        configureSlider(freqSlider_, freqLabel_, "FREQ");
        configureSlider(gainSlider_, gainLabel_, "GAIN");
        configureSlider(qSlider_, qLabel_, "Q");
        freqSlider_.setRange(20.0, 20000.0, 1.0);
        freqSlider_.setSkewFactorFromMidPoint(1000.0);
        gainSlider_.setRange(-18.0, 18.0, 0.1);
        qSlider_.setRange(0.15, 12.0, 0.01);
        freqSlider_.onValueChange = [this] { pushEqBand(); };
        gainSlider_.onValueChange = [this] { pushEqBand(); };
        qSlider_.onValueChange = [this] { pushEqBand(); };
    }
    else
    {
        configureSlider(thresholdSlider_, thresholdLabel_, "THRES");
        configureSlider(preSlider_, preLabel_, "PRE");
        configureSlider(postSlider_, postLabel_, "POST");
        thresholdSlider_.setRange(0.05, 0.98, 0.001);
        preSlider_.setRange(0.25, 8.0, 0.01);
        postSlider_.setRange(0.0, 2.0, 0.01);
        thresholdSlider_.onValueChange = [this] { pushClipper(); };
        preSlider_.onValueChange = [this] { pushClipper(); };
        postSlider_.onValueChange = [this] { pushClipper(); };
    }

    refreshFromHost();
}

void NativeEffectEditor::configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, Theme::accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, Theme::zinc700);
    slider.setColour(juce::Slider::thumbColourId, Theme::accentBright);
    slider.setColour(juce::Slider::textBoxTextColourId, Theme::zinc200);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, Theme::zinc950);
    slider.setColour(juce::Slider::textBoxOutlineColourId, Theme::zinc700);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, Theme::zinc400);
    label.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    addAndMakeVisible(label);
}

void NativeEffectEditor::refreshFromHost()
{
    if (type_ == "parametric-eq")
    {
        freqSlider_.setValue(host_.getNativeEffectParam(effectId_, "eq" + juce::String(selectedBand_) + "Freq"), juce::dontSendNotification);
        gainSlider_.setValue(host_.getNativeEffectParam(effectId_, "eq" + juce::String(selectedBand_) + "Gain"), juce::dontSendNotification);
        qSlider_.setValue(host_.getNativeEffectParam(effectId_, "eq" + juce::String(selectedBand_) + "Q"), juce::dontSendNotification);
    }
    else
    {
        thresholdSlider_.setValue(host_.getNativeEffectParam(effectId_, "clipThreshold"), juce::dontSendNotification);
        preSlider_.setValue(host_.getNativeEffectParam(effectId_, "clipPreGain"), juce::dontSendNotification);
        postSlider_.setValue(host_.getNativeEffectParam(effectId_, "clipPostGain"), juce::dontSendNotification);
    }
}

void NativeEffectEditor::pushEqBand()
{
    if (type_ != "parametric-eq") return;
    const auto idx = juce::String(selectedBand_);
    host_.setNativeEffectParam(effectId_, "eq" + idx + "Freq", (float)freqSlider_.getValue());
    host_.setNativeEffectParam(effectId_, "eq" + idx + "Gain", (float)gainSlider_.getValue());
    host_.setNativeEffectParam(effectId_, "eq" + idx + "Q", (float)qSlider_.getValue());
    repaint();
}

void NativeEffectEditor::pushClipper()
{
    if (type_ != "soft-clipper") return;
    host_.setNativeEffectParam(effectId_, "clipThreshold", (float)thresholdSlider_.getValue());
    host_.setNativeEffectParam(effectId_, "clipPreGain", (float)preSlider_.getValue());
    host_.setNativeEffectParam(effectId_, "clipPostGain", (float)postSlider_.getValue());
    repaint();
}

void NativeEffectEditor::selectBand(int band)
{
    selectedBand_ = juce::jlimit(0, 6, band);
    refreshFromHost();
    repaint();
}

void NativeEffectEditor::showEqPresetMenu()
{
    if (type_ != "parametric-eq")
        return;

    juce::PopupMenu menu;
    menu.addSectionHeader("EQ Presets");
    menu.addItem(1, "Clean Reset");
    menu.addItem(2, "Underwater");
    menu.addItem(3, "90s Old Sound");
    menu.addItem(4, "Vinyl Warmth");
    menu.addItem(5, "Phone / Radio");
    menu.addItem(6, "Bright Air");
    menu.addItem(7, "Drum Knock");

    juce::Component::SafePointer<NativeEffectEditor> safe(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea(localAreaToGlobal(presetButtonRect_))
            .withMinimumWidth(190),
        [safe](int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            static const char* ids[] = { "", "clean", "underwater", "90s", "vinyl", "phone", "air", "drum_knock" };
            safe->applyEqPreset(ids[juce::jlimit(0, 7, result)]);
        });
}

void NativeEffectEditor::applyEqPreset(const juce::String& presetId)
{
    if (type_ != "parametric-eq")
        return;

    EqPresetCurve curve {{
        { 28.0f, 0.0f, 0.72f },
        { 95.0f, 0.0f, 0.72f },
        { 280.0f, 0.0f, 1.0f },
        { 850.0f, 0.0f, 1.0f },
        { 2500.0f, 0.0f, 1.1f },
        { 9200.0f, 0.0f, 0.72f },
        { 19500.0f, 0.0f, 0.72f }
    }};

    if (presetId == "underwater")
    {
        curve = {{
            { 170.0f, 0.0f, 0.80f },
            { 120.0f, 2.8f, 0.70f },
            { 360.0f, -2.4f, 1.35f },
            { 760.0f, 1.4f, 1.10f },
            { 1800.0f, -5.5f, 1.25f },
            { 3200.0f, -8.5f, 0.72f },
            { 4200.0f, 0.0f, 0.82f }
        }};
    }
    else if (presetId == "90s")
    {
        curve = {{
            { 45.0f, 0.0f, 0.72f },
            { 115.0f, 1.8f, 0.72f },
            { 310.0f, -2.2f, 1.05f },
            { 1150.0f, 1.1f, 1.15f },
            { 3300.0f, -1.8f, 0.95f },
            { 7200.0f, -4.2f, 0.72f },
            { 11200.0f, 0.0f, 0.72f }
        }};
    }
    else if (presetId == "vinyl")
    {
        curve = {{
            { 38.0f, 0.0f, 0.72f },
            { 120.0f, 2.2f, 0.72f },
            { 420.0f, -1.2f, 1.10f },
            { 1400.0f, 0.6f, 1.00f },
            { 3600.0f, -1.0f, 0.90f },
            { 6800.0f, -3.0f, 0.72f },
            { 12500.0f, 0.0f, 0.72f }
        }};
    }
    else if (presetId == "phone")
    {
        curve = {{
            { 320.0f, 0.0f, 0.90f },
            { 420.0f, -4.5f, 0.72f },
            { 900.0f, 3.2f, 1.15f },
            { 1800.0f, 4.8f, 1.25f },
            { 3200.0f, 2.0f, 1.10f },
            { 4500.0f, -5.0f, 0.72f },
            { 5400.0f, 0.0f, 0.85f }
        }};
    }
    else if (presetId == "air")
    {
        curve = {{
            { 32.0f, 0.0f, 0.72f },
            { 90.0f, -0.8f, 0.72f },
            { 280.0f, -1.4f, 1.0f },
            { 1200.0f, 0.4f, 1.0f },
            { 4200.0f, 1.5f, 1.1f },
            { 10800.0f, 3.8f, 0.72f },
            { 19000.0f, 0.0f, 0.72f }
        }};
    }
    else if (presetId == "drum_knock")
    {
        curve = {{
            { 28.0f, 0.0f, 0.72f },
            { 75.0f, 2.4f, 0.70f },
            { 240.0f, -2.8f, 1.1f },
            { 850.0f, -0.8f, 1.0f },
            { 2800.0f, 2.0f, 1.05f },
            { 9000.0f, 1.1f, 0.72f },
            { 18000.0f, 0.0f, 0.72f }
        }};
    }

    for (int i = 0; i < 7; ++i)
        setEqBand(host_, effectId_, i, curve[(size_t)i]);

    refreshFromHost();
    repaint();
}

juce::Colour NativeEffectEditor::bandColour(int band) const
{
    static const juce::uint32 colours[] = { 0xff8b5cf6, 0xfff472b6, 0xfffb7185, 0xfffacc15, 0xff86efac, 0xff5eead4, 0xff60a5fa };
    return juce::Colour(colours[(size_t)juce::jlimit(0, 6, band)]);
}

float NativeEffectEditor::freqToX(float freq) const
{
    const float minL = std::log10(20.0f);
    const float maxL = std::log10(20000.0f);
    const float n = (std::log10(juce::jlimit(20.0f, 20000.0f, freq)) - minL) / (maxL - minL);
    return graphRect_.getX() + n * graphRect_.getWidth();
}

float NativeEffectEditor::xToFreq(float x) const
{
    const float n = juce::jlimit(0.0f, 1.0f, (x - graphRect_.getX()) / (float)juce::jmax(1, graphRect_.getWidth()));
    const float minL = std::log10(20.0f);
    const float maxL = std::log10(20000.0f);
    return std::pow(10.0f, minL + n * (maxL - minL));
}

float NativeEffectEditor::gainToY(float gain) const
{
    const float n = (juce::jlimit(-18.0f, 18.0f, gain) + 18.0f) / 36.0f;
    return graphRect_.getBottom() - n * graphRect_.getHeight();
}

float NativeEffectEditor::yToGain(float y) const
{
    const float n = juce::jlimit(0.0f, 1.0f, (graphRect_.getBottom() - y) / (float)juce::jmax(1, graphRect_.getHeight()));
    return -18.0f + n * 36.0f;
}

void NativeEffectEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::zinc950);
    auto b = getLocalBounds().reduced(12);
    Theme::drawChassis(g, b.toFloat(), 10.0f);

    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(18.0f).withStyle("Bold"));
    auto titleRow = b.removeFromTop(30);
    g.drawText(name_, titleRow, juce::Justification::centredLeft, true);

    if (type_ == "parametric-eq")
    {
        auto pr = presetButtonRect_.toFloat();
        juce::ColourGradient pg(juce::Colour(0xff27272a), pr.getX(), pr.getY(),
                                juce::Colour(0xff111114), pr.getX(), pr.getBottom(), false);
        g.setGradientFill(pg);
        g.fillRoundedRectangle(pr, 5.0f);
        g.setColour(Theme::accentBright.withAlpha(0.8f));
        g.drawRoundedRectangle(pr, 5.0f, 1.0f);
        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText("PRESETS", presetButtonRect_, juce::Justification::centred, true);

        g.setColour(juce::Colour(0xff10242b));
        g.fillRoundedRectangle(graphRect_.toFloat(), 4.0f);
        g.setColour(Theme::zinc700);
        g.drawRoundedRectangle(graphRect_.toFloat(), 4.0f, 1.0f);

        for (int i = 0; i <= 12; ++i)
        {
            const float x = graphRect_.getX() + graphRect_.getWidth() * i / 12.0f;
            g.setColour(juce::Colours::white.withAlpha(i % 3 == 0 ? 0.10f : 0.045f));
            g.drawVerticalLine((int)x, (float)graphRect_.getY(), (float)graphRect_.getBottom());
        }
        for (int i = -18; i <= 18; i += 6)
        {
            const float y = gainToY((float)i);
            g.setColour(i == 0 ? Theme::zinc300.withAlpha(0.48f) : juce::Colours::white.withAlpha(0.07f));
            g.drawHorizontalLine((int)y, (float)graphRect_.getX(), (float)graphRect_.getRight());
        }

        juce::Path curve;
        for (int x = graphRect_.getX(); x <= graphRect_.getRight(); ++x)
        {
            const float freq = xToFreq((float)x);
            float gain = 0.0f;
            for (int i = 1; i < 6; ++i)
            {
                const float f = host_.getNativeEffectParam(effectId_, "eq" + juce::String(i) + "Freq");
                const float gDb = host_.getNativeEffectParam(effectId_, "eq" + juce::String(i) + "Gain");
                const float q = host_.getNativeEffectParam(effectId_, "eq" + juce::String(i) + "Q");
                const float d = std::abs(std::log2(freq / juce::jmax(1.0f, f)));
                gain += gDb * std::exp(-d * d * q * 1.8f);
            }
            const float y = gainToY(gain);
            if (x == graphRect_.getX()) curve.startNewSubPath((float)x, y);
            else curve.lineTo((float)x, y);
        }
        g.setColour(Theme::zinc100.withAlpha(0.9f));
        g.strokePath(curve, juce::PathStrokeType(2.0f));

        static const char* freqLabels[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
        static const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
        for (int i = 0; i < 10; ++i)
        {
            const int x = (int)freqToX(freqs[i]);
            g.setColour(Theme::zinc500);
            g.drawText(freqLabels[i], x - 18, graphRect_.getBottom() + 2, 36, 12, juce::Justification::centred);
        }

        for (int i = 0; i < 7; ++i)
        {
            const float freq = host_.getNativeEffectParam(effectId_, "eq" + juce::String(i) + "Freq");
            const float gain = host_.getNativeEffectParam(effectId_, "eq" + juce::String(i) + "Gain");
            auto node = juce::Rectangle<float>(freqToX(freq) - 5.0f, gainToY(gain) - 5.0f, 10.0f, 10.0f);
            g.setColour(bandColour(i).withAlpha(i == selectedBand_ ? 0.95f : 0.55f));
            g.fillEllipse(node);
            g.setColour(juce::Colours::black);
            g.drawEllipse(node, 1.0f);
            g.setColour(Theme::zinc950);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(7.5f).withStyle("Bold"));
            g.drawText("C" + juce::String(i + 1), node.expanded(7.0f).toNearestInt(), juce::Justification::centred);

            auto br = bandRects_[(size_t)i];
            g.setColour(i == selectedBand_ ? bandColour(i).withAlpha(0.22f) : Theme::zinc900);
            g.fillRoundedRectangle(br, 3.0f);
            g.setColour(i == selectedBand_ ? bandColour(i) : Theme::zinc700);
            g.drawRoundedRectangle(br, 3.0f, 1.0f);
            g.setColour(bandColour(i));
            const float meter = juce::jmap(gain, -18.0f, 18.0f, 0.0f, br.getHeight());
            g.fillRoundedRectangle(br.withY(br.getBottom() - meter).withHeight(meter), 3.0f);
            g.setColour(Theme::zinc200);
            g.drawText("C" + juce::String(i + 1), br.toNearestInt().removeFromTop(16), juce::Justification::centred);
        }
    }
    else
    {
        auto graph = graphRect_.toFloat();
        g.setColour(juce::Colour(0xff24383d));
        g.fillRoundedRectangle(graph, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        for (int i = 1; i < 4; ++i)
        {
            g.drawVerticalLine((int)(graph.getX() + graph.getWidth() * i / 4.0f), graph.getY(), graph.getBottom());
            g.drawHorizontalLine((int)(graph.getY() + graph.getHeight() * i / 4.0f), graph.getX(), graph.getRight());
        }

        const float threshold = (float)thresholdSlider_.getValue();
        const float pre = (float)preSlider_.getValue();
        const float post = (float)postSlider_.getValue();
        juce::Path transfer;
        for (int i = 0; i <= 180; ++i)
        {
            float xNorm = juce::jmap((float)i, 0.0f, 180.0f, -1.0f, 1.0f);
            float x = xNorm * pre;
            const float sign = x < 0.0f ? -1.0f : 1.0f;
            const float ax = std::abs(x);
            if (ax > threshold)
            {
                const float over = (ax - threshold) / juce::jmax(0.001f, 1.0f - threshold);
                x = sign * (threshold + (1.0f - threshold) * std::tanh(over));
            }
            const float yNorm = juce::jlimit(-1.0f, 1.0f, x * post);
            const float px = graph.getX() + graph.getWidth() * i / 180.0f;
            const float py = graph.getCentreY() - yNorm * graph.getHeight() * 0.45f;
            if (i == 0) transfer.startNewSubPath(px, py);
            else transfer.lineTo(px, py);
        }
        g.setColour(Theme::zinc100);
        g.strokePath(transfer, juce::PathStrokeType(2.2f));
        g.setColour(Theme::accent);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(24.0f).withStyle("Bold"));
        g.drawText("SOFT", 18, 162, 78, 34, juce::Justification::centred);
        g.setColour(Theme::zinc200);
        g.drawText("CLIPPER", 96, 162, 125, 34, juce::Justification::centredLeft);
    }
}

void NativeEffectEditor::resized()
{
    auto area = getLocalBounds().reduced(18);
    auto topRow = area.removeFromTop(44);
    presetButtonRect_ = {};
    if (type_ == "parametric-eq")
        presetButtonRect_ = topRow.removeFromRight(96).withHeight(22).translated(0, 2);

    if (type_ == "parametric-eq")
    {
        auto right = area.removeFromRight(185);
        graphRect_ = area.reduced(4).withTrimmedBottom(28);

        auto bandArea = right.removeFromTop(225).reduced(5, 0);
        const int bandW = 19;
        for (int i = 0; i < 7; ++i)
            bandRects_[(size_t)i] = juce::Rectangle<float>((float)bandArea.getX() + i * 24.0f,
                                                           (float)bandArea.getY() + 22.0f,
                                                           (float)bandW, 156.0f);

        auto knobs = right.withTrimmedTop(4);
        const int knobW = 58;
        freqSlider_.setBounds(knobs.removeFromLeft(knobW));
        gainSlider_.setBounds(knobs.removeFromLeft(knobW));
        qSlider_.setBounds(knobs.removeFromLeft(knobW));
        freqLabel_.setBounds(freqSlider_.getBounds().removeFromTop(16));
        gainLabel_.setBounds(gainSlider_.getBounds().removeFromTop(16));
        qLabel_.setBounds(qSlider_.getBounds().removeFromTop(16));
    }
    else
    {
        auto top = area.removeFromTop(120);
        thresholdSlider_.setBounds(top.removeFromLeft(92));
        preSlider_.setBounds(top.removeFromLeft(92));
        postSlider_.setBounds(top.removeFromLeft(92));
        thresholdLabel_.setBounds(thresholdSlider_.getBounds().removeFromBottom(18));
        preLabel_.setBounds(preSlider_.getBounds().removeFromBottom(18));
        postLabel_.setBounds(postSlider_.getBounds().removeFromBottom(18));
        graphRect_ = area.reduced(4, 10);
    }
}

void NativeEffectEditor::mouseDown(const juce::MouseEvent& e)
{
    if (type_ != "parametric-eq") return;
    if (presetButtonRect_.contains(e.x, e.y))
    {
        showEqPresetMenu();
        return;
    }

    for (int i = 0; i < 7; ++i)
    {
        const float freq = host_.getNativeEffectParam(effectId_, "eq" + juce::String(i) + "Freq");
        const float gain = host_.getNativeEffectParam(effectId_, "eq" + juce::String(i) + "Gain");
        auto node = juce::Rectangle<float>(freqToX(freq) - 9.0f, gainToY(gain) - 9.0f, 18.0f, 18.0f);
        if (node.contains(e.position) || bandRects_[(size_t)i].contains(e.position))
        {
            draggingBand_ = i;
            selectBand(i);
            return;
        }
    }
}

void NativeEffectEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (type_ != "parametric-eq" || draggingBand_ < 0 || !graphRect_.expanded(20).contains(e.x, e.y))
        return;

    selectBand(draggingBand_);
    freqSlider_.setValue(xToFreq((float)e.x), juce::dontSendNotification);
    gainSlider_.setValue(yToGain((float)e.y), juce::dontSendNotification);
    pushEqBand();
}

NativeEffectWindow::NativeEffectWindow(const juce::String& title, std::unique_ptr<juce::Component> editor)
    : title_(title), editor_(std::move(editor))
{
    setOpaque(true);
    closeBtn_.setButtonText("X");
    closeBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff27272a));
    closeBtn_.setColour(juce::TextButton::textColourOffId, Theme::accentBright);
    closeBtn_.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(closeBtn_);

    if (editor_)
    {
        addAndMakeVisible(editor_.get());
        setSize(editor_->getWidth(), editor_->getHeight() + titleH_);
    }
}

NativeEffectWindow::~NativeEffectWindow() = default;

void NativeEffectWindow::paint(juce::Graphics& g)
{
    g.fillAll(Theme::zinc950);
    auto title = getLocalBounds().removeFromTop(titleH_);
    juce::ColourGradient grad(Theme::zinc800, 0, (float)title.getY(), Theme::zinc900, 0, (float)title.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRect(title);
    g.setColour(Theme::zinc700);
    g.drawHorizontalLine(title.getBottom() - 1, 0.0f, (float)getWidth());
    g.setColour(Theme::zinc200);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
    g.drawText(title_, title.reduced(10, 0).withTrimmedRight(32), juce::Justification::centredLeft, true);
    g.setColour(juce::Colours::black);
    g.drawRect(getLocalBounds());
}

void NativeEffectWindow::resized()
{
    auto b = getLocalBounds();
    auto title = b.removeFromTop(titleH_);
    closeBtn_.setBounds(title.removeFromRight(titleH_).reduced(3));
    if (editor_)
        editor_->setBounds(b);
}

void NativeEffectWindow::mouseDown(const juce::MouseEvent& e)
{
    draggingTitle_ = e.y < titleH_;
    if (draggingTitle_)
        dragger_.startDraggingComponent(this, e);
}

void NativeEffectWindow::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingTitle_)
        dragger_.dragComponent(this, e, nullptr);
}
