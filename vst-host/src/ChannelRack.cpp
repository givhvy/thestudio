#include "ChannelRack.h"
#include "PluginHost.h"
#include "Theme.h"
#include <algorithm>
#include <map>

ChannelRack::ChannelRack(PluginHost& pluginHost)
    : pluginHost_(pluginHost)
{
    // Initialize with some default channels
    channels_.push_back({"Kick", InstrumentType::Kick, std::vector<bool>(16, false)});
    channels_.push_back({"Snare", InstrumentType::Snare, std::vector<bool>(16, false)});
    channels_.push_back({"Hihat", InstrumentType::Hihat, std::vector<bool>(16, false)});
    channels_.push_back({"Clap", InstrumentType::Clap, std::vector<bool>(16, false)});
    
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
    g.drawText(juce::String(totalSteps_) + " STEP", stepCountRect.toNearestInt(), juce::Justification::centred);

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

    const int volumeChannel = selectedChannel_ >= 0 && selectedChannel_ < (int)channels_.size() ? selectedChannel_ : 0;
    auto volRect = getHeaderVolumeRect();
    if (!volRect.isEmpty() && volumeChannel >= 0 && volumeChannel < (int)channels_.size())
    {
        const auto& ch = channels_[(size_t)volumeChannel];
        auto vf = volRect.toFloat();
        g.setColour(juce::Colour(0xff050507));
        g.fillRoundedRectangle(vf, 4.0f);
        g.setColour(juce::Colour(0xff27272a));
        g.drawRoundedRectangle(vf, 4.0f, 1.0f);

        const int fillW = (int)((float)volRect.getWidth() * juce::jlimit(0.0f, 1.0f, ch.volume));
        auto fill = volRect.withWidth(fillW).toFloat();
        juce::ColourGradient vg(Theme::orange1, fill.getX(), fill.getY(),
                                Theme::orange4, fill.getX(), fill.getBottom(), false);
        g.setGradientFill(vg);
        g.fillRoundedRectangle(fill, 4.0f);

        g.setColour(Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
        g.drawText("VOL", volRect.getX() - 28, volRect.getY(), 24, volRect.getHeight(),
                   juce::Justification::centredRight);
        g.setColour(Theme::zinc200);
        g.drawText(juce::String((int)std::round(ch.volume * 100.0f)) + "%",
                   volRect.getRight() + 5, volRect.getY(), 38, volRect.getHeight(),
                   juce::Justification::centredLeft);
    }
    
    // Step number labels
    int stepLabelX = CHANNELS_START_X;
    for (int step = 0; step < totalSteps_; ++step)
    {
        if (step % 4 == 0 && step > 0)
            stepLabelX += BEAT_GAP;
        
        juce::String stepText = juce::String(step + 1);
        bool isCurrent = (step == currentStep_);
        bool isBeat = (step % 4 == 0);
        
        if (isCurrent)
        {
            g.setColour(juce::Colour(0xfff97316));
        }
        else
        {
            g.setColour(isBeat ? juce::Colour(0xff71717a) : juce::Colour(0xff52525b));
        }
        g.setFont(juce::FontOptions().withName("Consolas").withHeight(9.5f).withStyle(isBeat ? "Bold" : ""));
        g.drawText(stepText, stepLabelX, header.getY(), STEP_WIDTH, header.getHeight(), juce::Justification::centred);
        stepLabelX += STEP_WIDTH + STEP_GAP;
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
    // Other layout is handled in paint()
}

void ChannelRack::mouseDown(const juce::MouseEvent& e)
{
    isDraggingPanel_ = false;

    // Bottom "+" button: open VST instrument picker.
    if (getAddVstButtonRect().contains(e.x, e.y))
    {
        if (onAddVstChannel) onAddVstChannel();
        return;
    }
    
    if (e.y < HEADER_HEIGHT)
    {
        juce::Rectangle<int> stepCountRect(14, 6, 58, 18);
        if (stepCountRect.contains(e.x, e.y))
        {
            if (onToggle16_32) onToggle16_32();
            else toggleStepCount();
            return;
        }

        if (getHeaderVolumeRect().contains(e.x, e.y))
        {
            draggingHeaderVolume_ = true;
            setSelectedChannelVolumeFromX(e.x);
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
        if (onChannelClicked) onChannelClicked(channelIdx);
        repaint();
        return;
    }

    if (getHiHatChangeButtonRect(channelIdx).contains(e.x, e.y))
    {
        selectedChannel_ = channelIdx;
        rerollHiHatPattern();
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

void ChannelRack::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingHeaderVolume_)
    {
        setSelectedChannelVolumeFromX(e.x);
        return;
    }

    if (isDraggingPanel_)
        dragger_.dragComponent(this, e, nullptr);
}

void ChannelRack::mouseUp(const juce::MouseEvent&)
{
    draggingHeaderVolume_ = false;
    isDraggingPanel_ = false;
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
    juce::File file(path);
    
    if (file.existsAsFile())
    {
        int channelIdx = getChannelAtY(details.localPosition.y);
        
        if (channelIdx >= 0 && channelIdx < (int)channels_.size())
        {
            // Drop onto an existing row → replace its sample (stay on current view)
            auto& ch = channels_[channelIdx];
            ch.sampleFile = file;
            ch.name = file.getFileNameWithoutExtension();
            selectedChannel_ = channelIdx;
        }
        else
        {
            // Drop onto empty space → add a new channel
            Channel ch;
            ch.name = file.getFileNameWithoutExtension();
            ch.type = InstrumentType::Kick;
            ch.steps = std::vector<bool>(totalSteps_, false);
            ch.sampleFile = file;
            channels_.push_back(std::move(ch));
            selectedChannel_ = (int)channels_.size() - 1;

            int bottomPad = 22;
            int ideal = HEADER_HEIGHT + (int)channels_.size() * CHANNEL_HEIGHT + bottomPad;
            if (getHeight() < ideal) setSize(getWidth(), ideal);
        }
    }
    
    dropHighlightRow_ = -1;
    repaint();
}

void ChannelRack::setPlaying(bool playing)
{
    const bool wasPlaying = isPlaying_;
    isPlaying_ = playing;

    if (playing)
    {
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
                for (int i = 0; i < (int)channels_.size(); ++i)
                {
                    const auto& ch = channels_[(size_t)i];
                    const int channelLength = getChannelPatternLength(ch);
                    const int channelStep = isMelodicChannel(ch)
                        ? (absoluteStep_ % juce::jmax(1, channelLength))
                        : currentStep_;
                    if (isMelodicChannel(ch) || (channelStep < (int)ch.steps.size() && ch.steps[(size_t)channelStep]))
                        triggerChannel(i, channelStep);
                }
            }
        }
    }
    else
    {
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
    totalSteps_ = (totalSteps_ == 16) ? 32 : 16;
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
        for (int i = 0; i < (int)channels_.size(); ++i)
        {
            const auto& ch = channels_[(size_t)i];
            const int channelLength = getChannelPatternLength(ch);
            const int channelStep = isMelodicChannel(ch)
                ? (absoluteStep_ % juce::jmax(1, channelLength))
                : currentStep_;
            if (isMelodicChannel(ch) || (channelStep < (int)ch.steps.size() && ch.steps[(size_t)channelStep]))
                triggerChannel(i, channelStep);
        }
    }
    
    if (onPlayheadTick) onPlayheadTick(absoluteStep_, isPlaying_);
    repaint();
}

bool ChannelRack::isMelodicChannel(const Channel& channel) const
{
    return channel.pluginSlotId >= 0
        || channel.builtInInstrument == "piano"
        || channel.type == InstrumentType::Lead
        || channel.type == InstrumentType::Pad
        || channel.type == InstrumentType::Bass;
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
        for (const auto& note : collectNotesAtCurrentStep())
        {
            const int pitch = juce::jlimit(0, 127, note.pitch);
            const int velocity = juce::jlimit(1, 127, (int)std::round(ch.volume * (float)note.velocity));
            const int offDelayMs = juce::jmax(40, (int)std::round((60000.0 / juce::jmax(1.0, bpm_) / 4.0) * juce::jmax(1, note.lengthSteps)));
            pluginHost_.sendMidiNote(slot, 1, pitch, velocity, true);
            juce::Timer::callAfterDelay(offDelayMs, [this, slot, pitch]() {
                pluginHost_.sendMidiNote(slot, 1, pitch, 0, false);
            });
        }
        return;
    }

    if (ch.builtInInstrument == "piano")
    {
        const double secondsPerStep = 60.0 / juce::jmax(1.0, bpm_) / 4.0;
        for (const auto& note : collectNotesAtCurrentStep())
        {
            const float velocity = juce::jlimit(0.0f, 1.0f, ch.volume * ((float)note.velocity / 127.0f));
            pluginHost_.playSynthPiano(note.pitch, 0.0, secondsPerStep * juce::jmax(1, note.lengthSteps), velocity);
        }
        return;
    }

    // If a sample file is assigned (e.g. dragged from Browser), play it.
    // Route through the channel's assigned mixer track (default = row index).
    if (ch.sampleFile.existsAsFile())
    {
        const int track = (ch.mixerTrack >= 0) ? ch.mixerTrack : channelIdx;
        pluginHost_.playSampleFile(ch.sampleFile, track, 0.0, ch.volume);
        return;
    }
    
    // Otherwise fall back to the built-in synthesized drum based on channel type
    double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    switch (ch.type)
    {
        case InstrumentType::Kick:  pluginHost_.playSynthKick(now); break;
        case InstrumentType::Snare: pluginHost_.playSynthSnare(now); break;
        case InstrumentType::Hihat: pluginHost_.playSynthHihat(now, false); break;
        case InstrumentType::Clap:  pluginHost_.playSynthClap(now); break;
        default:
            pluginHost_.playSynthTone(440.0, now, 0.15, 0.6f);
            break;
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
    
    int relativeX = x - CHANNELS_START_X;
    int stepX = 0;
    const int hitPad = 3;

    for (int step = 0; step < totalSteps_; ++step)
    {
        if (step % 4 == 0 && step > 0)
            stepX += BEAT_GAP;
        
        if (relativeX >= stepX - hitPad && relativeX < stepX + STEP_WIDTH + hitPad)
            return step;

        stepX += STEP_WIDTH + STEP_GAP;
    }

    return -1;
}

int ChannelRack::getRequiredWidthForSteps() const
{
    int stepsW = 0;
    for (int step = 0; step < totalSteps_; ++step)
    {
        if (step % 4 == 0 && step > 0)
            stepsW += BEAT_GAP;
        stepsW += STEP_WIDTH + STEP_GAP;
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

juce::Rectangle<int> ChannelRack::getHiHatChangeButtonRect(int channelIndex) const
{
    if (!isHiHatChannel(channelIndex))
        return {};

    int stepAreaW = 0;
    for (int step = 0; step < totalSteps_; ++step)
    {
        if (step % 4 == 0 && step > 0)
            stepAreaW += BEAT_GAP;
        stepAreaW += STEP_WIDTH + STEP_GAP;
    }

    const int rowY = HEADER_HEIGHT + channelIndex * CHANNEL_HEIGHT;
    return juce::Rectangle<int>(CHANNELS_START_X + stepAreaW + 5,
                                rowY + (CHANNEL_HEIGHT - 18) / 2,
                                18, 18);
}

juce::Rectangle<int> ChannelRack::getHeaderVolumeRect() const
{
    const int w = 104;
    const int x = getWidth() - w - 54;
    if (x < CHANNELS_START_X + 120)
        return {};
    return juce::Rectangle<int>(x, 7, w, 16);
}

void ChannelRack::setSelectedChannelVolumeFromX(int x)
{
    int idx = selectedChannel_;
    if (idx < 0 || idx >= (int)channels_.size())
        idx = channels_.empty() ? -1 : 0;
    if (idx < 0)
        return;

    auto r = getHeaderVolumeRect();
    if (r.isEmpty())
        return;

    const float rel = (float)(x - r.getX()) / (float)juce::jmax(1, r.getWidth());
    channels_[(size_t)idx].volume = juce::jlimit(0.0f, 1.0f, rel);
    repaint();
    if (onChannelDataChanged)
        onChannelDataChanged(idx);
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
    g.drawText(juce::String(channelIndex + 1), x, bounds.getY(), CH_INDEX_WIDTH, bounds.getHeight(), juce::Justification::centred);
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
    if (isMelodicChannel(channel) && !channel.pianoRollNotes.empty())
    {
        int stepAreaW = 0;
        for (int step = 0; step < totalSteps_; ++step)
        {
            if (step % 4 == 0 && step > 0)
                stepAreaW += BEAT_GAP;
            stepAreaW += STEP_WIDTH + STEP_GAP;
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

    for (int step = 0; step < totalSteps_; ++step)
    {
        if (step % 4 == 0 && step > 0)
            x += BEAT_GAP;
        
        auto stepRect = juce::Rectangle<float>((float)x, (float)cy - STEP_HEIGHT/2.0f, (float)STEP_WIDTH, (float)STEP_HEIGHT);
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
        
        x += STEP_WIDTH + STEP_GAP;
    }

    auto changeRect = getHiHatChangeButtonRect(channelIndex);
    if (!changeRect.isEmpty())
    {
        auto rf = changeRect.toFloat();
        juce::ColourGradient cg(juce::Colour(0xff27272a), rf.getX(), rf.getY(),
                                juce::Colour(0xff0a0a0c), rf.getX(), rf.getBottom(), false);
        g.setGradientFill(cg);
        g.fillRoundedRectangle(rf, 9.0f);
        g.setColour(juce::Colour(0xfff97316).withAlpha(0.35f));
        g.fillEllipse(rf.reduced(4.0f));
        g.setColour(juce::Colour(0xfff97316));
        g.drawRoundedRectangle(rf.reduced(0.5f), 9.0f, 1.0f);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
        g.drawText("R", changeRect, juce::Justification::centred);
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
    static const char* const BOOM_BAP_FOLDER =
        R"(E:\!Storage\1500 THE DRUMS LORD COLLECTION\! Maxeyy Stash V5 Drum Kit\Boom Bap Stash V2)";
    static const char* const TRAP_FOLDER =
        R"(E:\!Storage\1500 THE DRUMS LORD COLLECTION\! Maxeyy - Stash V3 Drum Kit)";
    static const char* const DETROIT_FOLDER =
        R"(E:\!Storage\1500 THE DRUMS LORD COLLECTION\!FLINT DETROIT\What Up Sav Vol 1(Flint & Detroit Drum Kit))";

    static const DrumPreset kPresets[] = {
        { "boom_bap", "Boom Bap", ROWS_BOOMBAP, BOOM_BAP_FOLDER },
        { "hiphop",   "Hip Hop",  ROWS_HIPHOP,  "" },
        { "trap",     "Trap",     ROWS_TRAP,    TRAP_FOLDER },
        { "drill",    "Drill",    ROWS_DRILL,   "" },
        { "house",    "House",    ROWS_HOUSE,   "" },
        { "rnb",      "R&B",      ROWS_RNB,     "" },
        { "lofi",     "Lo-Fi",    ROWS_LOFI,    "" },
        { "rock",     "Rock",          ROWS_ROCK,    "" },
        { "detroit",  "Detroit Flint", ROWS_DETROIT, DETROIT_FOLDER },
        { "afrobeat", "Afrobeat",      ROWS_AFROBEAT, "" },
        { "reggaeton","Reggaeton",     ROWS_REGGAETON, "" },
        { "jersey",   "Jersey Club",   ROWS_JERSEY, "" },
        { "ukg",      "UK Garage",     ROWS_UKG, "" },
        { "dnb",      "Drum & Bass",   ROWS_DNB, "" },
        { "techno",   "Techno",        ROWS_TECHNO, "" },
        { "phonk",    "Phonk",         ROWS_PHONK, "" },
        { "memphis",  "Memphis",       ROWS_MEMPHIS, "" },
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

bool ChannelRack::applyDrumPreset(const juce::String& presetId, juce::StringArray* outMissing)
{
    const DrumPreset* p = findPreset(presetId);
    if (!p) return false;
    currentDrumPresetId_ = p->id;
    hiHatVariantCounter_ = 0;

    const juce::File folder(juce::String::fromUTF8(p->sampleFolder));
    const bool haveFolder = folder.isDirectory();
    const auto& pool = haveFolder ? scanAudioRecursive(folder) : juce::Array<juce::File>{};
    juce::Random rng;  // seeded from time by default

    // Replace the channel list with fresh rows from the preset. Without this,
    // switching presets keeps accumulating rows because the previous preset
    // renamed the channels to sample filenames so alias matching fails.
    channels_.clear();
    selectedChannel_ = -1;

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
    int rowIndex = 0;
    for (const PresetRow* row = p->rows; row->name != nullptr; ++row, ++rowIndex)
    {
        if (rowIndex >= (int)channels_.size())
            break;

        auto& ch = channels_[(size_t)rowIndex];
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

juce::StringArray ChannelRack::getAvailableDrumPresets()
{
    juce::StringArray ids;
    for (auto& p : kPresets) ids.add(p.id);
    return ids;
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
        o->setProperty("sampleFile", ch.sampleFile.getFullPathName());
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
    return juce::var(obj);
}

void ChannelRack::fromJson(const juce::var& v)
{
    if (!v.isObject()) return;
    if (v.hasProperty("totalSteps"))
        totalSteps_ = juce::jlimit(16, 32, (int)v.getProperty("totalSteps", 16));

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
        ch.volume = (float)(double)cv.getProperty("volume", 0.8);
        ch.pan    = (float)(double)cv.getProperty("pan",    0.0);
        ch.builtInInstrument = cv.getProperty("builtInInstrument", "").toString();

        juce::String path = cv.getProperty("sampleFile", "").toString();
        if (path.isNotEmpty()) ch.sampleFile = juce::File(path);

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
    repaint();
}
