#include "ChannelRack.h"
#include "PluginHost.h"
#include "PatternsPanel.h"
#include "Theme.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>

namespace
{
juce::String parseAudioDragPathForRack(const juce::String& description)
{
    if (!description.startsWith("audio\n"))
        return description;

    const auto parts = juce::StringArray::fromLines(description);
    return parts.size() >= 3 ? parts[2] : juce::String();
}

juce::String makePatternChannelDragDescription(const juce::String& patternName,
                                               int channelIndex,
                                               const juce::String& channelName,
                                               int patternSteps)
{
    juce::String description;
    description << "pattern-channel\n"
                << patternName << "\n"
                << channelIndex << "\n"
                << channelName << "\n"
                << patternSteps;
    return description;
}

int foldMidiIntoC4ToC6ForRack(int midi)
{
    while (midi < 60)
        midi += 12;
    while (midi > 84)
        midi -= 12;

    return juce::jlimit(60, 84, midi);
}

struct MidiMenuRow
{
    int id = 0;
    juce::String text;
    bool header = false;
    bool enabled = true;
};

struct SavedMidiPattern
{
    juce::String genreId;
    juce::String laneId;
    juce::String name;
    std::vector<int> steps;
};

juce::File savedMidiPatternFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stratum DAW");
    dir.createDirectory();
    return dir.getChildFile("channel-rack-midi-patterns.json");
}

std::vector<SavedMidiPattern> loadSavedMidiPatterns()
{
    std::vector<SavedMidiPattern> result;
    const auto file = savedMidiPatternFile();
    if (!file.existsAsFile())
        return result;

    auto json = juce::JSON::parse(file);
    if (auto* arr = json.getArray())
    {
        for (const auto& item : *arr)
        {
            if (!item.isObject())
                continue;

            SavedMidiPattern p;
            p.genreId = item.getProperty("genreId", {}).toString();
            p.laneId = item.getProperty("laneId", {}).toString();
            p.name = item.getProperty("name", {}).toString();
            if (auto* steps = item.getProperty("steps", {}).getArray())
                for (const auto& s : *steps)
                    p.steps.push_back((int)s);

            if (p.genreId.isNotEmpty() && p.laneId.isNotEmpty() && p.name.isNotEmpty() && !p.steps.empty())
                result.push_back(std::move(p));
        }
    }
    return result;
}

void saveMidiPatterns(const std::vector<SavedMidiPattern>& patterns)
{
    juce::Array<juce::var> arr;
    for (const auto& p : patterns)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("genreId", p.genreId);
        obj->setProperty("laneId", p.laneId);
        obj->setProperty("name", p.name);
        juce::Array<juce::var> steps;
        for (int s : p.steps)
            steps.add(s != 0 ? 1 : 0);
        obj->setProperty("steps", steps);
        arr.add(juce::var(obj));
    }
    savedMidiPatternFile().replaceWithText(juce::JSON::toString(juce::var(arr), false));
}

void upsertSavedMidiPattern(SavedMidiPattern pattern)
{
    auto patterns = loadSavedMidiPatterns();
    for (auto& existing : patterns)
    {
        if (existing.genreId == pattern.genreId
            && existing.laneId == pattern.laneId
            && existing.name == pattern.name)
        {
            existing.steps = std::move(pattern.steps);
            saveMidiPatterns(patterns);
            return;
        }
    }
    patterns.push_back(std::move(pattern));
    saveMidiPatterns(patterns);
}

class MidiPatternCallout final : public juce::Component,
                                 public juce::FileDragAndDropTarget
{
public:
    MidiPatternCallout(juce::String title,
                       juce::String subtitle,
                       std::vector<MidiMenuRow> rows,
                       std::function<void(const juce::File&)> dropHandler,
                       std::function<void(int)> rowHandler)
        : title_(std::move(title)),
          subtitle_(std::move(subtitle)),
          rows_(std::move(rows)),
          onMidiDropped_(std::move(dropHandler)),
          onRowChosen_(std::move(rowHandler))
    {
        setSize(440, 620);
    }

    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (const auto& path : files)
        {
            const auto ext = juce::File(path).getFileExtension();
            if (ext.equalsIgnoreCase(".mid") || ext.equalsIgnoreCase(".midi"))
                return true;
        }
        return false;
    }

    void fileDragEnter(const juce::StringArray&, int, int) override
    {
        draggingMidi_ = true;
        repaint();
    }

    void fileDragExit(const juce::StringArray&) override
    {
        draggingMidi_ = false;
        repaint();
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        draggingMidi_ = false;
        for (const auto& path : files)
        {
            const juce::File file(path);
            const auto ext = file.getFileExtension();
            if (file.existsAsFile() && (ext.equalsIgnoreCase(".mid") || ext.equalsIgnoreCase(".midi")))
            {
                if (onMidiDropped_)
                    onMidiDropped_(file);
                if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                    box->dismiss();
                return;
            }
        }
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        const int maxScroll = juce::jmax(0, (int)rows_.size() * rowH_ - listRect_.getHeight());
        scrollY_ = juce::jlimit(0, maxScroll, scrollY_ - (int)std::round(wheel.deltaY * 80.0f));
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (dropRect_.contains(e.x, e.y))
            return;

        if (!listRect_.contains(e.x, e.y))
            return;

        const int idx = (e.y - listRect_.getY() + scrollY_) / rowH_;
        if (idx < 0 || idx >= (int)rows_.size())
            return;

        const auto& row = rows_[(size_t)idx];
        if (row.header || !row.enabled || row.id <= 0)
            return;

        if (onRowChosen_)
            onRowChosen_(row.id);
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff121216));

        auto area = getLocalBounds().reduced(14);
        g.setColour(Theme::accentBright);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
        g.drawText(title_, area.removeFromTop(22), juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
        g.drawText(subtitle_, area.removeFromTop(20), juce::Justification::centredLeft, true);

        dropRect_ = area.removeFromTop(74).reduced(0, 6);
        g.setColour(draggingMidi_ ? juce::Colour(0xff4a260d) : juce::Colour(0xff1b1b20));
        g.fillRoundedRectangle(dropRect_.toFloat(), 6.0f);
        g.setColour(draggingMidi_ ? Theme::accentBright : Theme::accent.withAlpha(0.9f));
        g.drawRoundedRectangle(dropRect_.toFloat(), 6.0f, 1.4f);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
        g.drawText("Drop MIDI here", dropRect_.withTrimmedBottom(24), juce::Justification::centred, true);
        g.setColour(Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
        g.drawText("Saves to this genre and lane, then applies it", dropRect_.withTrimmedTop(34),
                   juce::Justification::centred, true);

        area.removeFromTop(8);
        listRect_ = area;
        g.saveState();
        g.reduceClipRegion(listRect_);

        int y = listRect_.getY() - scrollY_;
        for (const auto& row : rows_)
        {
            juce::Rectangle<int> r(listRect_.getX(), y, listRect_.getWidth(), rowH_);
            y += rowH_;
            if (!r.intersects(listRect_))
                continue;

            if (row.header)
            {
                g.setColour(Theme::accentBright);
                g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
                g.drawText(row.text, r.reduced(8, 0), juce::Justification::centredLeft, true);
                continue;
            }

            if (row.id > 0 && row.enabled)
            {
                g.setColour(juce::Colour(0xff18181d));
                g.fillRect(r);
            }
            g.setColour(row.enabled ? Theme::zinc100 : Theme::zinc500);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f));
            g.drawText(row.text, r.reduced(12, 0), juce::Justification::centredLeft, true);
        }
        g.restoreState();
    }

private:
    juce::String title_;
    juce::String subtitle_;
    std::vector<MidiMenuRow> rows_;
    std::function<void(const juce::File&)> onMidiDropped_;
    std::function<void(int)> onRowChosen_;
    juce::Rectangle<int> dropRect_;
    juce::Rectangle<int> listRect_;
    int scrollY_ = 0;
    bool draggingMidi_ = false;
    static constexpr int rowH_ = 39;
};
}

ChannelRack::ChannelRack(PluginHost& pluginHost)
    : pluginHost_(pluginHost)
{
    setWantsKeyboardFocus(true);
    // Initialize with some default channels. Steps are sized to whatever the
    // current pattern length is (8 BAR = 128 by default), so toggling later
    // doesn't have to grow them from a smaller size.
    channels_.push_back({"Kick",  InstrumentType::Kick,  std::vector<bool>((size_t)totalSteps_, false)});
    channels_.push_back({"Snare", InstrumentType::Snare, std::vector<bool>((size_t)totalSteps_, false)});
    channels_.push_back({"Hihat", InstrumentType::Hihat, std::vector<bool>((size_t)totalSteps_, false)});
    channels_.push_back({"Clap",  InstrumentType::Clap,  std::vector<bool>((size_t)totalSteps_, false)});
    
    // Set some default pattern
    channels_[0].steps[0] = channels_[0].steps[4] = channels_[0].steps[8] = channels_[0].steps[12] = true;
    channels_[1].steps[4] = channels_[1].steps[12] = true;
    channels_[2].steps[2] = channels_[2].steps[6] = channels_[2].steps[10] = channels_[2].steps[14] = true;

    // Bottom-edge resize handle. The user can drag the bottom edge of the
    // channel-rack to grow / shrink it vertically. Width-resize is disabled.
    sizeConstrainer_.setMinimumHeight(HEADER_HEIGHT + CHANNEL_HEIGHT + 24);
    sizeConstrainer_.setMaximumHeight(4000);
    bottomResizer_.reset(new juce::ResizableEdgeComponent(
        this, &sizeConstrainer_, juce::ResizableEdgeComponent::bottomEdge));
    addAndMakeVisible(bottomResizer_.get());
    bottomResizer_->setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    fitWidthToStepCount();
}

ChannelRack::~ChannelRack()
{
    
}

void ChannelRack::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    int w = bounds.getWidth();
    int h = bounds.getHeight();
    auto fb = bounds.toFloat();
    
    // ── Brushed-metal chassis ───────────────────────────────
    juce::ColourGradient chassis(juce::Colour(0xff2a2a2e), 0.0f, 0.0f,
                                 juce::Colour(0xff121214), (float)w, (float)h, false);
    g.setGradientFill(chassis);
    g.fillRoundedRectangle(fb, 10.0f);
    
    // Chassis border (zinc-700)
    g.setColour(juce::Colour(0xff3f3f46));
    g.drawRoundedRectangle(fb.reduced(0.5f), 10.0f, 1.0f);
    
    // Inner top highlight (inset 0 1px rgba(255,255,255,0.1))
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawHorizontalLine(1, fb.getX() + 10, fb.getRight() - 10);
    
    // Volumetric glint (top-left)
    juce::ColourGradient glint(juce::Colours::white.withAlpha(0.04f), 0.0f, 0.0f,
                               juce::Colours::transparentBlack, (float)w * 0.6f, (float)h * 0.6f, true);
    g.setGradientFill(glint);
    g.fillRoundedRectangle(fb, 10.0f);
    
    // Brushed metal vertical lines (very subtle)
    g.setColour(juce::Colours::white.withAlpha(0.012f));
    for (int sx = 0; sx < w; sx += 4)
        g.drawVerticalLine(sx, 0.0f, (float)h);
    
    // Corner fastener screws
    auto drawScrew = [&](float cx, float cy, float angleDeg) {
        auto r = juce::Rectangle<float>(cx - 4, cy - 4, 8, 8);
        juce::ColourGradient sg(juce::Colour(0xff52525b), r.getX(), r.getY(),
                                juce::Colour(0xff27272a), r.getRight(), r.getBottom(), false);
        g.setGradientFill(sg);
        g.fillEllipse(r);
        g.setColour(juce::Colours::black);
        g.drawEllipse(r, 0.5f);
        // Slot
        g.setColour(juce::Colour(0xff09090b));
        juce::Path slot;
        slot.addRectangle(cx - 3, cy - 0.5f, 6.0f, 1.0f);
        slot.applyTransform(juce::AffineTransform::rotation(angleDeg * juce::MathConstants<float>::pi / 180.0f, cx, cy));
        g.fillPath(slot);
    };
    drawScrew(8.0f, 8.0f, 45.0f);
    drawScrew((float)w - 8.0f, 8.0f, -12.0f);
    drawScrew(8.0f, (float)h - 8.0f, 75.0f);
    drawScrew((float)w - 8.0f, (float)h - 8.0f, 6.0f);
    
    // ── Header strip (recessed) ─────────────────────────────
    auto header = bounds.removeFromTop(HEADER_HEIGHT);
    auto headerF = header.toFloat();
    juce::ColourGradient hg(juce::Colour(0xff18181b), 0.0f, headerF.getY(),
                            juce::Colour(0xff0d0d10), 0.0f, headerF.getBottom(), false);
    g.setGradientFill(hg);
    g.fillRect(headerF.withTrimmedLeft(2).withTrimmedRight(2).withTrimmedTop(2));
    
    // Header dividers (etched)
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(header.getBottom() - 1, 0.0f, (float)w);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawHorizontalLine(header.getBottom(), 0.0f, (float)w);

    auto stepCountRect = juce::Rectangle<float>(14.0f, (float)header.getY() + 6.0f, 58.0f, 18.0f);
    juce::ColourGradient scg(juce::Colour(0xff27272a), stepCountRect.getX(), stepCountRect.getY(),
                             juce::Colour(0xff141417), stepCountRect.getX(), stepCountRect.getBottom(), false);
    g.setGradientFill(scg);
    g.fillRoundedRectangle(stepCountRect, 4.0f);
    g.setColour(juce::Colour(0xff3f3f46));
    g.drawRoundedRectangle(stepCountRect, 4.0f, 1.0f);
    g.setColour(juce::Colour(0xfff97316));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
    {
        // Convert step count to bars (16 steps = 1 bar). Falls back to a
        // step count if the value isn't a clean multiple of 16 (e.g. legacy
        // project files with 16/32 step patterns).
        const int barCount = totalSteps_ / 16;
        const juce::String lengthLabel = (totalSteps_ > 0 && totalSteps_ % 16 == 0)
            ? (juce::String(barCount) + " BAR")
            : (juce::String(totalSteps_) + " STEP");
        g.drawText(lengthLabel, stepCountRect.toNearestInt(), juce::Justification::centred);
    }

    auto patternRect = juce::Rectangle<float>(78.0f, (float)header.getY() + 6.0f, 84.0f, 18.0f);
    juce::ColourGradient pcg(juce::Colour(0xff17171a), patternRect.getX(), patternRect.getY(),
                             juce::Colour(0xff0b0b0d), patternRect.getX(), patternRect.getBottom(), false);
    g.setGradientFill(pcg);
    g.fillRoundedRectangle(patternRect, 4.0f);
    g.setColour(juce::Colour(0xff2f2f35));
    g.drawRoundedRectangle(patternRect, 4.0f, 1.0f);
    g.setColour(juce::Colour(0xfff97316));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
    g.drawText(currentPatternName_, patternRect.toNearestInt().reduced(7, 0),
               juce::Justification::centredLeft, true);

    auto genreRect = getDrumGenreButtonRect().toFloat();
    if (!genreRect.isEmpty())
    {
        const bool hasGenre = !currentDrumPresetId_.equalsIgnoreCase("none")
                           && !currentDrumPresetId_.equalsIgnoreCase("empty");
        juce::ColourGradient gg(hasGenre ? Theme::accent : juce::Colour(0xff1f1f23),
                                genreRect.getX(), genreRect.getY(),
                                hasGenre ? Theme::accent.darker(0.38f) : juce::Colour(0xff0e0e11),
                                genreRect.getX(), genreRect.getBottom(), false);
        g.setGradientFill(gg);
        g.fillRoundedRectangle(genreRect, 4.0f);
        g.setColour(hasGenre ? Theme::accentBright : juce::Colour(0xff3f3f46));
        g.drawRoundedRectangle(genreRect, 4.0f, 1.0f);
        g.setColour(hasGenre ? juce::Colours::black : Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
        g.drawText(getCurrentDrumPresetLabel().toUpperCase(), genreRect.toNearestInt().reduced(7, 0),
                   juce::Justification::centred);
    }

    auto swingRect = getSwingButtonRect().toFloat();
    if (!swingRect.isEmpty())
    {
        const bool hasSwing = swingPreset_ != SwingPreset::None;
        juce::ColourGradient sg(hasSwing ? juce::Colour(0xff7c3aed) : juce::Colour(0xff1f1f23),
                                swingRect.getX(), swingRect.getY(),
                                hasSwing ? juce::Colour(0xff5b21b6) : juce::Colour(0xff0e0e11),
                                swingRect.getX(), swingRect.getBottom(), false);
        g.setGradientFill(sg);
        g.fillRoundedRectangle(swingRect, 4.0f);
        g.setColour(hasSwing ? juce::Colour(0xffa78bfa) : juce::Colour(0xff3f3f46));
        g.drawRoundedRectangle(swingRect, 4.0f, 1.0f);
        g.setColour(hasSwing ? juce::Colours::white : Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
        g.drawText(getSwingPresetLabel().toUpperCase(), swingRect.toNearestInt().reduced(4, 0),
                   juce::Justification::centred, true);
    }

    auto splitRect = getSplitButtonRect().toFloat();
    if (!splitRect.isEmpty())
    {
        juce::ColourGradient spg(juce::Colour(0xff1f1f23), splitRect.getX(), splitRect.getY(),
                                 juce::Colour(0xff0e0e11), splitRect.getX(), splitRect.getBottom(), false);
        g.setGradientFill(spg);
        g.fillRoundedRectangle(splitRect, 4.0f);
        g.setColour(juce::Colour(0xff3f3f46));
        g.drawRoundedRectangle(splitRect, 4.0f, 1.0f);
        g.setColour(Theme::zinc300);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
        g.drawText("SPLIT", splitRect.toNearestInt().reduced(4, 0), juce::Justification::centred, true);
    }

    const int volumeChannel = selectedChannel_ >= 0 && selectedChannel_ < (int)channels_.size() ? selectedChannel_ : 0;
    auto volRect = getHeaderVolumeRect();
    if (!volRect.isEmpty() && volumeChannel >= 0 && volumeChannel < (int)channels_.size())
    {
        const auto& ch = channels_[(size_t)volumeChannel];
        Theme::drawKnob(g, volRect.toFloat(), juce::jlimit(0.0f, 1.0f, ch.volume), Theme::orange2, true);
    }

    auto closeRect = getCloseButtonRect().toFloat();
    if (!closeRect.isEmpty())
    {
        juce::ColourGradient cg(juce::Colour(0xff27272a), closeRect.getX(), closeRect.getY(),
                                juce::Colour(0xff111114), closeRect.getX(), closeRect.getBottom(), false);
        g.setGradientFill(cg);
        g.fillRoundedRectangle(closeRect, 4.0f);
        g.setColour(juce::Colour(0xff3f3f46));
        g.drawRoundedRectangle(closeRect, 4.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.88f));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
        g.drawText("X", closeRect.toNearestInt(), juce::Justification::centred);
    }

    // Draw channels
    for (int i = 0; i < (int)channels_.size(); ++i)
    {
        auto channelBounds = bounds.removeFromTop(CHANNEL_HEIGHT);
        drawChannel(g, channelBounds, i);
    }

    // ── Bottom "+" button (FL Studio-style add VST instrument) ─────────
    auto plusR = getAddVstButtonRect().toFloat();
    if (plusR.getWidth() > 0)
    {
        juce::ColourGradient pg(juce::Colour(0xff27272a), plusR.getX(), plusR.getY(),
                                juce::Colour(0xff18181b), plusR.getX(), plusR.getBottom(), false);
        g.setGradientFill(pg);
        g.fillRoundedRectangle(plusR, 4.0f);
        g.setColour(juce::Colour(0xff3f3f46));
        g.drawRoundedRectangle(plusR, 4.0f, 1.0f);
        g.setColour(juce::Colour(0xfff97316));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(16.0f).withStyle("Bold"));
        g.drawText("+", plusR.toNearestInt(), juce::Justification::centred);
    }
}

void ChannelRack::resized()
{
    if (bottomResizer_)
        bottomResizer_->setBounds(0, getHeight() - 6, getWidth(), 6);
    fitWidthToStepCount();
}

void ChannelRack::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    isDraggingPanel_ = false;

    // Bottom "+" button: open VST instrument picker.
    if (getAddVstButtonRect().contains(e.x, e.y))
    {
        if (onAddVstChannel) onAddVstChannel();
        return;
    }
    
    if (e.y < HEADER_HEIGHT)
    {
        if (getCloseButtonRect().contains(e.x, e.y))
        {
            setVisible(false);
            return;
        }

        juce::Rectangle<int> stepCountRect(14, 6, 58, 18);
        if (stepCountRect.contains(e.x, e.y))
        {
            // Right-click → popup menu with explicit length choices
            // (16 / 32 steps + 4 BAR / 8 BAR). Left-click stays as the
            // simple toggle between 4 BAR ↔ 8 BAR.
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu menu;
                menu.addSectionHeader("Pattern length");
                menu.addItem(1, "16 steps",  true, totalSteps_ == 16);
                menu.addItem(2, "32 steps",  true, totalSteps_ == 32);
                menu.addItem(3, "4 BAR (64 steps)",  true, totalSteps_ == STEPS_4_BAR);
                menu.addItem(4, "8 BAR (128 steps)", true, totalSteps_ == STEPS_8_BAR);

                menu.showMenuAsync(juce::PopupMenu::Options{}
                    .withTargetComponent(this)
                    .withTargetScreenArea(localAreaToGlobal(stepCountRect)),
                    [this](int result)
                    {
                        int target = 0;
                        switch (result)
                        {
                            case 1: target = 16;            break;
                            case 2: target = 32;            break;
                            case 3: target = STEPS_4_BAR;   break;
                            case 4: target = STEPS_8_BAR;   break;
                            default: return;
                        }
                        if (target == totalSteps_) return;

                        // Resize every channel's step vector, preserving the
                        // existing pattern by wrapping/truncating like toggleStepCount.
                        const int oldTotal = totalSteps_;
                        totalSteps_ = target;
                        currentStep_ %= totalSteps_;
                        for (auto& ch : channels_)
                        {
                            auto previous = ch.steps;
                            ch.steps.assign((size_t)totalSteps_, false);
                            for (int s = 0; s < totalSteps_; ++s)
                            {
                                const int src = (oldTotal > 0) ? (s % oldTotal) : s;
                                if (src >= 0 && src < (int)previous.size())
                                    ch.steps[(size_t)s] = previous[(size_t)src];
                            }
                            const int dp = DEFAULT_DRUM_PITCH;
                            ch.pianoRollNotes.erase(std::remove_if(ch.pianoRollNotes.begin(),
                                ch.pianoRollNotes.end(),
                                [dp](const Channel::Note& n){ return n.pitch == dp; }),
                                ch.pianoRollNotes.end());
                            for (int s = 0; s < totalSteps_; ++s)
                                if (ch.steps[(size_t)s])
                                    ch.pianoRollNotes.push_back({ dp, s, 1, 100 });
                        }
                        fitWidthToStepCount();
                        repaint();
                        for (int i = 0; i < (int)channels_.size(); ++i)
                            if (onChannelDataChanged) onChannelDataChanged(i);
                    });
                return;
            }

            if (onToggle16_32) onToggle16_32();
            else toggleStepCount();
            return;
        }

        if (getHeaderVolumeRect().contains(e.x, e.y))
        {
            if (e.mods.isPopupMenu())
            {
                showHeaderVolumeMenu();
                return;
            }

            int idx = selectedChannel_;
            if (idx < 0 || idx >= (int)channels_.size())
                idx = channels_.empty() ? -1 : 0;
            if (idx >= 0)
            {
                draggingHeaderVolume_ = true;
                headerVolumeDragStartY_ = e.y;
                headerVolumeDragStartValue_ = channels_[(size_t)idx].volume;
            }
            return;
        }

        if (getDrumGenreButtonRect().contains(e.x, e.y))
        {
            if (e.mods.isPopupMenu())
                showDrumPatternVariantMenu();
            else
                showDrumPresetMenu();
            return;
        }

        if (getSwingButtonRect().contains(e.x, e.y))
        {
            showSwingMenu();
            return;
        }

        if (getSplitButtonRect().contains(e.x, e.y))
        {
            if (onSplitPatternToPlaylist)
                onSplitPatternToPlaylist();
            return;
        }

        // Header (top strip): start dragging the whole panel
        dragger_.startDraggingComponent(this, e);
        isDraggingPanel_ = true;
        return;
    }
    
    int channelIdx = getChannelAtY(e.y);
    if (channelIdx < 0 || channelIdx >= (int)channels_.size())
        return;
    
    auto& channel = channels_[channelIdx];
    int rowY = HEADER_HEIGHT + channelIdx * CHANNEL_HEIGHT;
    int cy = rowY + CHANNEL_HEIGHT / 2;

    // Channel index column (left-most number). Click → jump to mixer track.
    juce::Rectangle<int> indexRect (LEFT_PADDING, rowY, CH_INDEX_WIDTH, CHANNEL_HEIGHT);
    if (indexRect.contains (e.x, e.y))
    {
        selectedChannel_ = channelIdx;
        if (onChannelIndexClicked) onChannelIndexClicked (channelIdx);
        repaint();
        return;
    }
    
    // Layout (mirrors drawChannel):
    //  LEFT_PADDING + CH_INDEX_WIDTH + 6 → LED
    //  + CH_LED_SIZE + 6                → Mute
    //  + CH_MUTE_SIZE + 4               → Solo
    //  + CH_MUTE_SIZE + 6               → Name
    int ledX  = LEFT_PADDING + CH_INDEX_WIDTH + 6 + 2;
    int muteX = LEFT_PADDING + CH_INDEX_WIDTH + 6 + CH_LED_SIZE + 6;
    int soloX = muteX + CH_MUTE_SIZE + 4;
    int nameX = soloX + CH_MUTE_SIZE + 6;
    
    juce::Rectangle<float> ledRect ((float)ledX,  (float)cy - CH_LED_SIZE/2.0f,  (float)CH_LED_SIZE,  (float)CH_LED_SIZE);
    juce::Rectangle<float> muteRect((float)muteX, (float)cy - CH_MUTE_SIZE/2.0f, (float)CH_MUTE_SIZE, (float)CH_MUTE_SIZE);
    juce::Rectangle<float> soloRect((float)soloX, (float)cy - CH_MUTE_SIZE/2.0f, (float)CH_MUTE_SIZE, (float)CH_MUTE_SIZE);
    juce::Rectangle<float> nameRect((float)nameX, (float)rowY, (float)CH_NAME_WIDTH, (float)CHANNEL_HEIGHT);
    
    // LED or mute click → toggle mute
    if (ledRect.contains((float)e.x, (float)e.y) || muteRect.contains((float)e.x, (float)e.y))
    {
        channel.muted = !channel.muted;
        repaint();
        return;
    }
    
    // Solo click → toggle solo
    if (soloRect.contains((float)e.x, (float)e.y))
    {
        channel.solo = !channel.solo;
        repaint();
        return;
    }
    
    // Name click → select channel (open piano roll, etc.)
    if (nameRect.contains((float)e.x, (float)e.y))
    {
        selectedChannel_ = channelIdx;
        if (e.mods.isPopupMenu())
        {
            showChannelContextMenu(channelIdx, nameRect.toNearestInt());
            repaint();
            return;
        }
        pendingPatternDragChannel_ = channelIdx;
        pendingChannelNameClick_ = true;
        startedPatternChannelDrag_ = false;
        repaint();
        return;
    }

    // MIDI "R" button — right-click opens genre pattern menu, left-click rerolls hihat
    if (getMidiButtonRect(channelIdx).contains(e.x, e.y))
    {
        selectedChannel_ = channelIdx;
        if (e.mods.isRightButtonDown())
        {
            showMidiPatternMenu(channelIdx);
        }
        else
        {
            if (isHiHatChannel(channelIdx))
                rerollHiHatPattern();
            else
                showMidiPatternMenu(channelIdx);
        }
        return;
    }
    
    // Step click -> left adds/enables, right deletes/disables.
    int stepIdx = getStepAtX(e.x);
    if (stepIdx >= 0 && stepIdx < (int)channel.steps.size())
    {
        bool nowActive = e.mods.isRightButtonDown() ? false : true;
        channel.steps[stepIdx] = nowActive;
        selectedChannel_ = channelIdx;
        
        if (nowActive)
        {
            // Add a default-pitch note at this step if none exists there
            bool hasNoteAtStep = false;
            for (const auto& n : channel.pianoRollNotes)
                if (n.startStep == stepIdx) { hasNoteAtStep = true; break; }
            
            if (!hasNoteAtStep)
                channel.pianoRollNotes.push_back({ DEFAULT_DRUM_PITCH, stepIdx, 1, 100 });
        }
        else
        {
            // Remove all notes that start at this step
            channel.pianoRollNotes.erase(
                std::remove_if(channel.pianoRollNotes.begin(), channel.pianoRollNotes.end(),
                    [stepIdx](const Channel::Note& n) { return n.startStep == stepIdx; }),
                channel.pianoRollNotes.end());
        }
        
        if (onChannelDataChanged) onChannelDataChanged(channelIdx);
        repaint();
        return;
    }
    
    // Empty area click → just select the row
    selectedChannel_ = channelIdx;
    repaint();
}

bool ChannelRack::keyPressed(const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'q' || key.getTextCharacter() == 'Q')
    {
        auditionSelectedChannelC5();
        return true;
    }

    return false;
}

void ChannelRack::mouseDrag(const juce::MouseEvent& e)
{
    if (pendingPatternDragChannel_ >= 0 && !startedPatternChannelDrag_
        && e.getDistanceFromDragStart() >= 6)
    {
        const int ci = pendingPatternDragChannel_;
        if (ci >= 0 && ci < (int)channels_.size())
        {
            if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor(this))
            {
                startedPatternChannelDrag_ = true;
                pendingChannelNameClick_ = false;

                const auto& ch = channels_[(size_t)ci];
                auto dragImage = juce::Image(juce::Image::ARGB, 190, 32, true);
                juce::Graphics g(dragImage);
                juce::ColourGradient grad(juce::Colour(0xfff97316), 0.0f, 0.0f,
                                          juce::Colour(0xff9a3412), 0.0f, 32.0f, false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(0.0f, 0.0f, 190.0f, 32.0f, 5.0f);
                g.setColour(juce::Colours::white.withAlpha(0.92f));
                g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
                g.drawText(currentPatternName_ + " - " + ch.name, 10, 0, 172, 32,
                           juce::Justification::centredLeft, true);

                dnd->startDragging(makePatternChannelDragDescription(currentPatternName_, ci, ch.name, totalSteps_),
                                   this, dragImage);
                return;
            }
        }
    }

    if (draggingHeaderVolume_)
    {
        setSelectedChannelVolumeFromDrag(headerVolumeDragStartY_, e.y, headerVolumeDragStartValue_);
        return;
    }

    if (isDraggingPanel_)
        dragger_.dragComponent(this, e, nullptr);
}

void ChannelRack::mouseUp(const juce::MouseEvent&)
{
    if (pendingChannelNameClick_ && !startedPatternChannelDrag_
        && pendingPatternDragChannel_ >= 0 && pendingPatternDragChannel_ < (int)channels_.size())
    {
        selectedChannel_ = pendingPatternDragChannel_;
        if (onChannelClicked) onChannelClicked(pendingPatternDragChannel_);
    }

    draggingHeaderVolume_ = false;
    isDraggingPanel_ = false;
    pendingPatternDragChannel_ = -1;
    pendingChannelNameClick_ = false;
    startedPatternChannelDrag_ = false;
}

void ChannelRack::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.y < HEADER_HEIGHT)
    {
        auto volRect = getHeaderVolumeRect();
        if (!volRect.isEmpty() && volRect.contains(e.x, e.y))
        {
            int idx = selectedChannel_;
            if (idx < 0 || idx >= (int)channels_.size())
                idx = channels_.empty() ? -1 : 0;
            if (idx >= 0)
            {
                auto& ch = channels_[(size_t)idx];
                ch.volume = juce::jlimit(0.0f, 1.0f, ch.volume + (float)wheel.deltaY * 0.08f);
                repaint();
                if (onChannelDataChanged)
                    onChannelDataChanged(idx);
            }
            return;
        }
    }

    const int channelIdx = getChannelAtY(e.y);
    if (channelIdx >= 0 && channelIdx < (int)channels_.size())
    {
        if (!currentDrumPresetId_.equalsIgnoreCase("none") && !currentDrumPresetId_.equalsIgnoreCase("empty"))
        {
            juce::StringArray missing;
            rerollChannelSample(channelIdx, &missing);
        }
        return;
    }

    juce::Component::mouseWheelMove(e, wheel);
}

bool ChannelRack::isInterestedInDragSource(const SourceDetails& details)
{
    // Accept drags whose description is a string path (from the Browser)
    return details.description.isString();
}

void ChannelRack::itemDragEnter(const SourceDetails& details)
{
    dropHighlightRow_ = getChannelAtY(details.localPosition.y);
    repaint();
}

void ChannelRack::itemDragMove(const SourceDetails& details)
{
    dropHighlightRow_ = getChannelAtY(details.localPosition.y);
    repaint();
}

void ChannelRack::itemDragExit(const SourceDetails& /*details*/)
{
    dropHighlightRow_ = -1;
    repaint();
}

void ChannelRack::itemDropped(const SourceDetails& details)
{
    juce::String path = details.description.toString();
    if (path.startsWith("plugin\n"))
    {
        auto parts = juce::StringArray::fromLines(path);
        if (parts.size() >= 3)
        {
            const juce::String name = parts[1];
            const juce::String fileOrIdentifier = parts[2];
            juce::String err;
            int slotId = pluginHost_.loadPlugin(fileOrIdentifier, err);
            if (slotId >= 0)
            {
                int channelIdx = getChannelAtY(details.localPosition.y);
                if (channelIdx >= 0 && channelIdx < (int)channels_.size())
                {
                    auto& ch = channels_[(size_t)channelIdx];
                    ch.name = name;
                    ch.type = InstrumentType::Lead;
                    ch.sampleFile = {};
                    ch.builtInInstrument.clear();
                    ch.pluginSlotId = slotId;
                    pluginHost_.setSlotTrack(slotId, channelIdx);
                    selectedChannel_ = channelIdx;
                }
                else
                {
                    Channel ch;
                    ch.name = name;
                    ch.type = InstrumentType::Lead;
                    ch.steps = std::vector<bool>((size_t)totalSteps_, false);
                    ch.pluginSlotId = slotId;
                    ch.volume = 0.85f;
                    int newIdx = (int)channels_.size();
                    channels_.push_back(std::move(ch));
                    selectedChannel_ = newIdx;
                    pluginHost_.setSlotTrack(slotId, newIdx);
                }

                pluginHost_.showEditor(slotId, true);
                int bottomPad = 22;
                int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
                if (getHeight() < ideal) setSize(getWidth(), ideal);
                if (onChannelDataChanged) onChannelDataChanged(selectedChannel_);
                if (onChannelsChanged) onChannelsChanged();
            }
        }

        dropHighlightRow_ = -1;
        repaint();
        return;
    }

    juce::File file(parseAudioDragPathForRack(path));
    
    if (file.existsAsFile())
    {
        int channelIdx = getChannelAtY(details.localPosition.y);
        
        if (channelIdx >= 0 && channelIdx < (int)channels_.size())
        {
            // Drop onto an existing row → replace its sample (stay on current view)
            auto& ch = channels_[channelIdx];
            if (ch.pluginSlotId >= 0)
                pluginHost_.clearSlotTrack(ch.pluginSlotId);
            ch.sampleFile = file;
            ch.name = file.getFileNameWithoutExtension();
            ch.pluginSlotId = -1;
            ch.builtInInstrument.clear();
            selectedChannel_ = channelIdx;
            if (onChannelsChanged) onChannelsChanged();
        }
        else
        {
            // Drop onto empty space → add a new channel
            Channel ch;
            ch.name = file.getFileNameWithoutExtension();
            ch.type = InstrumentType::Kick;
            ch.steps = std::vector<bool>(totalSteps_, false);
            ch.sampleFile = file;
            int newIdx = (int)channels_.size();
            channels_.push_back(std::move(ch));
            selectedChannel_ = newIdx;

            int bottomPad = 22;
            int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
            if (getHeight() < ideal) setSize(getWidth(), ideal);
        }
    }

    if (onChannelsChanged) onChannelsChanged();

    dropHighlightRow_ = -1;
    repaint();
}

void ChannelRack::setPlaying(bool playing)
{
    const bool wasPlaying = isPlaying_;
    isPlaying_ = playing;

    if (playing)
    {
        ++playbackEpoch_;
        startTimer(60000.0 / bpm_ / 4.0); // 16th notes
        if (!wasPlaying)
        {
            int triggerStep = absoluteStep_ % totalSteps_;
            bool stepAllowed = true;
            if (getPlaybackStep)
            {
                triggerStep = getPlaybackStep(absoluteStep_, totalSteps_);
                stepAllowed = triggerStep >= 0;
            }
            else if (shouldPlayStep)
            {
                stepAllowed = shouldPlayStep(absoluteStep_);
            }

            currentStep_ = stepAllowed ? (triggerStep % totalSteps_) : (absoluteStep_ % totalSteps_);
            if (playbackAudible_ && stepAllowed)
            {
                const bool linearClipStep = isPlaylistPlaybackActive && isPlaylistPlaybackActive();
                for (int i = 0; i < (int)channels_.size(); ++i)
                {
                    int channelTriggerStep = triggerStep;
                    if (getPlaybackStepForChannel)
                    {
                        channelTriggerStep = getPlaybackStepForChannel(absoluteStep_, totalSteps_, i);
                        if (channelTriggerStep < 0)
                            continue;
                    }
                    else if (shouldPlayChannelAtStep && !shouldPlayChannelAtStep(absoluteStep_, i))
                    {
                        continue;
                    }

                    const auto& ch = channels_[(size_t)i];
                    const int channelLength = getChannelPatternLength(ch);
                    const bool melodic = isMelodicChannel(ch);
                    const int channelStep = melodic
                        ? (linearClipStep ? channelTriggerStep : (absoluteStep_ % juce::jmax(1, channelLength)))
                        : (channelTriggerStep % juce::jmax(1, (int)ch.steps.size()));
                    if (isMelodicChannel(ch) || (channelStep < (int)ch.steps.size() && ch.steps[(size_t)channelStep]))
                        triggerChannel(i, channelStep);
                }
            }
        }
    }
    else
    {
        ++playbackEpoch_;
        stopTimer();
    }

    if (onPlayheadTick) onPlayheadTick(absoluteStep_, isPlaying_);
}

void ChannelRack::setBPM(double bpm)
{
    bpm_ = bpm;
    if (isPlaying_)
    {
        stopTimer();
        startTimer(60000.0 / bpm_ / 4.0); // 16th notes
    }
}

void ChannelRack::setAbsoluteStep(int step)
{
    absoluteStep_ = juce::jmax(0, step);
    currentStep_ = absoluteStep_ % totalSteps_;
    repaint();
    if (onPlayheadTick) onPlayheadTick(absoluteStep_, isPlaying_);
}

void ChannelRack::toggleStepCount()
{
    const int oldTotalSteps = totalSteps_;
    // Toggle 4 BAR ↔ 8 BAR. Any legacy/unknown size jumps to 8 BAR first so
    // the user always lands on a defined state.
    totalSteps_ = (totalSteps_ == STEPS_8_BAR) ? STEPS_4_BAR : STEPS_8_BAR;
    currentStep_ %= totalSteps_;

    for (auto& ch : channels_)
    {
        auto previous = ch.steps;
        ch.steps.assign((size_t)totalSteps_, false);
        for (int step = 0; step < totalSteps_; ++step)
        {
            const int sourceStep = (oldTotalSteps > 0) ? (step % oldTotalSteps) : step;
            if (sourceStep >= 0 && sourceStep < (int)previous.size())
                ch.steps[(size_t)step] = previous[(size_t)sourceStep];
        }

        const int drumPitch = DEFAULT_DRUM_PITCH;
        ch.pianoRollNotes.erase(std::remove_if(ch.pianoRollNotes.begin(), ch.pianoRollNotes.end(),
            [drumPitch](const Channel::Note& n) { return n.pitch == drumPitch; }),
            ch.pianoRollNotes.end());
        for (int step = 0; step < totalSteps_; ++step)
            if (ch.steps[(size_t)step])
                ch.pianoRollNotes.push_back({ drumPitch, step, 1, 100 });
    }

    fitWidthToStepCount();
    repaint();
    for (int i = 0; i < (int)channels_.size(); ++i)
        if (onChannelDataChanged) onChannelDataChanged(i);
}

void ChannelRack::timerCallback()
{
    ++absoluteStep_;
    int triggerStep = absoluteStep_ % totalSteps_;
    bool stepAllowed = true;
    if (getPlaybackStep)
    {
        triggerStep = getPlaybackStep(absoluteStep_, totalSteps_);
        stepAllowed = triggerStep >= 0;
    }
    else if (shouldPlayStep)
    {
        stepAllowed = shouldPlayStep(absoluteStep_);
    }

    currentStep_ = stepAllowed ? (triggerStep % totalSteps_) : (absoluteStep_ % totalSteps_);
    
    // Trigger channels that have steps at current position.
    // Playlist mode still uses this timer as the transport clock, but mutes
    // rack-triggered audio so only timeline clips are heard.
    if (playbackAudible_ && stepAllowed)
    {
        const bool linearClipStep = isPlaylistPlaybackActive && isPlaylistPlaybackActive();
        for (int i = 0; i < (int)channels_.size(); ++i)
        {
            int channelTriggerStep = triggerStep;
            if (getPlaybackStepForChannel)
            {
                channelTriggerStep = getPlaybackStepForChannel(absoluteStep_, totalSteps_, i);
                if (channelTriggerStep < 0)
                    continue;
            }
            else if (shouldPlayChannelAtStep && !shouldPlayChannelAtStep(absoluteStep_, i))
            {
                continue;
            }

            const auto& ch = channels_[(size_t)i];
            const int channelLength = getChannelPatternLength(ch);
            const bool melodic = isMelodicChannel(ch);
            const int channelStep = melodic
                ? (linearClipStep ? channelTriggerStep : (absoluteStep_ % juce::jmax(1, channelLength)))
                : (channelTriggerStep % juce::jmax(1, (int)ch.steps.size()));
            if (melodic || (channelStep < (int)ch.steps.size() && ch.steps[(size_t)channelStep]))
                triggerChannel(i, channelStep);
        }
    }
    
    if (onPlayheadTick) onPlayheadTick(absoluteStep_, isPlaying_);
    repaint();
}

bool ChannelRack::isMelodicChannel(const Channel& channel) const
{
    if (isMusicLoopChannel(channel))
        return false;

    return channel.pluginSlotId >= 0
        || channel.builtInInstrument == "piano"
        || channel.builtInInstrument == "guitar"
        || channel.builtInInstrument == "bass"
        || channel.type == InstrumentType::Lead
        || channel.type == InstrumentType::Pad
        || channel.type == InstrumentType::Bass;
}

bool ChannelRack::isMusicLoopChannel(const Channel& channel) const
{
    return channel.isMusicLoop && channel.sampleFile.existsAsFile();
}

juce::String ChannelRack::musicLoopSlotLabel(int loopSlot)
{
    if (loopSlot <= 0)
        return "0";
    return "0." + juce::String(loopSlot);
}

void ChannelRack::buildWaveformPeaksForChannel(Channel& channel)
{
    channel.waveformPeaks.clear();
    if (!channel.sampleFile.existsAsFile())
        return;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(channel.sampleFile));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return;

    constexpr int peakCount = 192;
    channel.waveformPeaks.assign((size_t)peakCount, 0.0f);
    const juce::int64 totalSamples = reader->lengthInSamples;
    const int channels = juce::jlimit(1, 2, (int)reader->numChannels);

    for (int i = 0; i < peakCount; ++i)
    {
        const juce::int64 start = (totalSamples * i) / peakCount;
        const juce::int64 end = (totalSamples * (i + 1)) / peakCount;
        const int num = (int)juce::jlimit<juce::int64>(1, 8192, end - start);
        juce::AudioBuffer<float> buffer(channels, num);
        buffer.clear();
        reader->read(&buffer, 0, num, start, true, channels > 1);

        float peak = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* data = buffer.getReadPointer(ch);
            for (int s = 0; s < num; ++s)
                peak = juce::jmax(peak, std::abs(data[s]));
        }
        channel.waveformPeaks[(size_t)i] = juce::jlimit(0.0f, 1.0f, peak);
    }
}

int ChannelRack::getMusicLoopChannelCount() const
{
    int count = 0;
    for (const auto& ch : channels_)
        if (isMusicLoopChannel(ch))
            ++count;
    return count;
}

int ChannelRack::getDrumChannelIndexAmongDrums(int channelIndex) const
{
    if (channelIndex < 0 || channelIndex >= (int)channels_.size())
        return -1;
    if (isMusicLoopChannel(channels_[(size_t)channelIndex]))
        return -1;

    int drumIndex = 0;
    for (int i = 0; i < channelIndex; ++i)
    {
        if (!isMusicLoopChannel(channels_[(size_t)i]))
            ++drumIndex;
    }
    return drumIndex;
}

juce::String ChannelRack::getChannelStripNumber(int channelIndex) const
{
    if (channelIndex < 0 || channelIndex >= (int)channels_.size())
        return {};

    const auto& ch = channels_[(size_t)channelIndex];
    if (isMusicLoopChannel(ch))
        return musicLoopSlotLabel(ch.loopSlot);

    const int drumIndex = getDrumChannelIndexAmongDrums(channelIndex);
    return drumIndex >= 0 ? juce::String(drumIndex + 1) : juce::String();
}

int ChannelRack::getMixerTrackForLoopFile(const juce::File& file) const
{
    if (!file.existsAsFile())
        return -1;

    for (const auto& ch : channels_)
    {
        if (isMusicLoopChannel(ch) && ch.sampleFile == file)
            return ch.mixerTrack >= 0 ? ch.mixerTrack : ch.loopSlot;
    }
    return -1;
}

void ChannelRack::syncMusicLoopChannels(const std::vector<std::pair<juce::File, juce::String>>& loopsInOrder)
{
    std::vector<Channel> drumChannels;
    drumChannels.reserve(channels_.size());
    for (const auto& ch : channels_)
    {
        if (!isMusicLoopChannel(ch))
            drumChannels.push_back(ch);
    }

    std::vector<Channel> loopChannels;
    loopChannels.reserve(loopsInOrder.size());

    for (size_t i = 0; i < loopsInOrder.size(); ++i)
    {
        const auto& [file, label] = loopsInOrder[i];
        if (!file.existsAsFile())
            continue;

        Channel* existing = nullptr;
        for (auto& ch : channels_)
        {
            if (isMusicLoopChannel(ch) && ch.sampleFile == file)
            {
                existing = &ch;
                break;
            }
        }

        Channel loop;
        if (existing != nullptr)
            loop = *existing;
        else
        {
            loop.type = InstrumentType::Pad;
            loop.steps.assign((size_t)totalSteps_, false);
            loop.volume = 0.85f;
        }

        loop.isMusicLoop = true;
        loop.loopSlot = (int)i;
        loop.sampleFile = file;
        loop.name = label.isNotEmpty() ? label : file.getFileNameWithoutExtension();
        loop.pluginSlotId = -1;
        loop.builtInInstrument.clear();
        loop.mixerTrack = (int)i;
        if (loop.waveformPeaks.empty())
            buildWaveformPeaksForChannel(loop);

        loopChannels.push_back(std::move(loop));
    }

    channels_.clear();
    channels_.insert(channels_.end(), loopChannels.begin(), loopChannels.end());
    channels_.insert(channels_.end(), drumChannels.begin(), drumChannels.end());

    if (selectedChannel_ >= (int)channels_.size())
        selectedChannel_ = (int)channels_.size() - 1;

    int bottomPad = 22;
    int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
    if (getHeight() < ideal)
        setSize(getWidth(), ideal);
    fitWidthToStepCount();
    repaint();
    if (onChannelsChanged)
        onChannelsChanged();
}

int ChannelRack::getChannelPatternLength(const Channel& channel) const
{
    int length = juce::jmax(totalSteps_, (int)channel.steps.size());
    if (isMelodicChannel(channel))
    {
        for (const auto& note : channel.pianoRollNotes)
            length = juce::jmax(length, note.startStep + juce::jmax(1, note.lengthSteps));
    }
    return juce::jmax(1, length);
}

void ChannelRack::triggerChannel(int channelIdx, int playbackStep)
{
    if (channelIdx < 0 || channelIdx >= (int)channels_.size())
        return;

    const auto& ch = channels_[channelIdx];
    if (ch.muted) return;
    if (ch.builtInInstrument == "akai_mpc") return;
    const int stepToTrigger = playbackStep >= 0 ? playbackStep : currentStep_;
    const double swingDelay = getSwingDelaySeconds(stepToTrigger, ch);
    const int delayMs = juce::jmax(0, (int)std::round(swingDelay * 1000.0));
    if (delayMs > 0)
    {
        const uint64_t epoch = playbackEpoch_;
        juce::Component::SafePointer<ChannelRack> safe(this);
        juce::Timer::callAfterDelay(delayMs, [safe, channelIdx, playbackStep, epoch]()
        {
            if (safe == nullptr || !safe->isPlaying_ || safe->playbackEpoch_ != epoch)
                return;
            safe->triggerChannelImpl(channelIdx, playbackStep);
        });
        return;
    }

    triggerChannelImpl(channelIdx, playbackStep);
}

void ChannelRack::auditionChannel(int channelIndex)
{
    triggerChannel(channelIndex);
}

void ChannelRack::auditionPianoRollNote(int channelIdx, int pitch, int lengthSteps, int velocity)
{
    if (channelIdx < 0 || channelIdx >= (int)channels_.size())
        return;

    const auto& ch = channels_[(size_t)channelIdx];
    // Note: deliberately NO mute/solo check here — auditioning a note should
    // always make sound regardless of channel mute/solo state.
    if (isMusicLoopChannel(ch))
        return;

    const int safePitch = juce::jlimit(0, 127, pitch);
    const int safeVel   = juce::jlimit(1, 127, velocity);
    const int safeLen   = juce::jmax(1, lengthSteps);
    const int holdSteps = pianoRealFeel_ ? juce::jmax(6, safeLen) : safeLen;

    // VST instrument → MIDI note on, scheduled off after hold duration.
    if (ch.pluginSlotId >= 0)
    {
        const int slot = ch.pluginSlotId;
        const double stepMs = 60000.0 / juce::jmax(1.0, bpm_) / 4.0;
        const int offDelayMs = juce::jmax(50, (int)std::round(stepMs * holdSteps));
        const int midiVel = juce::jlimit(1, 127, (int)std::round(ch.volume * (float)safeVel));
        pluginHost_.sendMidiNote(slot, 1, safePitch, midiVel, true);
        juce::Timer::callAfterDelay(offDelayMs, [this, slot, safePitch]() {
            pluginHost_.sendMidiNote(slot, 1, safePitch, 0, false);
        });
        return;
    }

    const double secondsPerStep = 60.0 / juce::jmax(1.0, bpm_) / 4.0;
    const float chanVel = juce::jlimit(0.0f, 1.0f, ch.volume * ((float)safeVel / 127.0f));
    const int track = (ch.mixerTrack >= 0) ? ch.mixerTrack : channelIdx;

    if (ch.builtInInstrument == "piano")
    {
        pluginHost_.playSynthPiano(safePitch, 0.0, secondsPerStep * holdSteps, chanVel, track);
        return;
    }
    if (ch.builtInInstrument == "bass")
    {
        pluginHost_.playSynthBass(safePitch, 0.0, secondsPerStep * safeLen, chanVel, track);
        return;
    }

    if (ch.sampleFile.existsAsFile())
    {
        // Smooth release on the previous voice (no click on 808 retrigger).
        pluginHost_.stopSampleVoicesOnTrack(ch.sampleFile, track, false);
        if (ch.type == InstrumentType::Bass || ch.type == InstrumentType::Lead || ch.type == InstrumentType::Pad)
        {
            const double rate = std::pow(2.0, ((double)safePitch - (double)DEFAULT_DRUM_PITCH) / 12.0);
            pluginHost_.playSampleFile(ch.sampleFile, track, 0.0, chanVel, rate);
        }
        else
        {
            pluginHost_.playSampleFile(ch.sampleFile, track, 0.0, chanVel);
        }
        return;
    }

    // Last resort fallback: synth tone at the clicked pitch.
    const double frequency = 440.0 * std::pow(2.0, ((double)safePitch - 69.0) / 12.0);
    pluginHost_.playSynthTone(frequency, 0, 0.2, chanVel);
}

void ChannelRack::triggerChannelImpl(int channelIdx, int playbackStep)
{
    if (channelIdx < 0 || channelIdx >= (int)channels_.size())
        return;
    
    const auto& ch = channels_[channelIdx];
    if (ch.muted) return;

    // Solo: if any channel is soloed, silence all non-soloed channels
    const bool anySolo = std::any_of(channels_.begin(), channels_.end(),
                                     [](const Channel& c) { return c.solo; });
    if (anySolo && !ch.solo) return;

    if (isMusicLoopChannel(ch))
        return;
    const int stepToTrigger = playbackStep >= 0 ? playbackStep : currentStep_;

    auto collectNotesAtCurrentStep = [&]() {
        std::vector<Channel::Note> notes;
        for (const auto& n : ch.pianoRollNotes)
            if (n.startStep == stepToTrigger)
                notes.push_back(n);
        if (notes.empty() && !isMelodicChannel(ch))
            notes.push_back({ DEFAULT_DRUM_PITCH, stepToTrigger, 1, 100 });
        return notes;
    };

    // If a VST instrument is loaded for this channel, send MIDI notes to it.
    // Used for things like Kontakt where the plugin renders the sound itself.
    if (ch.pluginSlotId >= 0)
    {
        const int slot  = ch.pluginSlotId;
        const double stepMs = 60000.0 / juce::jmax(1.0, bpm_) / 4.0;
        const int minRealFeelSteps = pianoRealFeel_ ? 6 : 1;
        const bool drumHit = !isMelodicChannel(ch);
        if (drumHit)
            pluginHost_.sendAllNotesOff(slot, 1);

        for (const auto& note : collectNotesAtCurrentStep())
        {
            const int pitch = juce::jlimit(0, 127, note.pitch);
            const int velocity = juce::jlimit(1, 127, (int)std::round(ch.volume * (float)note.velocity));
            const int holdSteps = juce::jmax(minRealFeelSteps, note.lengthSteps);
            const int offDelayMs = juce::jmax(40, (int)std::round(stepMs * holdSteps));
            pluginHost_.sendMidiNote(slot, 1, pitch, velocity, true);
            const uint64_t epoch = playbackEpoch_;
            juce::Timer::callAfterDelay(offDelayMs, [this, slot, pitch, epoch]()
            {
                if (playbackEpoch_ != epoch)
                    return;
                pluginHost_.sendMidiNote(slot, 1, pitch, 0, false);
            });
        }
        return;
    }

    if (ch.builtInInstrument == "piano")
    {
        const double secondsPerStep = 60.0 / juce::jmax(1.0, bpm_) / 4.0;
        const int minRealFeelSteps = pianoRealFeel_ ? 6 : 1;
        for (const auto& note : collectNotesAtCurrentStep())
        {
            const float velocity = juce::jlimit(0.0f, 1.0f, ch.volume * ((float)note.velocity / 127.0f));
            const int track = (ch.mixerTrack >= 0) ? ch.mixerTrack : channelIdx;
            pluginHost_.playSynthPiano(note.pitch, 0.0, secondsPerStep * juce::jmax(minRealFeelSteps, note.lengthSteps), velocity, track);
        }
        return;
    }

    if (ch.builtInInstrument == "bass")
    {
        const double secondsPerStep = 60.0 / juce::jmax(1.0, bpm_) / 4.0;
        for (const auto& note : collectNotesAtCurrentStep())
        {
            const float velocity = juce::jlimit(0.0f, 1.0f, ch.volume * ((float)note.velocity / 127.0f));
            const int track = (ch.mixerTrack >= 0) ? ch.mixerTrack : channelIdx;
            pluginHost_.playSynthBass(note.pitch, 0.0, secondsPerStep * juce::jmax(1, note.lengthSteps), velocity, track);
        }
        return;
    }

    // If a sample file is assigned (e.g. dragged from Browser), play it.
    // Route through the channel's assigned mixer track (default = row index).
    if (ch.sampleFile.existsAsFile())
    {
        const int track = (ch.mixerTrack >= 0) ? ch.mixerTrack : channelIdx;
        const auto notes = collectNotesAtCurrentStep();
        // For melodic sample channels (808, lead, pad), the timer calls us on
        // EVERY step. If no note starts at this step, just return — don't
        // choke the sustaining voice. Otherwise a long 808 note would get cut
        // off every 16th step and sound broken.
        if (notes.empty())
            return;
        // Use the release envelope (immediate=false) rather than an instant
        // kill — for 808 sub-bass an abrupt cut mid-cycle creates an audible
        // click. The release ramp gives the FL-Studio "Cut Itself" feel.
        pluginHost_.stopSampleVoicesOnTrack(ch.sampleFile, track, false);
        if (ch.type == InstrumentType::Bass || ch.type == InstrumentType::Lead || ch.type == InstrumentType::Pad)
        {
            for (const auto& note : notes)
            {
                const double rate = std::pow(2.0, ((double)note.pitch - (double)DEFAULT_DRUM_PITCH) / 12.0);
                const float velocity = juce::jlimit(0.0f, 1.0f, ch.volume * ((float)note.velocity / 127.0f));
                pluginHost_.playSampleFile(ch.sampleFile, track, 0.0, velocity, rate);
            }
        }
        else
        {
            pluginHost_.playSampleFile(ch.sampleFile, track, 0.0, ch.volume);
        }
        return;
    }
    
    // No assigned sample, plugin, or explicit built-in instrument: stay silent.
    // This prevents empty drum slots from playing placeholder synth sounds.
}

void ChannelRack::auditionSelectedChannelC5()
{
    int idx = selectedChannel_;
    if (idx < 0 || idx >= (int)channels_.size())
        return;

    const auto& ch = channels_[(size_t)idx];
    if (ch.muted)
        return;

    constexpr int c5 = 72;
    const int velocity = juce::jlimit(1, 127, (int)std::round(ch.volume * 110.0f));

    if (ch.pluginSlotId >= 0)
    {
        const int slot = ch.pluginSlotId;
        pluginHost_.sendMidiNote(slot, 1, c5, velocity, true);
        juce::Timer::callAfterDelay(360, [this, slot, c5]() {
            pluginHost_.sendMidiNote(slot, 1, c5, 0, false);
        });
        return;
    }

    if (ch.builtInInstrument == "piano")
    {
        const int track = (ch.mixerTrack >= 0) ? ch.mixerTrack : idx;
        pluginHost_.playSynthPiano(c5, 0.0, 0.45, ch.volume, track);
        return;
    }

    if (ch.builtInInstrument == "bass")
    {
        const int track = (ch.mixerTrack >= 0) ? ch.mixerTrack : idx;
        pluginHost_.playSynthBass(36, 0.0, 0.55, ch.volume, track);
        return;
    }

    if (ch.sampleFile.existsAsFile())
    {
        const int track = (ch.mixerTrack >= 0) ? ch.mixerTrack : idx;
        pluginHost_.playSampleFile(ch.sampleFile, track, 0.0, ch.volume);
    }
}

juce::Rectangle<int> ChannelRack::getAddVstButtonBounds() const
{
    return getAddVstButtonRect();
}

juce::Rectangle<int> ChannelRack::getAddVstButtonRect() const
{
    // Centered, just below the last channel row.
    int y = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + 6;
    int btnW = 32, btnH = 18;
    if (y + btnH > getHeight() - 8) return {}; // no room
    int x = getWidth() / 2 - btnW / 2;
    return { x, y, btnW, btnH };
}

int ChannelRack::getChannelAtY(int y) const
{
    if (y < HEADER_HEIGHT) return -1;
    int channelY = y - HEADER_HEIGHT;
    return channelY / CHANNEL_HEIGHT;
}

int ChannelRack::getStepAtX(int x) const
{
    if (x < CHANNELS_START_X) return -1;

    const int stepW = stepCellWidth();
    const int stepG = stepCellGap();
    const int beatG = beatCellGap();
    const int relativeX = x - CHANNELS_START_X;
    // Narrower cells in 4/8-bar mode need a tighter hit-pad so we don't bleed
    // into the neighbouring cell. Cap at 3 to preserve the 16-step feel.
    const int hitPad = juce::jmin(3, stepG);
    int stepX = 0;

    for (int step = 0; step < totalSteps_; ++step)
    {
        if (step % 4 == 0 && step > 0)
            stepX += beatG;

        if (relativeX >= stepX - hitPad && relativeX < stepX + stepW + hitPad)
            return step;

        stepX += stepW + stepG;
    }

    return -1;
}

int ChannelRack::stepCellWidth() const
{
    // 4 BAR / 8 BAR patterns have far more cells than the original 16 STEP
    // layout — shrink each cell so the rack stays within a desktop window.
    if (totalSteps_ <= 16) return STEP_WIDTH;   // 18 — legacy 16-step
    if (totalSteps_ <= 32) return 14;            // legacy 32-step
    if (totalSteps_ <= 64) return 10;            // 4 BAR
    return 7;                                    // 8 BAR
}

int ChannelRack::stepCellGap() const
{
    return totalSteps_ <= 32 ? STEP_GAP : 1;
}

int ChannelRack::beatCellGap() const
{
    if (totalSteps_ <= 32) return BEAT_GAP;
    if (totalSteps_ <= 64) return 4;
    return 3;
}

int ChannelRack::getRequiredWidthForSteps() const
{
    const int stepW = stepCellWidth();
    const int stepG = stepCellGap();
    const int beatG = beatCellGap();

    int stepsW = 0;
    for (int step = 0; step < totalSteps_; ++step)
    {
        if (step % 4 == 0 && step > 0)
            stepsW += beatG;
        stepsW += stepW + stepG;
    }
    return CHANNELS_START_X + stepsW + 48;
}

void ChannelRack::fitWidthToStepCount()
{
    const int targetW = getRequiredWidthForSteps();
    if (getWidth() <= 0)
        setSize(targetW, getHeight() > 0 ? getHeight() : HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + 22);
    else if (getWidth() != targetW)
        setSize(targetW, getHeight());
}

bool ChannelRack::isHiHatChannel(int channelIndex) const
{
    if (channelIndex < 0 || channelIndex >= (int)channels_.size())
        return false;

    const auto& ch = channels_[(size_t)channelIndex];
    return ch.type == InstrumentType::Hihat && ch.name.containsIgnoreCase("hihat");
}


juce::Rectangle<int> ChannelRack::getMidiButtonRect(int channelIndex) const
{
    if (channelIndex < 0 || channelIndex >= (int)channels_.size())
        return {};

    // For melodic channels (piano roll preview), skip the R button
    const auto& ch = channels_[(size_t)channelIndex];
    if (isMelodicChannel(ch) && !ch.pianoRollNotes.empty())
        return {};

    const int stepW = stepCellWidth();
    const int stepG = stepCellGap();
    const int beatG = beatCellGap();
    int stepAreaW = 0;
    for (int step = 0; step < totalSteps_; ++step)
    {
        if (step % 4 == 0 && step > 0)
            stepAreaW += beatG;
        stepAreaW += stepW + stepG;
    }

    const int rowY = HEADER_HEIGHT + channelIndex * CHANNEL_HEIGHT;
    return juce::Rectangle<int>(CHANNELS_START_X + stepAreaW + 5,
                                rowY + (CHANNEL_HEIGHT - 18) / 2,
                                18, 18);
}

juce::Rectangle<int> ChannelRack::getHiHatChangeButtonRect(int channelIndex) const
{
    if (!isHiHatChannel(channelIndex))
        return {};
    return getMidiButtonRect(channelIndex);
}

juce::Rectangle<int> ChannelRack::getCloseButtonRect() const
{
    const int buttonSize = 22;
    const int x = getWidth() - buttonSize - 8;
    if (x < CHANNELS_START_X + 60)
        return {};
    return juce::Rectangle<int>(x, (HEADER_HEIGHT - buttonSize) / 2, buttonSize, buttonSize);
}

juce::Rectangle<int> ChannelRack::getHeaderVolumeRect() const
{
    const int knobSize = 24;
    const auto split = getSplitButtonRect();
    if (split.isEmpty())
        return {};

    const int x = split.getRight() + 8;
    const auto close = getCloseButtonRect();
    if ((x + knobSize > getWidth() - 8) || (!close.isEmpty() && x + knobSize + 8 > close.getX()))
        return {};
    return juce::Rectangle<int>(x, (HEADER_HEIGHT - knobSize) / 2, knobSize, knobSize);
}

juce::Rectangle<int> ChannelRack::getDrumGenreButtonRect() const
{
    const int x = 168;
    const int w = 72;
    return juce::Rectangle<int>(x, 6, w, 18);
}

juce::Rectangle<int> ChannelRack::getSwingButtonRect() const
{
    auto genre = getDrumGenreButtonRect();
    if (genre.isEmpty())
        return {};
    const int w = 64;
    const int x = genre.getRight() + 4;
    if (x + w > getWidth() - 34)
        return {};
    return juce::Rectangle<int>(x, 6, w, 18);
}

juce::Rectangle<int> ChannelRack::getSplitButtonRect() const
{
    auto swing = getSwingButtonRect();
    if (swing.isEmpty())
        return {};
    const int w = 58;
    const int x = swing.getRight() + 4;
    const auto close = getCloseButtonRect();
    const int limit = close.isEmpty() ? getWidth() - 8 : close.getX() - 8;
    if (x + w > limit)
        return {};
    return juce::Rectangle<int>(x, 6, w, 18);
}

juce::String ChannelRack::getSwingPresetLabel() const
{
    switch (swingPreset_)
    {
        case SwingPreset::Dilla:       return "Dilla";
        case SwingPreset::MfDoom:      return "MF Doom";
        case SwingPreset::JoeyBadass:  return "Joey";
        case SwingPreset::None:
        default:                       return "Swing";
    }
}

namespace
{
    struct SwingProfile
    {
        float offBeatRatio = 0.0f;
        float kickMul = 0.0f;
        float snareMul = 0.0f;
        float hatMul = 1.0f;
        float otherMul = 0.65f;
    };

    SwingProfile profileFor(ChannelRack::SwingPreset preset)
    {
        switch (preset)
        {
            case ChannelRack::SwingPreset::Dilla:
                return { 0.62f, 0.0f, 0.10f, 1.0f, 0.78f };
            case ChannelRack::SwingPreset::MfDoom:
                return { 0.54f, 0.0f, 0.14f, 0.96f, 0.68f };
            case ChannelRack::SwingPreset::JoeyBadass:
                return { 0.40f, 0.0f, 0.22f, 0.86f, 0.52f };
            case ChannelRack::SwingPreset::None:
            default:
                return {};
        }
    }
}

void ChannelRack::setSwingPreset(SwingPreset preset)
{
    swingPreset_ = preset;
    repaint();
}

double ChannelRack::getSwingDelaySeconds(int stepIndex, const Channel& channel) const
{
    if (swingPreset_ == SwingPreset::None || stepIndex < 0)
        return 0.0;

    const int stepInBar = stepIndex % juce::jmax(1, totalSteps_);
    if ((stepInBar % 2) == 0)
        return 0.0;

    const auto profile = profileFor(swingPreset_);
    float mul = profile.otherMul;
    const auto name = channel.name.toLowerCase();
    if (channel.type == InstrumentType::Kick)
        mul = profile.kickMul;
    else if (channel.type == InstrumentType::Snare)
        mul = profile.snareMul;
    else if (channel.type == InstrumentType::Hihat
          || name.contains("hat") || name.contains("ride") || name.contains("cym")
          || name.contains("shaker") || name.contains("perc"))
        mul = profile.hatMul;

    if (mul <= 0.0001f)
        return 0.0;

    const double halfStepSec = (60.0 / juce::jmax(1.0, bpm_)) / 4.0 * 0.5;
    return halfStepSec * profile.offBeatRatio * mul;
}

void ChannelRack::showSwingMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("Swing feel");
    menu.addItem(1, "None", true, swingPreset_ == SwingPreset::None);
    menu.addSeparator();
    menu.addItem(2, "J Dilla", true, swingPreset_ == SwingPreset::Dilla);
    menu.addItem(3, "MF DOOM", true, swingPreset_ == SwingPreset::MfDoom);
    menu.addItem(4, "Joey Badass", true, swingPreset_ == SwingPreset::JoeyBadass);

    auto target = getSwingButtonRect();
    if (target.isEmpty())
        target = getDrumGenreButtonRect();

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(target)),
        [this](int result)
        {
            if (result == 1) setSwingPreset(SwingPreset::None);
            if (result == 2) setSwingPreset(SwingPreset::Dilla);
            if (result == 3) setSwingPreset(SwingPreset::MfDoom);
            if (result == 4) setSwingPreset(SwingPreset::JoeyBadass);
        });
}

juce::String ChannelRack::getCurrentDrumPresetLabel() const
{
    if (currentDrumPresetId_.isEmpty()
        || currentDrumPresetId_.equalsIgnoreCase("none")
        || currentDrumPresetId_.equalsIgnoreCase("empty"))
        return "None";

    const auto id = currentDrumPresetId_.toLowerCase();
    if (id == "boom_bap") return "Boom Bap";
    if (id == "hiphop") return "Hip Hop";
    if (id == "trap") return "Trap";
    if (id == "drill") return "Drill";
    if (id == "house") return "House";
    if (id == "rnb") return "R&B";
    if (id == "lofi") return "Lo-Fi";
    if (id == "rock") return "Rock";
    if (id == "detroit") return "Detroit";
    if (id == "afrobeat") return "Afrobeat";
    if (id == "reggaeton") return "Reggaeton";
    if (id == "jersey") return "Jersey";
    if (id == "ukg") return "UK Garage";
    if (id == "dnb") return "D&B";
    if (id == "techno") return "Techno";
    if (id == "phonk") return "Phonk";
    if (id == "memphis") return "Memphis";
    if (id == "funk") return "Funk";

    return currentDrumPresetId_;
}

void ChannelRack::showDrumPresetMenu()
{
    struct PresetMenuItem
    {
        const char* id;
        const char* label;
    };

    static const PresetMenuItem items[] = {
        { "boom_bap", "Boom Bap" },
        { "hiphop", "Hip Hop" },
        { "trap", "Trap" },
        { "drill", "Drill" },
        { "house", "House" },
        { "rnb", "R&B" },
        { "lofi", "Lo-Fi" },
        { "rock", "Rock" },
        { "detroit", "Detroit Flint" },
        { "afrobeat", "Afrobeat" },
        { "reggaeton", "Reggaeton" },
        { "jersey", "Jersey Club" },
        { "ukg", "UK Garage" },
        { "dnb", "Drum & Bass" },
        { "techno", "Techno" },
        { "phonk", "Phonk" },
        { "memphis", "Memphis" },
        { "funk", "Funk" },
        { "empty", "Clear All" }
    };

    juce::PopupMenu menu;
    menu.addSectionHeader("Drum Pattern Presets");

    for (int i = 0; i < (int)(sizeof(items) / sizeof(items[0])); ++i)
    {
        if (juce::String(items[i].id) == "empty")
            menu.addSeparator();

        menu.addItem(i + 1,
                     items[i].label,
                     true,
                     currentDrumPresetId_.equalsIgnoreCase(items[i].id));
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea(localAreaToGlobal(getDrumGenreButtonRect()))
            .withMinimumWidth(220),
        [this](int result)
        {
            if (result <= 0)
                return;

            static const PresetMenuItem callbackItems[] = {
                { "boom_bap", "Boom Bap" },
                { "hiphop", "Hip Hop" },
                { "trap", "Trap" },
                { "drill", "Drill" },
                { "house", "House" },
                { "rnb", "R&B" },
                { "lofi", "Lo-Fi" },
                { "rock", "Rock" },
                { "detroit", "Detroit Flint" },
                { "afrobeat", "Afrobeat" },
                { "reggaeton", "Reggaeton" },
                { "jersey", "Jersey Club" },
                { "ukg", "UK Garage" },
                { "dnb", "Drum & Bass" },
                { "techno", "Techno" },
                { "phonk", "Phonk" },
                { "memphis", "Memphis" },
                { "funk", "Funk" },
                { "empty", "Clear All" }
            };

            const int index = result - 1;
            if (index < 0 || index >= (int)(sizeof(callbackItems) / sizeof(callbackItems[0])))
                return;

            const juce::String presetId(callbackItems[index].id);
            const juce::String presetLabel(callbackItems[index].label);

            if (onDrumGenreButtonClicked)
                onDrumGenreButtonClicked(presetId, presetLabel);
            else
                applyDrumPreset(presetId);

            repaint();
        });
}

void ChannelRack::showDrumPatternVariantMenu()
{
    const auto presetId = currentDrumPresetId_.toLowerCase();
    if (presetId.isEmpty() || presetId == "none" || presetId == "empty")
    {
        showDrumPresetMenu();
        return;
    }

    auto variants = PatternsPanel::getPatternsForPreset(presetId);
    if (variants.empty())
        return;

    juce::PopupMenu menu;
    menu.addSectionHeader(getCurrentDrumPresetLabel() + " Patterns");
    for (int i = 0; i < (int)variants.size(); ++i)
    {
        const auto& pat = variants[(size_t)i];
        juce::String label = pat.title;
        if (pat.feel.isNotEmpty())
            label += " - " + pat.feel;
        menu.addItem(i + 1, label);
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options{}
            .withTargetScreenArea(localAreaToGlobal(getDrumGenreButtonRect()))
            .withMinimumWidth(360)
            .withStandardItemHeight(30),
        [this, variants](int result)
        {
            if (result <= 0 || result > (int)variants.size())
                return;

            const auto& pattern = variants[(size_t)result - 1];
            if (onDrumPatternVariantClicked)
            {
                onDrumPatternVariantClicked(pattern);
                return;
            }

            PatternGrid grid {};
            for (size_t r = 0; r < grid.size(); ++r)
                for (size_t s = 0; s < grid[r].size(); ++s)
                    grid[r][s] = pattern.rows[r][s];

            if (pattern.useFullPresetRows)
            {
                applyDrumPreset(pattern.presetId);
                if (!pattern.id.containsIgnoreCase("_default"))
                    applyStepPatternToExistingRows(grid);
            }
            else
            {
                applyStepPattern(pattern.title, grid);
            }
        });
}

void ChannelRack::setSelectedChannelVolumeFromDrag(int startY, int currentY, float startValue)
{
    int idx = selectedChannel_;
    if (idx < 0 || idx >= (int)channels_.size())
        idx = channels_.empty() ? -1 : 0;
    if (idx < 0)
        return;

    if (getHeaderVolumeRect().isEmpty())
        return;

    const float delta = (float)(startY - currentY) / 120.0f;
    channels_[(size_t)idx].volume = juce::jlimit(0.0f, 1.0f, startValue + delta);
    repaint();
    if (onChannelDataChanged)
        onChannelDataChanged(idx);
}

void ChannelRack::applyDefaultChannelSettings(int channelIndex)
{
    if (channelIndex < 0 || channelIndex >= (int)channels_.size())
        return;

    auto& ch = channels_[(size_t)channelIndex];
    ch.volume = 1.0f;
    ch.pan = 0.0f;
    ch.muted = false;
    ch.solo = false;
    repaint();
    if (onChannelDataChanged)
        onChannelDataChanged(channelIndex);
}

void ChannelRack::showHeaderVolumeMenu()
{
    int idx = selectedChannel_;
    if (idx < 0 || idx >= (int)channels_.size())
        idx = channels_.empty() ? -1 : 0;
    if (idx < 0)
        return;

    auto knobRect = getHeaderVolumeRect();
    if (knobRect.isEmpty())
        return;

    juce::PopupMenu menu;
    menu.addItem(1, "Default setting");

    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea(localAreaToGlobal(knobRect)),
        [this, idx](int result)
        {
            if (result == 1)
                applyDefaultChannelSettings(idx);
        });
}

void ChannelRack::drawChannel(juce::Graphics& g, juce::Rectangle<int> bounds, int channelIndex)
{
    const auto& channel = channels_[channelIndex];
    bool isSelected = (channelIndex == selectedChannel_);
    bool isDropTarget = (channelIndex == dropHighlightRow_);
    bool isAlt = (channelIndex % 2 == 1);
    
    // Row background — alternating with subtle separation
    auto rowF = bounds.toFloat().withTrimmedLeft(2).withTrimmedRight(2);
    if (isSelected)
    {
        // Orange wash on selected row
        juce::ColourGradient rowG(juce::Colour(0x33f97316), rowF.getX(), rowF.getY(),
                                  juce::Colour(0x0df97316), rowF.getRight(), rowF.getY(), false);
        g.setGradientFill(rowG);
        g.fillRect(rowF);
        g.setColour(juce::Colour(0x66f97316));
        g.drawHorizontalLine(bounds.getY(), rowF.getX(), rowF.getRight());
        g.setColour(juce::Colour(0x33f97316));
        g.drawHorizontalLine(bounds.getBottom() - 1, rowF.getX(), rowF.getRight());
    }
    else if (isDropTarget)
    {
        g.setColour(juce::Colour(0x336366f1));
        g.fillRect(rowF);
    }
    else if (isAlt)
    {
        g.setColour(juce::Colour(0x40000000));
        g.fillRect(rowF);
    }
    
    // Subtle row separator
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawHorizontalLine(bounds.getBottom() - 1, rowF.getX(), rowF.getRight());
    g.setColour(juce::Colours::white.withAlpha(0.025f));
    g.drawHorizontalLine(bounds.getBottom(), rowF.getX(), rowF.getRight());
    
    int x = bounds.getX() + LEFT_PADDING;
    int cy = bounds.getCentreY();
    
    // Channel index — engraved style
    g.setColour(juce::Colour(0xff52525b));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f));
    g.drawText(getChannelStripNumber(channelIndex), x, bounds.getY(), CH_INDEX_WIDTH, bounds.getHeight(), juce::Justification::centred);
    x += CH_INDEX_WIDTH + 6;
    
    // ── LED indicator (skeuomorphic glow with bright core) ──
    {
        auto ledRect = juce::Rectangle<float>((float)x + 2, (float)cy - CH_LED_SIZE/2.0f, (float)CH_LED_SIZE, (float)CH_LED_SIZE);
        bool on = !channel.muted;
        juce::Colour led = on ? juce::Colour(0xff22c55e) : juce::Colour(0xff1a1a1e);
        
        if (on)
        {
            // Outer glow
            g.setColour(led.withAlpha(0.35f));
            g.fillEllipse(ledRect.expanded(2.5f));
            g.setColour(led.withAlpha(0.6f));
            g.fillEllipse(ledRect.expanded(1.0f));
        }
        // Body
        g.setColour(led);
        g.fillEllipse(ledRect);
        // Bright core (top-left highlight)
        if (on)
        {
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.fillEllipse(ledRect.getX() + 1.5f, ledRect.getY() + 1.0f, ledRect.getWidth() * 0.45f, ledRect.getHeight() * 0.4f);
        }
        // Dark rim
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawEllipse(ledRect, 0.6f);
    }
    x += CH_LED_SIZE + 6;
    
    // ── Mute button (recessed pill) ──
    auto drawSmallBtn = [&](juce::Rectangle<float> r, bool active, juce::Colour activeColor, const juce::String& letter) {
        if (active)
        {
            // Active: gradient fill + glow
            juce::ColourGradient ag(activeColor.brighter(0.1f), 0.0f, r.getY(),
                                    activeColor.darker(0.4f), 0.0f, r.getBottom(), false);
            g.setGradientFill(ag);
            g.fillRoundedRectangle(r, 2.5f);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawHorizontalLine((int)r.getY() + 1, r.getX() + 2, r.getRight() - 2);
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(r.reduced(0.5f), 2.5f, 0.6f);
            g.setColour(juce::Colours::white);
        }
        else
        {
            // Recessed dark well
            g.setColour(juce::Colour(0xff0a0a0c));
            g.fillRoundedRectangle(r, 2.5f);
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(r.reduced(0.5f), 2.5f, 0.6f);
            g.setColour(juce::Colour(0xff52525b));
        }
        g.setFont(juce::FontOptions().withName("Consolas").withHeight(8.0f).withStyle("Bold"));
        g.drawText(letter, r.toNearestInt(), juce::Justification::centred);
    };
    
    auto muteBtn = juce::Rectangle<float>((float)x, (float)cy - CH_MUTE_SIZE/2.0f, (float)CH_MUTE_SIZE, (float)CH_MUTE_SIZE);
    drawSmallBtn(muteBtn, channel.muted, juce::Colour(0xffef4444), "M");
    x += CH_MUTE_SIZE + 4;
    
    auto soloBtn = juce::Rectangle<float>((float)x, (float)cy - CH_MUTE_SIZE/2.0f, (float)CH_MUTE_SIZE, (float)CH_MUTE_SIZE);
    drawSmallBtn(soloBtn, channel.solo, juce::Colour(0xfff97316), "S");
    x += CH_MUTE_SIZE + 6;
    
    // ── Channel name (text-shadow style) ──
    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f).withStyle("Bold"));
    g.drawText(channel.name, x, bounds.getY() + 1, CH_NAME_WIDTH, bounds.getHeight(), juce::Justification::centredLeft);
    // Main text
    g.setColour(isSelected ? juce::Colour(0xfff5f5f5) : juce::Colour(0xffe4e4e7));
    g.drawText(channel.name, x, bounds.getY(), CH_NAME_WIDTH, bounds.getHeight(), juce::Justification::centredLeft);
    
    // ── Step buttons: deep recessed wells, orange gradient when active ──
    x = bounds.getX() + CHANNELS_START_X;
    if (isMusicLoopChannel(channel) && !channel.waveformPeaks.empty())
    {
        const int stepW = stepCellWidth();
        const int stepG = stepCellGap();
        const int beatG = beatCellGap();
        int stepAreaW = 0;
        for (int step = 0; step < totalSteps_; ++step)
        {
            if (step % 4 == 0 && step > 0)
                stepAreaW += beatG;
            stepAreaW += stepW + stepG;
        }

        auto wave = juce::Rectangle<float>((float)x, (float)bounds.getY() + 5.0f,
                                           (float)stepAreaW, (float)bounds.getHeight() - 10.0f);
        g.setColour(juce::Colour(0xff0b1220));
        g.fillRoundedRectangle(wave, 2.5f);
        g.setColour(juce::Colour(0xff2563eb).withAlpha(0.35f));
        g.drawRoundedRectangle(wave.reduced(0.5f), 2.5f, 0.8f);

        const int peakCount = (int)channel.waveformPeaks.size();
        for (int px = 0; px < (int)wave.getWidth(); ++px)
        {
            const int peakIdx = juce::jlimit(0, peakCount - 1,
                (int)((float)px / juce::jmax(1.0f, wave.getWidth()) * (float)peakCount));
            const float rawPeak = juce::jlimit(0.0f, 1.0f, channel.waveformPeaks[(size_t)peakIdx]);
            const float amp = juce::jmax(0.04f, rawPeak);
            const float barH = amp * (wave.getHeight() * 0.82f);
            const float barX = wave.getX() + (float)px;
            g.setColour(juce::Colour(0xff60a5fa).withAlpha(0.55f + amp * 0.35f));
            g.fillRect(barX, wave.getCentreY() - barH * 0.5f, 1.0f, barH);
        }

        if (isPlaying_)
        {
            const float phase = (float)(absoluteStep_ % juce::jmax(1, totalSteps_)) / (float)juce::jmax(1, totalSteps_);
            const float px = wave.getX() + phase * wave.getWidth();
            g.setColour(juce::Colour(0xff93c5fd).withAlpha(0.85f));
            g.drawVerticalLine((int)std::round(px), wave.getY(), wave.getBottom());
        }

        return;
    }

    if (isMelodicChannel(channel) && !channel.pianoRollNotes.empty())
    {
        const int stepW = stepCellWidth();
        const int stepG = stepCellGap();
        const int beatG = beatCellGap();
        int stepAreaW = 0;
        for (int step = 0; step < totalSteps_; ++step)
        {
            if (step % 4 == 0 && step > 0)
                stepAreaW += beatG;
            stepAreaW += stepW + stepG;
        }

        auto preview = juce::Rectangle<float>((float)x, (float)bounds.getY() + 5.0f,
                                              (float)stepAreaW, (float)bounds.getHeight() - 10.0f);
        g.setColour(juce::Colour(0xff050507));
        g.fillRoundedRectangle(preview, 2.5f);
        g.setColour(Theme::accent.withAlpha(0.28f));
        g.drawRoundedRectangle(preview.reduced(0.5f), 2.5f, 0.8f);

        int minPitch = 127;
        int maxPitch = 0;
        for (const auto& note : channel.pianoRollNotes)
        {
            minPitch = juce::jmin(minPitch, note.pitch);
            maxPitch = juce::jmax(maxPitch, note.pitch);
        }
        if (minPitch > maxPitch)
        {
            minPitch = 48;
            maxPitch = 72;
        }

        const int patternLength = getChannelPatternLength(channel);
        const float stepPx = preview.getWidth() / (float)juce::jmax(1, patternLength);
        const int pitchRange = juce::jmax(1, maxPitch - minPitch);
        for (const auto& note : channel.pianoRollNotes)
        {
            const float nx = preview.getX() + (float)note.startStep * stepPx;
            const float nw = juce::jmax(3.0f, (float)juce::jmax(1, note.lengthSteps) * stepPx - 1.0f);
            const float norm = (float)(note.pitch - minPitch) / (float)pitchRange;
            const float ny = preview.getBottom() - 4.0f - norm * (preview.getHeight() - 8.0f);
            auto nr = juce::Rectangle<float>(nx, ny - 1.5f, nw, 3.0f).getIntersection(preview);
            g.setColour(Theme::accentBright.withAlpha(0.92f));
            g.fillRoundedRectangle(nr, 1.2f);
        }

        if (isPlaying_)
        {
            const int patternLength = getChannelPatternLength(channel);
            const float phase = (float)(absoluteStep_ % patternLength) / (float)juce::jmax(1, patternLength);
            const float px = preview.getX() + phase * preview.getWidth();
            g.setColour(Theme::accentBright.withAlpha(0.7f));
            g.drawVerticalLine((int)std::round(px), preview.getY(), preview.getBottom());
        }

        return;
    }

    const int stepW = stepCellWidth();
    const int stepG = stepCellGap();
    const int beatG = beatCellGap();
    for (int step = 0; step < totalSteps_; ++step)
    {
        if (step % 4 == 0 && step > 0)
            x += beatG;

        auto stepRect = juce::Rectangle<float>((float)x, (float)cy - STEP_HEIGHT/2.0f, (float)stepW, (float)STEP_HEIGHT);
        bool isActive = (step < (int)channel.steps.size() && channel.steps[step]);
        bool isCurrentPlay = (step == currentStep_ && isPlaying_);
        bool isBeatStart = (step % 4 == 0);
        
        if (isActive)
        {
            // Active: orange gradient with top highlight + glow
            // Outer glow
            g.setColour(juce::Colour(0x44f97316));
            g.fillRoundedRectangle(stepRect.expanded(1.0f), 3.0f);
            
            juce::ColourGradient sg(juce::Colour(0xfffb923c), 0.0f, stepRect.getY(),
                                    juce::Colour(0xffc2410c), 0.0f, stepRect.getBottom(), false);
            g.setGradientFill(sg);
            g.fillRoundedRectangle(stepRect, 2.5f);
            
            // Top highlight
            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.drawHorizontalLine((int)stepRect.getY() + 1, stepRect.getX() + 3, stepRect.getRight() - 3);
            
            // Border
            g.setColour(juce::Colour(0xff431407));
            g.drawRoundedRectangle(stepRect.reduced(0.5f), 2.5f, 0.8f);
            
            // Playhead pulse on active step
            if (isCurrentPlay)
            {
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.fillRoundedRectangle(stepRect.reduced(2.0f), 1.5f);
            }
        }
        else
        {
            // Inactive: deep recessed well
            juce::Colour wellTop    = isBeatStart ? juce::Colour(0xff1a1a1e) : juce::Colour(0xff141417);
            juce::Colour wellBottom = isBeatStart ? juce::Colour(0xff0a0a0c) : juce::Colour(0xff050507);
            
            juce::ColourGradient wg(wellTop, 0.0f, stepRect.getY(),
                                    wellBottom, 0.0f, stepRect.getBottom(), false);
            g.setGradientFill(wg);
            g.fillRoundedRectangle(stepRect, 2.5f);
            
            // Top inner shadow
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.drawHorizontalLine((int)stepRect.getY() + 1, stepRect.getX() + 2, stepRect.getRight() - 2);
            
            // Border
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(stepRect.reduced(0.5f), 2.5f, 0.6f);
            
            // Bottom highlight (etched edge)
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.drawHorizontalLine((int)stepRect.getBottom() - 1, stepRect.getX() + 2, stepRect.getRight() - 2);
            
            // Playhead overlay (no fill, just tint)
            if (isCurrentPlay)
            {
                g.setColour(juce::Colour(0x33f97316));
                g.fillRoundedRectangle(stepRect, 2.5f);
            }
        }
        
        x += stepW + stepG;
    }

    // ── MIDI "R" button — appears on ALL drum rows ──
    auto midiRect = getMidiButtonRect(channelIndex);
    if (!midiRect.isEmpty())
    {
        auto rf = midiRect.toFloat();
        bool hasSteps = false;
        for (bool s : channel.steps) if (s) { hasSteps = true; break; }

        if (hasSteps)
        {
            // Filled orange when pattern has active steps
            juce::ColourGradient cg(juce::Colour(0xfff97316), rf.getX(), rf.getY(),
                                    juce::Colour(0xff9a3412), rf.getX(), rf.getBottom(), false);
            g.setGradientFill(cg);
            g.fillEllipse(rf);
            // Glow
            g.setColour(juce::Colour(0xfff97316).withAlpha(0.4f));
            g.fillEllipse(rf.expanded(2.0f));
            g.setColour(juce::Colours::white);
        }
        else
        {
            // Empty: dark recessed circle
            juce::ColourGradient cg(juce::Colour(0xff27272a), rf.getX(), rf.getY(),
                                    juce::Colour(0xff0a0a0c), rf.getX(), rf.getBottom(), false);
            g.setGradientFill(cg);
            g.fillEllipse(rf);
            g.setColour(juce::Colour(0xfff97316).withAlpha(0.4f));
        }
        g.drawEllipse(rf.reduced(0.5f), 1.0f);
        g.setFont(juce::FontOptions().withName("Consolas").withHeight(10.0f).withStyle("Bold"));
        g.drawText("R", midiRect, juce::Justification::centred);
    }
}

// ─────────────────────────────────────────────────────────────────────
// Drum preset library
//  Each preset is a list of (channel-name, 16-step-pattern) rows.
//  applyDrumPreset() will match existing channels by any of the row's
//  aliases (case-insensitive). Missing channels are APPENDED so the user
//  can drop samples onto them.
// ─────────────────────────────────────────────────────────────────────
namespace {
    using IT = ChannelRack::InstrumentType;

    struct PresetRow {
        const char* name;                       // canonical label, e.g. "Open Hat"
        const char* const* aliases;             // null-terminated list
        IT   type;
        int  steps[16];
    };

    struct DrumPreset {
        const char* id;
        const char* label;
        const PresetRow* rows;   // terminated by a row with nullptr name
        const char* sampleFolder; // absolute path to a drum-kit folder (may be "" = none)
    };

    // Alias tables (null-terminated).
    static const char* const AL_KICK[]  = { "Kick", "BD", "Bass Drum", nullptr };
    static const char* const AL_SNARE[] = { "Snare", "SD", nullptr };
    static const char* const AL_HIHAT[] = { "Hihat", "Hi-Hat", "HiHat", "HH", "Closed Hat", nullptr };
    static const char* const AL_CLAP[]  = { "Clap", "CP", nullptr };
    static const char* const AL_OHAT[]  = { "Open Hat", "OpenHat", "OHat", "OH", nullptr };
    static const char* const AL_RIDE[]  = { "Ride", "Ride Cymbal", nullptr };
    static const char* const AL_CRASH[] = { "Crash", "Crash Cymbal", nullptr };
    static const char* const AL_RIM[]   = { "Rim", "Rimshot", "Rim Shot", "Sidestick", nullptr };
    static const char* const AL_PERC[]  = { "Perc", "Percussion", nullptr };
    static const char* const AL_SHAKE[] = { "Shaker", "Shake", nullptr };
    static const char* const AL_TOM[]   = { "Tom", "Tom1", "Floor Tom", nullptr };
    static const char* const AL_808[]   = { "808", "Sub", "Bass 808", nullptr };
    static const char* const AL_VINYL[] = { "Vinyl", "Crackle", "Noise", nullptr };

    // Row tables (each terminated with a name==nullptr sentinel).
    static const PresetRow ROWS_BOOMBAP[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,1,0, 1,0,0,0, 0,1,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,0,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0} },
        { "Ride",     AL_RIDE,  IT::Hihat, {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0} },
        { "Rim",      AL_RIM,   IT::Clap,  {0,0,1,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_HIPHOP[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,1,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,0,0,1, 0,0,0,0, 0,0,0,1, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_TRAP[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0} },
        { "808",      AL_808,   IT::Bass,  {1,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_DRILL[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,1,0, 0,1,0,0, 0,0,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,1, 0,0,0,0, 1,0,1,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,0} },
        { "808",      AL_808,   IT::Bass,  {1,0,0,0, 0,0,1,0, 0,1,0,0, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_HOUSE[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0} },
        { "Shaker",   AL_SHAKE, IT::Hihat, {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,0,0,1, 0,0,1,0, 0,0,0,1, 0,0,1,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_RNB[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,1, 0,0,0,0, 1,0,0,0, 0,0,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,1} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,1,0, 1,0,1,0, 1,0,1,1, 1,0,1,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Rim",      AL_RIM,   IT::Clap,  {0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,0,1,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_LOFI[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,0,1, 0,0,1,0, 0,0,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Rim",      AL_RIM,   IT::Clap,  {0,0,1,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { "Vinyl",    AL_VINYL, IT::Hihat, {1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_ROCK[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,0,0, 1,0,1,0, 0,0,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0} },
        { "Crash",    AL_CRASH, IT::Hihat, {1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0} },
        { "Ride",     AL_RIDE,  IT::Hihat, {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0} },
        { "Tom",      AL_TOM,   IT::Kick,  {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,1,1} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    // Detroit / Flint Michigan style:
    //  - Sparse, syncopated kick (off the grid)
    //  - Lazy backbeat snare with subtle ghost
    //  - Hats use a swung dotted-eighth feel with rests (very Flint signature)
    //  - 808 sub follows the kick with a slide accent
    //  - Clap doubles the snare for that layered slap
    static const PresetRow ROWS_DETROIT[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,1,0, 0,0,1,0, 0,1,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,1, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,0,1, 0,1,0,0, 1,0,0,1, 0,1,0,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0} },
        { "808",      AL_808,   IT::Bass,  {1,0,0,0, 0,0,1,0, 0,0,1,0, 0,1,0,0} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,0,1,0, 0,0,0,1, 0,0,0,0, 0,0,1,0} },
        { "Rim",      AL_RIM,   IT::Clap,  {0,0,0,0, 0,0,0,0, 0,1,0,0, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_AFROBEAT[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,1,0, 1,0,0,0, 0,1,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,1,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { "Shaker",   AL_SHAKE, IT::Hihat, {0,1,0,1, 0,1,0,1, 0,1,0,1, 0,1,0,1} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,0,0,1, 0,0,1,0, 0,0,0,1, 0,0,1,0} },
        { "Rim",      AL_RIM,   IT::Clap,  {0,0,1,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_REGGAETON[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,1,0, 1,0,0,0, 0,0,1,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,1, 0,0,0,0, 0,0,0,1, 0,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,1,0,0, 0,0,0,1, 0,1,0,0, 0,0,0,1} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_JERSEY[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,1, 0,1,0,0, 1,0,0,1, 0,1,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,1,0,1, 1,0,1,1, 1,1,0,1, 1,0,1,1} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,1,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,1,0,0, 0,0,1,0, 0,1,0,0, 0,0,1,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_UKG[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,1,0, 0,0,1,0, 0,0,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {0,1,1,0, 0,1,1,0, 0,1,1,0, 0,1,1,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,1,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,0,0,1, 0,0,0,0, 0,1,0,0, 0,0,0,1} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_DNB[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0} },
        { "Ride",     AL_RIDE,  IT::Hihat, {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0} },
        { "Bass 808", AL_808,   IT::Bass,  {1,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_TECHNO[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,0,0,1, 0,1,0,0, 0,0,0,1, 0,1,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_PHONK[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,0,1,0, 0,0,1,0, 0,1,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,1,1, 1,0,1,1, 1,0,1,1, 1,0,1,1} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0} },
        { "808",      AL_808,   IT::Bass,  {1,0,0,0, 0,0,1,0, 0,0,1,0, 0,1,0,0} },
        { "Rim",      AL_RIM,   IT::Clap,  {0,0,1,0, 0,0,0,0, 0,0,1,0, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_MEMPHIS[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,0, 0,1,0,0, 1,0,0,0, 0,1,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0} },
        { "808",      AL_808,   IT::Bass,  {1,0,0,0, 0,1,0,0, 1,0,0,0, 0,1,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_FUNK[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {1,0,0,1, 0,0,1,0, 1,0,0,1, 0,0,1,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 1,0,0,0, 0,0,1,0, 1,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {1,0,1,0, 1,1,1,0, 1,0,1,0, 1,1,1,0} },
        { "Open Hat", AL_OHAT,  IT::Hihat, {0,0,0,0, 0,0,1,0, 0,0,0,0, 0,0,1,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0} },
        { "Perc",     AL_PERC,  IT::Clap,  {0,1,0,0, 0,0,0,1, 0,1,0,0, 0,0,0,1} },
        { nullptr, nullptr, IT::Kick, {} },
    };
    static const PresetRow ROWS_EMPTY[] = {
        { "Kick",     AL_KICK,  IT::Kick,  {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0} },
        { "Snare",    AL_SNARE, IT::Snare, {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0} },
        { "Hihat",    AL_HIHAT, IT::Hihat, {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0} },
        { "Clap",     AL_CLAP,  IT::Clap,  {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0} },
        { nullptr, nullptr, IT::Kick, {} },
    };

    // Per-preset root folder to auto-pull samples from. Keep empty ("") for
    // presets with no configured folder — applyDrumPreset() will just update
    // steps/BPM without touching the channel's sampleFile.
    // Paths point to folders inside the user's genre-organized collection at
    // E:\!Storage\1500 THE DRUMS LORD COLLECTION. scanAudioRecursive() is used
    // so any path here is searched recursively for matching one-shot samples.
    static const char* const BOOM_BAP_FOLDER =
        R"(E:\!Storage\1500 THE DRUMS LORD COLLECTION\_Mixed General\! Maxeyy Stash V5 Drum Kit\Boom Bap Stash V2)";
    static const char* const TRAP_FOLDER =
        R"(E:\!Storage\1500 THE DRUMS LORD COLLECTION\_Mixed General\! Maxeyy - Stash V3 Drum Kit)";
    static const char* const DETROIT_FOLDER =
        R"(E:\!Storage\1500 THE DRUMS LORD COLLECTION\_Detroit Beat\!FLINT DETROIT\What Up Sav Vol 1(Flint & Detroit Drum Kit))";
    static const char* const AFROBEAT_FOLDER =
        R"(E:\!Storage\1500 THE DRUMS LORD COLLECTION\_Afrobeat)";
    static const char* const RNB_FOLDER =
        R"(E:\!Storage\1500 THE DRUMS LORD COLLECTION\_RnB)";
    static const char* const MEMPHIS_PHONK_FOLDER =
        R"(E:\!Storage\1500 THE DRUMS LORD COLLECTION\_Memphis Phonk)";

    static const DrumPreset kPresets[] = {
        { "boom_bap", "Boom Bap", ROWS_BOOMBAP, BOOM_BAP_FOLDER },
        { "hiphop",   "Hip Hop",  ROWS_HIPHOP,  "" },
        { "trap",     "Trap",     ROWS_TRAP,    TRAP_FOLDER },
        { "drill",    "Drill",    ROWS_DRILL,   "" },
        { "house",    "House",    ROWS_HOUSE,   "" },
        { "rnb",      "R&B",      ROWS_RNB,     RNB_FOLDER },
        { "lofi",     "Lo-Fi",    ROWS_LOFI,    "" },
        { "rock",     "Rock",          ROWS_ROCK,    "" },
        { "detroit",  "Detroit Flint", ROWS_DETROIT, DETROIT_FOLDER },
        { "afrobeat", "Afrobeat",      ROWS_AFROBEAT, AFROBEAT_FOLDER },
        { "reggaeton","Reggaeton",     ROWS_REGGAETON, "" },
        { "jersey",   "Jersey Club",   ROWS_JERSEY, "" },
        { "ukg",      "UK Garage",     ROWS_UKG, "" },
        { "dnb",      "Drum & Bass",   ROWS_DNB, "" },
        { "techno",   "Techno",        ROWS_TECHNO, "" },
        { "phonk",    "Phonk",         ROWS_PHONK, MEMPHIS_PHONK_FOLDER },
        { "memphis",  "Memphis",       ROWS_MEMPHIS, MEMPHIS_PHONK_FOLDER },
        { "funk",     "Funk",          ROWS_FUNK, "" },
        { "empty",    "Clear All",     ROWS_EMPTY,   "" },
    };

    const DrumPreset* findPreset(const juce::String& id)
    {
        for (auto& p : kPresets)
            if (id.equalsIgnoreCase(p.id) || id.equalsIgnoreCase(p.label))
                return &p;
        return nullptr;
    }

    // Find first channel whose name matches any alias (case-insensitive). -1 if not found.
    int findChannelByAliases(const std::vector<ChannelRack::Channel>& channels,
                             const char* const* aliases)
    {
        if (!aliases) return -1;
        for (size_t i = 0; i < channels.size(); ++i)
            for (const char* const* a = aliases; *a; ++a)
                if (channels[i].name.containsIgnoreCase(*a)) return (int)i;
        return -1;
    }

    void writeSteps(ChannelRack::Channel& ch, const int (&pattern)[16], int totalSteps)
    {
        totalSteps = juce::jlimit(16, 32, totalSteps);
        ch.steps.assign((size_t)totalSteps, false);
        for (int i = 0; i < totalSteps; ++i) ch.steps[(size_t)i] = (pattern[i % 16] != 0);

        // Keep pianoRollNotes in sync with steps so the Piano Roll view shows
        // the pattern. We only own notes at DEFAULT_DRUM_PITCH; melodic notes
        // the user drew on other pitches are preserved.
        const int drumPitch = ChannelRack::DEFAULT_DRUM_PITCH;
        auto& notes = ch.pianoRollNotes;
        notes.erase(std::remove_if(notes.begin(), notes.end(),
            [drumPitch](const ChannelRack::Channel::Note& n) { return n.pitch == drumPitch; }),
            notes.end());
        for (int i = 0; i < totalSteps; ++i)
            if (ch.steps[(size_t)i])
                notes.push_back({ drumPitch, i, 1, 100 });
    }

    // ── Audio-file scanner (cached per folder) ──────────────────────
    bool isAudio(const juce::File& f)
    {
        auto ext = f.getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".mp3" || ext == ".aiff" || ext == ".aif"
            || ext == ".flac" || ext == ".ogg";
    }

    const juce::Array<juce::File>& scanAudioRecursive(const juce::File& root)
    {
        static std::map<juce::String, juce::Array<juce::File>> cache;
        auto key = root.getFullPathName();
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;

        juce::Array<juce::File> files;
        if (root.isDirectory())
        {
            auto all = root.findChildFiles(juce::File::findFiles, true, "*");
            for (auto& f : all) if (isAudio(f)) files.add(f);
        }
        cache[key] = std::move(files);
        return cache[key];
    }

    // True if file name matches any alias AND does not collide with an
    // "opener" keyword that would steal Open Hat matches for plain Hihat rows.
    bool fileMatchesRow(const juce::File& f, const PresetRow& row)
    {
        auto name = f.getFileNameWithoutExtension();
        bool any = false;
        for (const char* const* a = row.aliases; *a; ++a)
            if (name.containsIgnoreCase(*a)) { any = true; break; }
        if (!any) return false;

        // Keep Open Hat and Hihat from colliding.
        bool looksOpen = name.containsIgnoreCase("open");
        bool rowIsOpen = false;
        for (const char* const* a = row.aliases; *a; ++a)
            if (juce::String(*a).containsIgnoreCase("open")) { rowIsOpen = true; break; }
        if (rowIsOpen && !looksOpen) return false;
        if (!rowIsOpen && looksOpen) return false;
        return true;
    }

    juce::File pickSampleForRow(const juce::Array<juce::File>& pool,
                                 const PresetRow& row,
                                 const juce::File& previous,
                                 juce::Random& rng)
    {
        juce::Array<juce::File> candidates;
        for (auto& f : pool) if (fileMatchesRow(f, row)) candidates.add(f);
        if (candidates.isEmpty()) return {};

        // Try to avoid repeating the previous pick.
        if (previous.existsAsFile() && candidates.size() > 1)
        {
            juce::Array<juce::File> filtered;
            for (auto& f : candidates)
                if (f.getFullPathName() != previous.getFullPathName()) filtered.add(f);
            if (!filtered.isEmpty()) return filtered[rng.nextInt(filtered.size())];
        }
        return candidates[rng.nextInt(candidates.size())];
    }
}

int ChannelRack::libraryPatternRowForChannel(const Channel& channel)
{
    const auto n = channel.name.toLowerCase();
    if (channel.type == InstrumentType::Kick)
        return 0;
    if (channel.type == InstrumentType::Snare)
        return 1;
    if (n.contains("open") || n.contains("ride") || n.contains("crash"))
        return 3;
    if (channel.type == InstrumentType::Clap || n.contains("clap") || n.contains("rim"))
        return 3;
    if (channel.type == InstrumentType::Hihat || n.contains("hat") || n.contains("cym"))
        return 2;
    return 2;
}

void ChannelRack::showMidiPatternMenu(int channelIndex)
{
    if (channelIndex < 0 || channelIndex >= (int)channels_.size())
        return;

    const auto& ch = channels_[(size_t)channelIndex];
    const auto genreId = currentDrumPresetId_.toLowerCase();
    const bool hasGenre = !genreId.isEmpty()
                       && genreId != "none"
                       && genreId != "empty";

    std::vector<MidiMenuRow> rows;
    auto addRow = [&rows](int id, const juce::String& text, bool enabled = true)
    {
        rows.push_back({ id, text, false, enabled });
    };
    auto addHeader = [&rows](const juce::String& text)
    {
        rows.push_back({ 0, text, true, false });
    };

    {
        int activeCount = 0;
        for (bool s : ch.steps) if (s) activeCount++;
        addRow(0, "Current: " + juce::String(activeCount) + "/"
            + juce::String((int)ch.steps.size()) + " steps", false);
    }

    if (!hasGenre)
        addRow(0, "Select a genre first to save dropped MIDI", false);

    addRow(90001, "Randomize Pattern");
    addRow(90002, "Clear All Steps");
    if (isHiHatChannel(channelIndex))
        addRow(90003, "Reroll Hi-Hat Variant");
    addRow(90010, "Load MIDI File (.mid)...");

    const char* const* channelAliases = nullptr;
    switch (ch.type)
    {
        case InstrumentType::Kick:  channelAliases = AL_KICK; break;
        case InstrumentType::Snare: channelAliases = AL_SNARE; break;
        case InstrumentType::Hihat: channelAliases = AL_HIHAT; break;
        case InstrumentType::Clap:  channelAliases = AL_CLAP; break;
        default: break;
    }

    auto matchesChannel = [&](const PresetRow& row) -> bool
    {
        if (channelAliases)
        {
            for (const char* const* a = row.aliases; a && *a; ++a)
                for (const char* const* ca = channelAliases; *ca; ++ca)
                    if (juce::String(*a).equalsIgnoreCase(*ca))
                        return true;
        }
        for (const char* const* a = row.aliases; a && *a; ++a)
            if (ch.name.containsIgnoreCase(*a))
                return true;
        return false;
    };

    std::vector<const PresetRow*> kitRows;
    std::vector<PatternsPanel::PatternDefinition> libraryPatterns;
    std::vector<SavedMidiPattern> savedPatterns;
    const int laneRow = libraryPatternRowForChannel(ch);
    const juce::String laneId = laneRow == 0 ? "kick" : (laneRow == 1 ? "snare" : (laneRow == 2 ? "hihat" : "perc"));

    if (hasGenre)
    {
        if (const DrumPreset* preset = findPreset(genreId))
        {
            addHeader(getCurrentDrumPresetLabel() + " lane patterns");
            for (const PresetRow* row = preset->rows; row->name != nullptr; ++row)
            {
                if (!matchesChannel(*row))
                    continue;
                kitRows.push_back(row);
                addRow(11000 + (int)kitRows.size() - 1, juce::String(row->name));
            }
        }

        for (const auto& saved : loadSavedMidiPatterns())
            if (saved.genreId == genreId && saved.laneId == laneId)
                savedPatterns.push_back(saved);

        if (!savedPatterns.empty())
        {
            addHeader("Dropped " + getCurrentDrumPresetLabel() + " " + laneId.toUpperCase() + " MIDI");
            for (int i = 0; i < (int)savedPatterns.size(); ++i)
                addRow(13000 + i, savedPatterns[(size_t)i].name);
        }

        libraryPatterns = PatternsPanel::getPatternsForPreset(genreId);
        if (!libraryPatterns.empty())
        {
            addHeader("Saved " + getCurrentDrumPresetLabel() + " patterns");
            for (int i = 0; i < (int)libraryPatterns.size(); ++i)
            {
                const auto& pat = libraryPatterns[(size_t)i];
                juce::String label = pat.title;
                if (pat.feel.isNotEmpty())
                    label += " - " + pat.feel;
                addRow(12000 + i, label);
            }
        }
    }

    auto midiRect = getMidiButtonRect(channelIndex);
    const int ci = channelIndex;

    auto applyStepsToChannel = [this, ci](const std::vector<int>& sourceSteps)
    {
        if (ci < 0 || ci >= (int)channels_.size() || sourceSteps.empty())
            return;

        auto& c = channels_[(size_t)ci];
        const int dp = DEFAULT_DRUM_PITCH;
        const int length = juce::jmax(totalSteps_, (int)sourceSteps.size());
        c.steps.assign((size_t)length, false);
        c.pianoRollNotes.erase(std::remove_if(c.pianoRollNotes.begin(), c.pianoRollNotes.end(),
            [dp](const Channel::Note& n) { return n.pitch == dp; }), c.pianoRollNotes.end());

        for (int s = 0; s < length; ++s)
        {
            const bool active = sourceSteps[(size_t)(s % (int)sourceSteps.size())] != 0;
            c.steps[(size_t)s] = active;
            if (active)
                c.pianoRollNotes.push_back({ dp, s, 1, 100 });
        }

        repaint();
        if (onChannelDataChanged) onChannelDataChanged(ci);
    };

    auto importMidiFile = [this, genreId, laneId, hasGenre, applyStepsToChannel](const juce::File& file, bool saveToLibrary)
    {
        if (!file.existsAsFile())
            return;

        juce::FileInputStream stream(file);
        if (!stream.openedOk())
            return;

        juce::MidiFile midiFile;
        if (!midiFile.readFrom(stream))
            return;

        const int ppq = midiFile.getTimeFormat();
        const double ticksPerStep = (ppq > 0) ? ((double)ppq / 4.0) : 1.0;
        std::vector<int> imported((size_t)juce::jmax(16, totalSteps_), 0);

        for (int track = 0; track < midiFile.getNumTracks(); ++track)
        {
            const auto* mt = midiFile.getTrack(track);
            if (!mt) continue;
            for (int ev = 0; ev < mt->getNumEvents(); ++ev)
            {
                const auto& msg = mt->getEventPointer(ev)->message;
                if (!msg.isNoteOn())
                    continue;

                const int step = juce::jmax(0, (int)std::round(msg.getTimeStamp() / ticksPerStep));
                if (step >= 256)
                    continue;
                if (step >= (int)imported.size())
                    imported.resize((size_t)step + 1, 0);
                imported[(size_t)step] = 1;
            }
        }

        applyStepsToChannel(imported);

        if (saveToLibrary && hasGenre)
        {
            SavedMidiPattern saved;
            saved.genreId = genreId;
            saved.laneId = laneId;
            saved.name = file.getFileNameWithoutExtension();
            saved.steps = imported;
            upsertSavedMidiPattern(std::move(saved));
        }
    };

    auto chooseRow = [this, ci, kitRows, libraryPatterns, savedPatterns, applyStepsToChannel, importMidiFile](int result)
    {
        if (result <= 0)
            return;

        if (result == 90001)
        {
            juce::Random rng;
            auto& c = channels_[(size_t)ci];
            const int dp = DEFAULT_DRUM_PITCH;
            c.pianoRollNotes.erase(std::remove_if(c.pianoRollNotes.begin(), c.pianoRollNotes.end(),
                [dp](const Channel::Note& n) { return n.pitch == dp; }), c.pianoRollNotes.end());
            for (int s = 0; s < (int)c.steps.size(); ++s)
            {
                c.steps[(size_t)s] = rng.nextFloat() > 0.55f;
                if (c.steps[(size_t)s])
                    c.pianoRollNotes.push_back({ dp, s, 1, 100 });
            }
            repaint();
            if (onChannelDataChanged) onChannelDataChanged(ci);
            return;
        }
        if (result == 90002)
        {
            auto& c = channels_[(size_t)ci];
            const int dp = DEFAULT_DRUM_PITCH;
            std::fill(c.steps.begin(), c.steps.end(), false);
            c.pianoRollNotes.erase(std::remove_if(c.pianoRollNotes.begin(), c.pianoRollNotes.end(),
                [dp](const Channel::Note& n) { return n.pitch == dp; }), c.pianoRollNotes.end());
            repaint();
            if (onChannelDataChanged) onChannelDataChanged(ci);
            return;
        }
        if (result == 90003)
        {
            rerollHiHatPattern();
            return;
        }
        if (result == 90010)
        {
            auto chooser = std::make_shared<juce::FileChooser>(
                "Load MIDI Pattern",
                juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
                "*.mid;*.midi");
            chooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [importMidiFile, chooser](const juce::FileChooser& fc)
                {
                    importMidiFile(fc.getResult(), true);
                });
            return;
        }

        if (result >= 11000 && result < 12000)
        {
            const int rowIdx = result - 11000;
            if (rowIdx >= 0 && rowIdx < (int)kitRows.size() && kitRows[(size_t)rowIdx] != nullptr)
            {
                auto& c = channels_[(size_t)ci];
                writeSteps(c, kitRows[(size_t)rowIdx]->steps, totalSteps_);
                repaint();
                if (onChannelDataChanged) onChannelDataChanged(ci);
            }
            return;
        }

        if (result >= 13000 && result < 14000)
        {
            const int savedIdx = result - 13000;
            if (savedIdx >= 0 && savedIdx < (int)savedPatterns.size())
                applyStepsToChannel(savedPatterns[(size_t)savedIdx].steps);
            return;
        }

        if (result >= 12000)
        {
            const int patIdx = result - 12000;
            if (patIdx >= 0 && patIdx < (int)libraryPatterns.size())
            {
                const auto& pat = libraryPatterns[(size_t)patIdx];
                const int libRow = libraryPatternRowForChannel(channels_[(size_t)ci]);
                std::vector<int> steps(16, 0);
                for (int s = 0; s < 16; ++s)
                    steps[(size_t)s] = pat.rows[(size_t)libRow][(size_t)s];
                applyStepsToChannel(steps);
            }
        }
    };

    auto panel = std::make_unique<MidiPatternCallout>(
        hasGenre ? (getCurrentDrumPresetLabel() + " | " + ch.name) : ("MIDI Patterns | " + ch.name),
        "Drop a .mid file to save it to this genre and lane.",
        rows,
        [importMidiFile](const juce::File& file)
        {
            importMidiFile(file, true);
        },
        chooseRow);

    juce::CallOutBox::launchAsynchronously(std::move(panel), localAreaToGlobal(midiRect), nullptr);
}

bool ChannelRack::applyDrumPreset(const juce::String& presetId, juce::StringArray* outMissing)
{
    const DrumPreset* p = findPreset(presetId);
    if (!p) return false;
    currentDrumPresetId_ = p->id;
    hiHatVariantCounter_ = 0;

    // Resolve the active drum-kit folders based on the user's Drum Path
    // configuration (mode + multi-folder list). Falls back to the hardcoded
    // sampleFolder if no runtime config exists for this preset.
    juce::Random rng;  // seeded from time by default
    juce::Array<juce::File> pool;
    {
        DrumPathConfig cfg = getDrumPathConfig(juce::String::fromUTF8(p->id));
        juce::StringArray activeFolders;
        if (cfg.folders.isEmpty())
        {
            juce::String fallback = juce::String::fromUTF8(p->sampleFolder);
            if (fallback.isNotEmpty()) activeFolders.add(fallback);
        }
        else
        {
            switch (cfg.mode)
            {
                case DrumPathMode::All:
                    activeFolders = cfg.folders;
                    break;
                case DrumPathMode::Randomize: {
                    int idx = rng.nextInt(cfg.folders.size());
                    activeFolders.add(cfg.folders[idx]);
                    break;
                }
                case DrumPathMode::Specific: {
                    int idx = juce::jlimit(0, cfg.folders.size() - 1, cfg.specificIndex);
                    activeFolders.add(cfg.folders[idx]);
                    break;
                }
            }
        }
        for (auto& fpath : activeFolders)
        {
            juce::File f(fpath);
            if (!f.isDirectory()) continue;
            const auto& sub = scanAudioRecursive(f);
            for (auto& file : sub) pool.add(file);
        }
    }
    const bool haveFolder = !pool.isEmpty();

    // Replace the channel list with fresh rows from the preset. Without this,
    // switching presets keeps accumulating rows because the previous preset
    // renamed the channels to sample filenames so alias matching fails.
    std::vector<Channel> preservedLoops;
    for (const auto& ch : channels_)
    {
        if (isMusicLoopChannel(ch))
            preservedLoops.push_back(ch);
    }

    channels_.clear();
    selectedChannel_ = -1;
    channels_.insert(channels_.end(), preservedLoops.begin(), preservedLoops.end());

    std::vector<int> touched;
    for (const PresetRow* row = p->rows; row->name != nullptr; ++row)
    {
        // Always create a fresh row for the preset (no alias-matching).
        Channel ch;
        ch.name  = row->name;
        ch.type  = row->type;
        ch.steps = std::vector<bool>(totalSteps_, false);
        channels_.push_back(std::move(ch));
        int idx = (int)channels_.size() - 1;
        writeSteps(channels_[idx], row->steps, totalSteps_);
        touched.push_back(idx);

        // ── Sample auto-assign ────────────────────────────────────
        if (haveFolder)
        {
            juce::File picked = pickSampleForRow(pool, *row, channels_[idx].sampleFile, rng);
            if (picked.existsAsFile())
            {
                channels_[idx].sampleFile = picked;
                channels_[idx].name = picked.getFileNameWithoutExtension();
            }
            else if (outMissing)
            {
                outMissing->add(row->name);
            }
        }
    }

    // Auto-expand panel height so every row is visible.
    int bottomPad = 22;  // space under the last row (matches paint())
    int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
    if (getHeight() < ideal) setSize(getWidth(), ideal);
    fitWidthToStepCount();

    repaint();
    if (onChannelDataChanged)
        for (int idx : touched) onChannelDataChanged(idx);
    if (onChannelsChanged) onChannelsChanged();
    return true;
}

bool ChannelRack::rerollDrumSamples(const juce::String& presetId, juce::StringArray* outMissing)
{
    const DrumPreset* p = findPreset(presetId);
    if (!p) return false;

    const juce::File folder(juce::String::fromUTF8(p->sampleFolder));
    if (!folder.isDirectory())
        return false;

    const auto& pool = scanAudioRecursive(folder);
    if (pool.isEmpty())
        return false;

    if (channels_.empty())
        return applyDrumPreset(presetId, outMissing);

    juce::Random rng;
    const int loopOffset = getMusicLoopChannelCount();
    int rowIndex = 0;
    for (const PresetRow* row = p->rows; row->name != nullptr; ++row, ++rowIndex)
    {
        const int channelIndex = loopOffset + rowIndex;
        if (channelIndex >= (int)channels_.size())
            break;

        auto& ch = channels_[(size_t)channelIndex];
        if (isMusicLoopChannel(ch))
            continue;

        juce::File picked = pickSampleForRow(pool, *row, ch.sampleFile, rng);
        if (picked.existsAsFile())
        {
            ch.sampleFile = picked;
            ch.name = picked.getFileNameWithoutExtension();
        }
        else if (outMissing)
        {
            outMissing->add(row->name);
        }
    }

    repaint();
    if (onChannelDataChanged)
        for (int i = 0; i < (int)channels_.size(); ++i)
            onChannelDataChanged(i);
    if (onChannelsChanged) onChannelsChanged();
    return true;
}

bool ChannelRack::rerollHiHatSample(juce::StringArray* outMissing)
{
    int rowIndex = -1;
    for (int i = 0; i < (int)channels_.size(); ++i)
    {
        if (isHiHatChannel(i))
        {
            rowIndex = i;
            break;
        }
    }
    return rerollChannelSample(rowIndex, outMissing);
}

bool ChannelRack::rerollChannelSample(int channelIndex, juce::StringArray* outMissing)
{
    if (channelIndex < 0 || channelIndex >= (int)channels_.size())
        return false;

    const DrumPreset* p = findPreset(currentDrumPresetId_);
    if (!p) return false;

    const juce::File folder(juce::String::fromUTF8(p->sampleFolder));
    if (!folder.isDirectory())
        return false;

    const auto& pool = scanAudioRecursive(folder);
    if (pool.isEmpty())
        return false;

    const auto& target = channels_[(size_t)channelIndex];
    const PresetRow* targetRow = nullptr;
    int rowIndex = 0;
    for (const PresetRow* row = p->rows; row->name != nullptr; ++row)
    {
        if (rowIndex == channelIndex)
        {
            targetRow = row;
            break;
        }

        const juce::String rowName(row->name);
        const bool exactName = target.name.containsIgnoreCase(rowName) || rowName.containsIgnoreCase(target.name);
        const bool sameType = row->type == target.type;
        if (sameType && exactName)
        {
            targetRow = row;
            break;
        }
        ++rowIndex;
    }
    if (!targetRow)
    {
        for (const PresetRow* row = p->rows; row->name != nullptr; ++row)
        {
            if (row->type == target.type)
            {
                targetRow = row;
                break;
            }
        }
    }
    if (!targetRow)
        return false;

    juce::Random rng;
    auto& ch = channels_[(size_t)channelIndex];
    auto picked = pickSampleForRow(pool, *targetRow, ch.sampleFile, rng);
    if (!picked.existsAsFile())
    {
        if (outMissing) outMissing->add(targetRow->name);
        return false;
    }

    ch.sampleFile = picked;
    ch.name = picked.getFileNameWithoutExtension();
    selectedChannel_ = channelIndex;
    repaint();
    if (onChannelDataChanged) onChannelDataChanged(channelIndex);
    if (onChannelsChanged) onChannelsChanged();
    return true;
}

bool ChannelRack::rerollHiHatPattern()
{
    int rowIndex = -1;
    for (int i = 0; i < (int)channels_.size(); ++i)
    {
        if (isHiHatChannel(i))
        {
            rowIndex = i;
            break;
        }
    }

    if (rowIndex < 0)
        return false;

    static const int boomBap[][16] = {
        {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,0,0},
        {1,0,0,1, 1,0,1,0, 1,0,0,1, 1,0,1,0},
        {1,0,1,0, 0,1,1,0, 1,0,1,0, 0,1,0,0},
        {1,0,0,0, 1,1,0,0, 1,0,1,0, 0,1,1,0}
    };
    static const int trap[][16] = {
        {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0},
        {1,1,0,1, 1,0,1,1, 1,1,0,1, 1,0,1,1},
        {1,0,1,1, 1,1,1,0, 1,0,1,1, 1,1,1,0},
        {1,1,1,1, 0,1,1,0, 1,1,1,1, 0,1,1,0}
    };
    static const int house[][16] = {
        {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0},
        {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0},
        {0,1,1,0, 0,1,1,0, 0,1,1,0, 0,1,1,0},
        {0,0,1,1, 0,0,1,1, 0,0,1,1, 0,0,1,1}
    };
    static const int dense[][16] = {
        {1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1},
        {1,1,0,1, 1,1,0,1, 1,1,0,1, 1,1,0,1},
        {1,0,1,1, 1,0,1,1, 1,0,1,1, 1,0,1,1},
        {1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0}
    };
    static const int syncopated[][16] = {
        {1,0,0,1, 0,1,0,0, 1,0,0,1, 0,1,0,0},
        {0,1,1,0, 0,1,0,1, 0,1,1,0, 0,1,0,1},
        {1,0,1,0, 0,1,0,1, 1,0,1,0, 0,1,0,1},
        {0,1,0,1, 1,0,1,0, 0,1,0,1, 1,0,1,0}
    };

    const int (*pool)[16] = boomBap;
    int poolSize = 4;
    const auto preset = currentDrumPresetId_.toLowerCase();
    if (preset == "trap" || preset == "drill" || preset == "memphis" || preset == "phonk")
        pool = trap;
    else if (preset == "house" || preset == "techno")
        pool = house;
    else if (preset == "dnb" || preset == "jersey")
        pool = dense;
    else if (preset == "afrobeat" || preset == "reggaeton" || preset == "ukg" || preset == "funk" || preset == "detroit")
        pool = syncopated;

    hiHatVariantCounter_ = (hiHatVariantCounter_ + 1) % poolSize;
    auto& ch = channels_[(size_t)rowIndex];
    ch.steps.assign((size_t)totalSteps_, false);

    const int drumPitch = DEFAULT_DRUM_PITCH;
    ch.pianoRollNotes.erase(std::remove_if(ch.pianoRollNotes.begin(), ch.pianoRollNotes.end(),
        [drumPitch](const Channel::Note& n) { return n.pitch == drumPitch; }),
        ch.pianoRollNotes.end());

    for (int step = 0; step < totalSteps_; ++step)
    {
        const bool active = pool[hiHatVariantCounter_][step % 16] != 0;
        ch.steps[(size_t)step] = active;
        if (active)
            ch.pianoRollNotes.push_back({ drumPitch, step, 1, 100 });
    }

    selectedChannel_ = rowIndex;
    repaint();
    if (onChannelDataChanged) onChannelDataChanged(rowIndex);
    return true;
}

void ChannelRack::applyStepPattern(const juce::String& title, const PatternGrid& grid)
{
    static const char* names[] = { "Kick", "Snare", "Hihat", "Perc" };
    static const InstrumentType types[] = {
        InstrumentType::Kick, InstrumentType::Snare, InstrumentType::Hihat, InstrumentType::Clap
    };

    channels_.clear();
    selectedChannel_ = -1;

    for (int row = 0; row < 4; ++row)
    {
        Channel ch;
        ch.name = juce::String(names[row]) + " - " + title;
        ch.type = types[row];
        ch.steps = std::vector<bool>(totalSteps_, false);

        const int drumPitch = DEFAULT_DRUM_PITCH;
        for (int step = 0; step < totalSteps_; ++step)
        {
            const bool active = grid[(size_t)row][(size_t)(step % 16)] != 0;
            ch.steps[(size_t)step] = active;
            if (active)
                ch.pianoRollNotes.push_back({ drumPitch, step, 1, 100 });
        }

        channels_.push_back(std::move(ch));
    }

    int bottomPad = 22;
    int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
    if (getHeight() < ideal) setSize(getWidth(), ideal);
    fitWidthToStepCount();

    repaint();
    if (onChannelDataChanged)
        for (int i = 0; i < (int)channels_.size(); ++i)
            onChannelDataChanged(i);
    if (onChannelsChanged) onChannelsChanged();
}

void ChannelRack::applyStepPatternToExistingRows(const PatternGrid& grid)
{
    const int rows = juce::jmin(4, (int)channels_.size());
    for (int row = 0; row < rows; ++row)
    {
        auto& ch = channels_[(size_t)row];
        if ((int)ch.steps.size() < totalSteps_)
            ch.steps.assign(totalSteps_, false);

        const int drumPitch = DEFAULT_DRUM_PITCH;
        ch.pianoRollNotes.erase(std::remove_if(ch.pianoRollNotes.begin(), ch.pianoRollNotes.end(),
            [drumPitch](const Channel::Note& n) { return n.pitch == drumPitch; }),
            ch.pianoRollNotes.end());

        for (int step = 0; step < totalSteps_; ++step)
        {
            const bool active = grid[(size_t)row][(size_t)(step % 16)] != 0;
            ch.steps[(size_t)step] = active;
            if (active)
                ch.pianoRollNotes.push_back({ drumPitch, step, 1, 100 });
        }
    }

    repaint();
    if (onChannelDataChanged)
        for (int i = 0; i < rows; ++i)
            onChannelDataChanged(i);
}

void ChannelRack::applyPatternLaneToExistingRows(const juce::String& title, int rowIndex, const std::array<int, 16>& steps)
{
    rowIndex = juce::jlimit(0, 3, rowIndex);

    std::vector<int> targets;
    for (int i = 0; i < (int)channels_.size(); ++i)
    {
        if (libraryPatternRowForChannel(channels_[(size_t)i]) == rowIndex)
            targets.push_back(i);
    }

    if (targets.empty() && rowIndex < (int)channels_.size())
        targets.push_back(rowIndex);

    for (int channelIndex : targets)
    {
        auto& ch = channels_[(size_t)channelIndex];
        if ((int)ch.steps.size() < totalSteps_)
            ch.steps.assign(totalSteps_, false);

        const int drumPitch = DEFAULT_DRUM_PITCH;
        ch.pianoRollNotes.erase(std::remove_if(ch.pianoRollNotes.begin(), ch.pianoRollNotes.end(),
            [drumPitch](const Channel::Note& n) { return n.pitch == drumPitch; }),
            ch.pianoRollNotes.end());

        for (int step = 0; step < totalSteps_; ++step)
        {
            const bool active = steps[(size_t)(step % 16)] != 0;
            ch.steps[(size_t)step] = active;
            if (active)
                ch.pianoRollNotes.push_back({ drumPitch, step, 1, 100 });
        }

        if (title.isNotEmpty())
            currentPatternName_ = title;
    }

    repaint();
    if (onChannelDataChanged)
        for (int channelIndex : targets)
            onChannelDataChanged(channelIndex);
}

void ChannelRack::showChannelContextMenu(int channelIndex, juce::Rectangle<int> targetArea)
{
    if (channelIndex < 0 || channelIndex >= (int)channels_.size())
        return;

    juce::PopupMenu menu;
    menu.addSectionHeader(channels_[(size_t)channelIndex].name);
    menu.addItem(1, "Use Stratum Bass  [Native]");
    menu.addSeparator();
    menu.addItem(2, "Clear assigned sound");

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(targetArea)),
        [this, channelIndex](int result)
        {
            if (result == 1)
            {
                setChannelToNativeBass(channelIndex);
                return;
            }

            if (result == 2 && channelIndex >= 0 && channelIndex < (int)channels_.size())
            {
                auto& ch = channels_[(size_t)channelIndex];
                if (ch.pluginSlotId >= 0)
                    pluginHost_.clearSlotTrack(ch.pluginSlotId);
                ch.pluginSlotId = -1;
                ch.builtInInstrument.clear();
                ch.sampleFile = {};
                repaint();
                if (onChannelDataChanged)
                    onChannelDataChanged(channelIndex);
                if (onChannelsChanged)
                    onChannelsChanged();
            }
        });
}

bool ChannelRack::setChannelToNativeBass(int channelIndex)
{
    if (channelIndex < 0 || channelIndex >= (int)channels_.size())
        return false;

    auto& ch = channels_[(size_t)channelIndex];
    if (ch.pluginSlotId >= 0)
        pluginHost_.clearSlotTrack(ch.pluginSlotId);
    ch.pluginSlotId = -1;
    ch.sampleFile = {};
    ch.builtInInstrument = "bass";
    ch.type = InstrumentType::Bass;
    if (ch.name.isEmpty() || ch.name == "Bass")
        ch.name = "Stratum Bass";
    selectedChannel_ = channelIndex;
    repaint();
    if (onChannelDataChanged)
        onChannelDataChanged(channelIndex);
    if (onChannelsChanged)
        onChannelsChanged();
    return true;
}

int ChannelRack::applyExtractedBassMidi(const juce::String& sourceName, const std::vector<Channel::Note>& notes, int targetChannel)
{
    if (notes.empty())
        return -1;

    int target = (targetChannel >= 0 && targetChannel < (int)channels_.size()) ? targetChannel : -1;
    if (target < 0)
    {
        for (int i = 0; i < (int)channels_.size(); ++i)
        {
            if (channels_[(size_t)i].name.startsWithIgnoreCase("Extracted Bass"))
            {
                target = i;
                break;
            }
        }
    }

    if (target < 0)
    {
        Channel ch;
        ch.name = "Extracted Bass";
        ch.type = InstrumentType::Bass;
        ch.steps = std::vector<bool>((size_t)totalSteps_, false);
        ch.volume = 0.85f;
        ch.mixerTrack = (int)channels_.size();
        channels_.push_back(std::move(ch));
        target = (int)channels_.size() - 1;
    }

    auto& ch = channels_[(size_t)target];
    if (ch.name.startsWithIgnoreCase("Extracted Bass") || ch.name.isEmpty())
        ch.name = sourceName.isNotEmpty() ? "Extracted Bass - " + sourceName : "Extracted Bass";
    ch.type = InstrumentType::Bass;
    ch.pianoRollNotes.clear();
    ch.steps.assign((size_t)totalSteps_, false);

    int maxEnd = totalSteps_;
    for (auto n : notes)
    {
        n.pitch = foldMidiIntoC4ToC6ForRack(n.pitch);
        n.startStep = juce::jmax(0, n.startStep);
        n.lengthSteps = juce::jmax(1, n.lengthSteps);
        n.velocity = juce::jlimit(1, 127, n.velocity);
        ch.pianoRollNotes.push_back(n);
        maxEnd = juce::jmax(maxEnd, n.startStep + n.lengthSteps);
        if (n.startStep >= 0 && n.startStep < totalSteps_)
            ch.steps[(size_t)n.startStep] = true;
    }

    if (maxEnd > (int)ch.steps.size())
        ch.steps.resize((size_t)juce::jmin(maxEnd, 256), false);
    for (const auto& n : ch.pianoRollNotes)
        if (n.startStep >= 0 && n.startStep < (int)ch.steps.size())
            ch.steps[(size_t)n.startStep] = true;

    selectedChannel_ = target;
    const int bottomPad = 22;
    const int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
    if (getHeight() < ideal)
        setSize(getWidth(), ideal);

    repaint();
    if (onChannelDataChanged)
        onChannelDataChanged(target);
    if (onChannelsChanged)
        onChannelsChanged();
    return target;
}

int ChannelRack::applyPlaylist808Midi(const juce::String& sourceName, const std::vector<Channel::Note>& notes)
{
    if (notes.empty())
        return -1;

    auto looksLike808Sound = [](const Channel& ch)
    {
        return ch.name.containsIgnoreCase("808")
            || ch.sampleFile.getFileNameWithoutExtension().containsIgnoreCase("808");
    };

    int target = -1;
    for (int i = 0; i < (int)channels_.size(); ++i)
    {
        if (looksLike808Sound(channels_[(size_t)i]))
        {
            target = i;
            break;
        }
    }

    if (target < 0)
    {
        for (int i = 0; i < (int)channels_.size(); ++i)
        {
            if (channels_[(size_t)i].name.equalsIgnoreCase("wait for 808")
                || channels_[(size_t)i].name.equalsIgnoreCase("waiting for 808"))
            {
                target = i;
                break;
            }
        }
    }

    if (target < 0)
    {
        Channel ch;
        ch.name = "wait for 808";
        ch.type = InstrumentType::Bass;
        ch.steps = std::vector<bool>((size_t)totalSteps_, false);
        ch.volume = 0.85f;
        ch.mixerTrack = (int)channels_.size();
        channels_.push_back(std::move(ch));
        target = (int)channels_.size() - 1;
    }

    auto& ch = channels_[(size_t)target];
    ch.type = InstrumentType::Bass;
    ch.pianoRollNotes.clear();
    ch.steps.assign((size_t)totalSteps_, false);

    int maxEnd = totalSteps_;
    for (auto n : notes)
    {
        n.startStep = juce::jmax(0, n.startStep);
        n.lengthSteps = juce::jmax(1, n.lengthSteps);
        n.velocity = juce::jlimit(1, 127, n.velocity);
        ch.pianoRollNotes.push_back(n);
        maxEnd = juce::jmax(maxEnd, n.startStep + n.lengthSteps);
    }

    if (maxEnd > (int)ch.steps.size())
        ch.steps.resize((size_t)juce::jmin(maxEnd, 256), false);
    for (const auto& n : ch.pianoRollNotes)
        if (n.startStep >= 0 && n.startStep < (int)ch.steps.size())
            ch.steps[(size_t)n.startStep] = true;

    selectedChannel_ = target;
    const int bottomPad = 22;
    const int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
    if (getHeight() < ideal)
        setSize(getWidth(), ideal);

    repaint();
    if (onChannelDataChanged)
        onChannelDataChanged(target);
    if (onChannelsChanged)
        onChannelsChanged();
    return target;
}

int ChannelRack::applyWaitFor808Midi(const std::vector<Channel::Note>& notes)
{
    if (notes.empty())
        return -1;

    int target = -1;
    for (int i = 0; i < (int)channels_.size(); ++i)
    {
        const auto& name = channels_[(size_t)i].name;
        if (name.equalsIgnoreCase("wait for 808")
            || name.equalsIgnoreCase("waiting for 808"))
        {
            target = i;
            break;
        }
    }

    if (target < 0)
    {
        Channel ch;
        ch.name = "wait for 808";
        ch.type = InstrumentType::Bass;
        ch.steps = std::vector<bool>((size_t)totalSteps_, false);
        ch.volume = 0.85f;
        ch.mixerTrack = (int)channels_.size();
        channels_.push_back(std::move(ch));
        target = (int)channels_.size() - 1;
    }

    auto& ch = channels_[(size_t)target];
    ch.name = "wait for 808";
    ch.type = InstrumentType::Bass;
    ch.pianoRollNotes.clear();
    ch.steps.assign((size_t)totalSteps_, false);

    int maxEnd = totalSteps_;
    for (auto n : notes)
    {
        n.pitch = foldMidiIntoC4ToC6ForRack(n.pitch);
        n.startStep = juce::jmax(0, n.startStep);
        n.lengthSteps = juce::jmax(1, n.lengthSteps);
        n.velocity = juce::jlimit(1, 127, n.velocity);
        ch.pianoRollNotes.push_back(n);
        maxEnd = juce::jmax(maxEnd, n.startStep + n.lengthSteps);
    }

    if (maxEnd > (int)ch.steps.size())
        ch.steps.resize((size_t)juce::jmin(maxEnd, 256), false);
    for (const auto& n : ch.pianoRollNotes)
        if (n.startStep >= 0 && n.startStep < (int)ch.steps.size())
            ch.steps[(size_t)n.startStep] = true;

    selectedChannel_ = target;
    const int bottomPad = 22;
    const int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
    if (getHeight() < ideal)
        setSize(getWidth(), ideal);

    repaint();
    if (onChannelDataChanged)
        onChannelDataChanged(target);
    if (onChannelsChanged)
        onChannelsChanged();
    return target;
}

int ChannelRack::applyExtractedChordifyMidi(const juce::String& sourceName, const std::vector<Channel::Note>& notes, int targetChannel)
{
    if (notes.empty())
        return -1;

    int target = (targetChannel >= 0 && targetChannel < (int)channels_.size()) ? targetChannel : -1;
    if (target < 0)
    {
        for (int i = 0; i < (int)channels_.size(); ++i)
        {
            if (channels_[(size_t)i].name.startsWithIgnoreCase("Extracted Chords"))
            {
                target = i;
                break;
            }
        }
    }

    if (target < 0)
    {
        Channel ch;
        ch.name = "Extracted Chords";
        ch.type = InstrumentType::Pad;
        ch.steps = std::vector<bool>((size_t)totalSteps_, false);
        ch.volume = 0.85f;
        ch.mixerTrack = (int)channels_.size();
        channels_.push_back(std::move(ch));
        target = (int)channels_.size() - 1;
    }

    auto& ch = channels_[(size_t)target];
    ch.name = sourceName.isNotEmpty() ? "Extracted Chords - " + sourceName : "Extracted Chords";
    ch.type = InstrumentType::Pad;
    ch.pianoRollNotes.clear();
    ch.steps.assign((size_t)totalSteps_, false);

    int maxEnd = totalSteps_;
    for (auto n : notes)
    {
        n.pitch = juce::jlimit(36, 96, n.pitch);
        n.startStep = juce::jmax(0, n.startStep);
        n.lengthSteps = juce::jmax(1, n.lengthSteps);
        n.velocity = juce::jlimit(1, 127, n.velocity);
        ch.pianoRollNotes.push_back(n);
        maxEnd = juce::jmax(maxEnd, n.startStep + n.lengthSteps);
        if (n.startStep >= 0 && n.startStep < totalSteps_)
            ch.steps[(size_t)n.startStep] = true;
    }

    if (maxEnd > (int)ch.steps.size())
        ch.steps.resize((size_t)juce::jmin(maxEnd, 256), false);

    selectedChannel_ = target;
    const int bottomPad = 22;
    const int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
    if (getHeight() < ideal)
        setSize(getWidth(), ideal);

    repaint();
    if (onChannelDataChanged)
        onChannelDataChanged(target);
    if (onChannelsChanged)
        onChannelsChanged();
    return target;
}

juce::StringArray ChannelRack::getAvailableDrumPresets()
{
    juce::StringArray ids;
    for (auto& p : kPresets) ids.add(p.id);
    return ids;
}

std::vector<ChannelRack::DrumPresetFolderInfo> ChannelRack::getDrumPresetFolders()
{
    std::vector<DrumPresetFolderInfo> out;
    for (auto& p : kPresets)
    {
        // Skip the "Clear All" entry — it isn't a real drum kit.
        if (juce::String(p.id).equalsIgnoreCase("empty"))
            continue;
        out.push_back({ juce::String::fromUTF8(p.id),
                        juce::String::fromUTF8(p.label),
                        juce::String::fromUTF8(p.sampleFolder) });
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────
// Drum Path configuration — runtime-editable, persisted to user data dir.
// ─────────────────────────────────────────────────────────────────────
namespace {
    std::vector<ChannelRack::DrumPathConfig>& mutableDrumPathConfigs()
    {
        static std::vector<ChannelRack::DrumPathConfig> configs;
        static bool initialized = false;
        if (!initialized)
        {
            initialized = true;
            // Seed defaults from the hardcoded kPresets table.
            for (auto& p : kPresets)
            {
                if (juce::String(p.id).equalsIgnoreCase("empty"))
                    continue;
                ChannelRack::DrumPathConfig c;
                c.id    = juce::String::fromUTF8(p.id);
                c.label = juce::String::fromUTF8(p.label);
                juce::String f = juce::String::fromUTF8(p.sampleFolder);
                if (f.isNotEmpty())
                    c.folders.add(f);
                c.mode = ChannelRack::DrumPathMode::All;
                c.specificIndex = 0;
                configs.push_back(std::move(c));
            }
            // Overlay anything persisted from previous sessions.
            ChannelRack::loadDrumPathConfigs();
        }
        return configs;
    }

    ChannelRack::DrumPathConfig* findMutableConfig(const juce::String& presetId)
    {
        for (auto& c : mutableDrumPathConfigs())
            if (c.id.equalsIgnoreCase(presetId)) return &c;
        return nullptr;
    }
}

juce::File ChannelRack::drumPathConfigFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Stratum");
    if (!dir.isDirectory()) dir.createDirectory();
    return dir.getChildFile("drum_paths.json");
}

std::vector<ChannelRack::DrumPathConfig> ChannelRack::getDrumPathConfigs()
{
    return mutableDrumPathConfigs();
}

ChannelRack::DrumPathConfig ChannelRack::getDrumPathConfig(const juce::String& presetId)
{
    if (auto* c = findMutableConfig(presetId)) return *c;
    return {};
}

void ChannelRack::addDrumPathFolder(const juce::String& presetId, const juce::String& absolutePath)
{
    auto* c = findMutableConfig(presetId);
    if (!c) return;
    if (absolutePath.isEmpty()) return;
    if (c->folders.contains(absolutePath)) return;  // de-dupe
    c->folders.add(absolutePath);
    saveDrumPathConfigs();
}

void ChannelRack::removeDrumPathFolder(const juce::String& presetId, int folderIndex)
{
    auto* c = findMutableConfig(presetId);
    if (!c) return;
    if (folderIndex < 0 || folderIndex >= c->folders.size()) return;
    c->folders.remove(folderIndex);
    if (c->specificIndex >= c->folders.size())
        c->specificIndex = juce::jmax(0, c->folders.size() - 1);
    saveDrumPathConfigs();
}

void ChannelRack::setDrumPathMode(const juce::String& presetId, DrumPathMode mode, int specificIndex)
{
    auto* c = findMutableConfig(presetId);
    if (!c) return;
    c->mode = mode;
    c->specificIndex = juce::jlimit(0, juce::jmax(0, c->folders.size() - 1), specificIndex);
    saveDrumPathConfigs();
}

void ChannelRack::saveDrumPathConfigs()
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    juce::Array<juce::var> arr;
    for (const auto& c : mutableDrumPathConfigs())
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("id",    c.id);
        obj->setProperty("label", c.label);
        juce::Array<juce::var> fldArr;
        for (auto& f : c.folders) fldArr.add(f);
        obj->setProperty("folders", fldArr);
        obj->setProperty("mode", (int)c.mode);
        obj->setProperty("specificIndex", c.specificIndex);
        arr.add(juce::var(obj.get()));
    }
    root->setProperty("configs", arr);
    auto file = drumPathConfigFile();
    file.replaceWithText(juce::JSON::toString(juce::var(root.get()), true));
}

void ChannelRack::loadDrumPathConfigs()
{
    auto file = drumPathConfigFile();
    if (!file.existsAsFile()) return;
    auto v = juce::JSON::parse(file);
    if (!v.isObject()) return;
    auto arr = v.getProperty("configs", juce::var());
    if (!arr.isArray()) return;
    auto& configs = mutableDrumPathConfigs();
    for (int i = 0; i < arr.size(); ++i)
    {
        auto& obj = arr[i];
        juce::String id = obj.getProperty("id", "").toString();
        if (id.isEmpty()) continue;
        // find or skip — we only overlay known presets
        DrumPathConfig* target = nullptr;
        for (auto& c : configs) if (c.id.equalsIgnoreCase(id)) { target = &c; break; }
        if (!target) continue;
        target->folders.clear();
        auto fldArr = obj.getProperty("folders", juce::var());
        if (fldArr.isArray())
            for (int k = 0; k < fldArr.size(); ++k)
                target->folders.add(fldArr[k].toString());
        target->mode = (DrumPathMode)(int)obj.getProperty("mode", 0);
        target->specificIndex = (int)obj.getProperty("specificIndex", 0);
        if (target->specificIndex >= target->folders.size())
            target->specificIndex = juce::jmax(0, target->folders.size() - 1);
    }
}

double ChannelRack::getPresetBPM(const juce::String& presetId)
{
    // Genre-typical tempos.
    if (presetId.equalsIgnoreCase("boom_bap") || presetId.equalsIgnoreCase("Boom Bap")) return 90.0;
    if (presetId.equalsIgnoreCase("hiphop")   || presetId.equalsIgnoreCase("Hip Hop"))  return 92.0;
    if (presetId.equalsIgnoreCase("trap"))                                              return 140.0;
    if (presetId.equalsIgnoreCase("drill"))                                             return 142.0;
    if (presetId.equalsIgnoreCase("house"))                                             return 124.0;
    if (presetId.equalsIgnoreCase("rnb")      || presetId.equalsIgnoreCase("R&B"))      return 75.0;
    if (presetId.equalsIgnoreCase("lofi")     || presetId.equalsIgnoreCase("Lo-Fi"))    return 75.0;
    if (presetId.equalsIgnoreCase("rock"))                                              return 120.0;
    if (presetId.equalsIgnoreCase("detroit") || presetId.equalsIgnoreCase("Detroit Flint")) return 70.0;
    if (presetId.equalsIgnoreCase("afrobeat") || presetId.equalsIgnoreCase("Afrobeat"))  return 104.0;
    if (presetId.equalsIgnoreCase("reggaeton") || presetId.equalsIgnoreCase("Reggaeton")) return 96.0;
    if (presetId.equalsIgnoreCase("jersey") || presetId.equalsIgnoreCase("Jersey Club")) return 135.0;
    if (presetId.equalsIgnoreCase("ukg") || presetId.equalsIgnoreCase("UK Garage"))      return 132.0;
    if (presetId.equalsIgnoreCase("dnb") || presetId.equalsIgnoreCase("Drum & Bass"))    return 174.0;
    if (presetId.equalsIgnoreCase("techno"))                                             return 128.0;
    if (presetId.equalsIgnoreCase("phonk"))                                              return 130.0;
    if (presetId.equalsIgnoreCase("memphis"))                                            return 150.0;
    if (presetId.equalsIgnoreCase("funk"))                                               return 102.0;
    return 0.0;  // empty / unknown
}

// ─── Project I/O ─────────────────────────────────────────────────────
juce::var ChannelRack::toJson() const
{
    auto* obj = new juce::DynamicObject();
    juce::Array<juce::var> chArr;
    for (const auto& ch : channels_)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("name",       ch.name);
        o->setProperty("type",       (int)ch.type);
        o->setProperty("muted",      ch.muted);
        o->setProperty("solo",       ch.solo);
        o->setProperty("volume",     ch.volume);
        o->setProperty("pan",        ch.pan);
        o->setProperty("mixerTrack", ch.mixerTrack);
        o->setProperty("sampleFile", ch.sampleFile.getFullPathName());
        o->setProperty("isMusicLoop", ch.isMusicLoop);
        o->setProperty("loopSlot", ch.loopSlot);
        o->setProperty("pluginSlotId", ch.pluginSlotId);
        o->setProperty("builtInInstrument", ch.builtInInstrument);

        juce::Array<juce::var> stepsArr;
        for (bool b : ch.steps) stepsArr.add(b);
        o->setProperty("steps", stepsArr);

        juce::Array<juce::var> notesArr;
        for (const auto& n : ch.pianoRollNotes)
        {
            auto* no = new juce::DynamicObject();
            no->setProperty("pitch",       n.pitch);
            no->setProperty("startStep",   n.startStep);
            no->setProperty("lengthSteps", n.lengthSteps);
            no->setProperty("velocity",    n.velocity);
            notesArr.add(juce::var(no));
        }
        o->setProperty("notes", notesArr);
        chArr.add(juce::var(o));
    }
    obj->setProperty("channels",   chArr);
    obj->setProperty("totalSteps", totalSteps_);
    obj->setProperty("drumPresetId", currentDrumPresetId_);
    juce::String swingId = "none";
    switch (swingPreset_)
    {
        case SwingPreset::Dilla:      swingId = "dilla"; break;
        case SwingPreset::MfDoom:     swingId = "mf_doom"; break;
        case SwingPreset::JoeyBadass: swingId = "joey_badass"; break;
        case SwingPreset::None:
        default: break;
    }
    obj->setProperty("drumSwingId", swingId);
    return juce::var(obj);
}

void ChannelRack::fromJson(const juce::var& v)
{
    if (!v.isObject()) return;
    if (v.hasProperty("totalSteps"))
    {
        // Allow legacy 16/32 projects to load, plus the new 64 (4 BAR) and
        // 128 (8 BAR) lengths. Default to 8 BAR on missing property.
        totalSteps_ = juce::jlimit(16, STEPS_8_BAR,
                                    (int)v.getProperty("totalSteps", STEPS_8_BAR));
    }
    currentDrumPresetId_ = v.getProperty("drumPresetId", "none").toString();
    if (currentDrumPresetId_.isEmpty())
        currentDrumPresetId_ = "none";

    const auto swingId = v.getProperty("drumSwingId", "none").toString().toLowerCase();
    if (swingId == "dilla") swingPreset_ = SwingPreset::Dilla;
    else if (swingId == "mf_doom" || swingId == "mf doom") swingPreset_ = SwingPreset::MfDoom;
    else if (swingId == "joey_badass" || swingId == "joey badass" || swingId == "joey")
        swingPreset_ = SwingPreset::JoeyBadass;
    else
        swingPreset_ = SwingPreset::None;

    auto chArr = v.getProperty("channels", juce::var());
    if (!chArr.isArray()) return;

    channels_.clear();
    for (auto& cv : *chArr.getArray())
    {
        Channel ch;
        ch.name   = cv.getProperty("name", "Channel").toString();
        ch.type   = (InstrumentType)(int)cv.getProperty("type", (int)InstrumentType::Kick);
        ch.muted  = (bool)cv.getProperty("muted", false);
        ch.solo   = (bool)cv.getProperty("solo",  false);
        ch.volume = (float)(double)cv.getProperty("volume", 1.0);
        ch.pan    = (float)(double)cv.getProperty("pan",    0.0);
        ch.mixerTrack = (int)cv.getProperty("mixerTrack", -1);
        ch.builtInInstrument = cv.getProperty("builtInInstrument", "").toString();
        ch.pluginSlotId = (int)cv.getProperty("pluginSlotId", -1);

        juce::String path = cv.getProperty("sampleFile", "").toString();
        if (path.isNotEmpty()) ch.sampleFile = juce::File(path);
        ch.isMusicLoop = (bool)cv.getProperty("isMusicLoop", false);
        ch.loopSlot = (int)cv.getProperty("loopSlot", 0);
        if (ch.isMusicLoop && ch.sampleFile.existsAsFile())
            buildWaveformPeaksForChannel(ch);

        ch.steps.assign(totalSteps_, false);
        if (auto* sArr = cv.getProperty("steps", juce::var()).getArray())
            for (int i = 0; i < sArr->size() && i < totalSteps_; ++i)
                ch.steps[i] = (bool)(*sArr)[i];

        if (auto* nArr = cv.getProperty("notes", juce::var()).getArray())
            for (auto& nv : *nArr)
            {
                Channel::Note n;
                n.pitch       = (int)nv.getProperty("pitch",       60);
                n.startStep   = (int)nv.getProperty("startStep",   0);
                n.lengthSteps = (int)nv.getProperty("lengthSteps", 1);
                n.velocity    = (int)nv.getProperty("velocity",    100);
                ch.pianoRollNotes.push_back(n);
            }

        channels_.push_back(std::move(ch));
    }

    // Auto-grow rack height if needed
    int bottomPad = 22;
    int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
    if (getHeight() < ideal) setSize(getWidth(), ideal);
    fitWidthToStepCount();

    selectedChannel_ = channels_.empty() ? -1 : 0;
    for (int i = 0; i < (int)channels_.size(); ++i)
    {
        if (channels_[i].pluginSlotId >= 0)
            pluginHost_.setSlotTrack(channels_[i].pluginSlotId,
                                     channels_[i].mixerTrack >= 0 ? channels_[i].mixerTrack : i);
    }
    if (onChannelsChanged) onChannelsChanged();
    repaint();
}
