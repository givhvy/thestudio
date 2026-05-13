#include "ChannelRack.h"
#include "PluginHost.h"
#include "Theme.h"
#include <algorithm>

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
    
    // Step number labels
    for (int step = 0; step < totalSteps_; ++step)
    {
        int x = CHANNELS_START_X + step * (STEP_WIDTH + STEP_GAP);
        if (step % 4 == 0 && step > 0)
            x += BEAT_GAP;
        
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
        g.drawText(stepText, x, header.getY(), STEP_WIDTH, header.getHeight(), juce::Justification::centred);
    }
    
    // Draw channels
    for (int i = 0; i < (int)channels_.size(); ++i)
    {
        auto channelBounds = bounds.removeFromTop(CHANNEL_HEIGHT);
        drawChannel(g, channelBounds, i);
    }
}

void ChannelRack::resized()
{
    // Layout is handled in paint()
}

void ChannelRack::mouseDown(const juce::MouseEvent& e)
{
    isDraggingPanel_ = false;
    
    // Header (top strip): start dragging the whole panel
    if (e.y < HEADER_HEIGHT)
    {
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
    
    // Step click → toggle step (also sync to piano roll notes)
    int stepIdx = getStepAtX(e.x);
    if (stepIdx >= 0 && stepIdx < (int)channel.steps.size())
    {
        bool nowActive = !channel.steps[stepIdx];
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
    if (isDraggingPanel_)
        dragger_.dragComponent(this, e, nullptr);
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
        }
    }
    
    dropHighlightRow_ = -1;
    repaint();
}

void ChannelRack::setPlaying(bool playing)
{
    isPlaying_ = playing;
    if (playing)
        startTimer(60000.0 / bpm_ / 4.0); // 16th notes
    else
        stopTimer();
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

void ChannelRack::timerCallback()
{
    currentStep_ = (currentStep_ + 1) % totalSteps_;
    
    // Trigger channels that have steps at current position
    for (int i = 0; i < (int)channels_.size(); ++i)
    {
        if (currentStep_ < (int)channels_[i].steps.size() && channels_[i].steps[currentStep_])
        {
            triggerChannel(i);
        }
    }
    
    repaint();
}

void ChannelRack::triggerChannel(int channelIdx)
{
    if (channelIdx < 0 || channelIdx >= (int)channels_.size())
        return;
    
    const auto& ch = channels_[channelIdx];
    if (ch.muted) return;
    
    // If a sample file is assigned (e.g. dragged from Browser), play it
    if (ch.sampleFile.existsAsFile())
    {
        pluginHost_.playSampleFile(ch.sampleFile);
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
    for (int step = 0; step < totalSteps_; ++step)
    {
        int stepX = step * (STEP_WIDTH + STEP_GAP);
        if (step % 4 == 0 && step > 0)
            stepX += BEAT_GAP;
        
        if (relativeX >= stepX && relativeX < stepX + STEP_WIDTH)
            return step;
    }
    return -1;
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
}
