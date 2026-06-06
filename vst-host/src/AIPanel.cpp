#include "AIPanel.h"
#include "Theme.h"
#include "ChannelRack.h"
#include <thread>

namespace
{
void drawSidePanelCloseGlyph(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    area = area.reduced(5.0f);
    const float barW = juce::jmax(2.0f, area.getWidth() * 0.24f);
    auto bar = area.removeFromLeft(barW);
    g.setColour(colour);
    g.fillRoundedRectangle(bar, 1.5f);

    auto chev = area.reduced(area.getWidth() * 0.12f, area.getHeight() * 0.18f);
    const float cx = chev.getCentreX();
    const float cy = chev.getCentreY();
    const float arm = chev.getWidth() * 0.34f;
    const float spread = chev.getHeight() * 0.42f;
    juce::Path arrow;
    arrow.startNewSubPath(cx - arm * 0.55f, cy - spread);
    arrow.lineTo(cx + arm * 0.45f, cy);
    arrow.lineTo(cx - arm * 0.55f, cy + spread);
    g.strokePath(arrow, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}
} // namespace

AIPanel::AIPanel()
{
    buttons_.push_back({ "boom_bap", "Boom Bap",   {} });
    buttons_.push_back({ "hiphop",   "Hip Hop",    {} });
    buttons_.push_back({ "trap",     "Trap",       {} });
    buttons_.push_back({ "drill",    "Drill",      {} });
    buttons_.push_back({ "house",    "House",      {} });
    buttons_.push_back({ "rnb",      "R&B",        {} });
    buttons_.push_back({ "lofi",     "Lo-Fi",      {} });
    buttons_.push_back({ "rock",     "Rock",         {} });
    buttons_.push_back({ "detroit",  "Detroit Flint",{} });
    buttons_.push_back({ "afrobeat", "Afrobeat",   {} });
    buttons_.push_back({ "reggaeton","Reggaeton",  {} });
    buttons_.push_back({ "jersey",   "Jersey Club",{} });
    buttons_.push_back({ "ukg",      "UK Garage",  {} });
    buttons_.push_back({ "dnb",      "Drum & Bass",{} });
    buttons_.push_back({ "techno",   "Techno",     {} });
    buttons_.push_back({ "phonk",    "Phonk",      {} });
    buttons_.push_back({ "memphis",  "Memphis",    {} });
    buttons_.push_back({ "funk",     "Funk",       {} });
    buttons_.push_back({ "empty",    "Clear All",    {} });

    for (const auto& p : PatternsPanel::getPatternLibrary())
    {
        if (!p.artistPattern)
            continue;
        artistLibrary_.push_back(p);
        if (!artists_.contains(p.genre))
            artists_.add(p.genre);
    }
    if (!artists_.isEmpty())
        selectedArtist_ = artists_[0];
    rebuildArtistPatternRows();
    rebuildDrumPathRows();
    buildApiControls();
    updateApiControlsVisibility();

    addAssistantMessage("Hey - I can drop drum patterns straight into your Channel Rack.");
    addAssistantMessage("Use Presets for genres, or Artist for producer-inspired patterns.");
}

// ── API tab (ElevenLabs AI Voice) ──────────────────────────────────────────
void AIPanel::buildApiControls()
{
    apiKeyEditor_.setPasswordCharacter((juce::juce_wchar)0x2022);
    apiKeyEditor_.setTextToShowWhenEmpty("paste ElevenLabs xi-api-key...", Theme::zinc500);
    apiKeyEditor_.setText(ElevenLabsClient::getApiKey(), juce::dontSendNotification);
    addChildComponent(apiKeyEditor_);

    apiSaveKeyBtn_.onClick = [this] {
        ElevenLabsClient::setApiKey(apiKeyEditor_.getText().trim());
        apiSetBusy(false, "API key saved.");
    };
    addChildComponent(apiSaveKeyBtn_);

    addChildComponent(apiVoiceCombo_);
    populateApiVoiceCombo(ElevenLabsClient::defaultVoices());

    apiRefreshBtn_.onClick = [this] { apiDoRefreshVoices(); };
    addChildComponent(apiRefreshBtn_);

    apiModelCombo_.addItem("Multilingual v2 (best)", 1);
    apiModelCombo_.addItem("Turbo v2.5 (fast)", 2);
    apiModelCombo_.addItem("Flash v2.5 (fastest)", 3);
    apiModelCombo_.setSelectedId(1, juce::dontSendNotification);
    addChildComponent(apiModelCombo_);

    apiTextEditor_.setMultiLine(true);
    apiTextEditor_.setReturnKeyStartsNewLine(true);
    apiTextEditor_.setTextToShowWhenEmpty("Type the line you want spoken / sung...", Theme::zinc500);
    addChildComponent(apiTextEditor_);

    apiStabilitySlider_.setRange(0.0, 1.0, 0.01);
    apiStabilitySlider_.setValue(0.5, juce::dontSendNotification);
    apiStabilitySlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    apiStabilitySlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    addChildComponent(apiStabilitySlider_);

    apiStatusLabel_.setText("Ready.", juce::dontSendNotification);
    apiStatusLabel_.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
    apiStatusLabel_.setColour(juce::Label::textColourId, Theme::zinc500);
    addChildComponent(apiStatusLabel_);

    apiGenerateBtn_.setColour(juce::TextButton::buttonColourId, Theme::orange1);
    apiGenerateBtn_.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    apiGenerateBtn_.onClick = [this] { apiDoGenerate(); };
    addChildComponent(apiGenerateBtn_);
}

void AIPanel::populateApiVoiceCombo(const std::vector<ElevenLabsClient::Voice>& voices)
{
    apiVoices_ = voices;
    apiVoiceCombo_.clear(juce::dontSendNotification);
    for (int i = 0; i < (int)apiVoices_.size(); ++i)
        apiVoiceCombo_.addItem(apiVoices_[(size_t)i].name, i + 1);
    if (!apiVoices_.empty())
        apiVoiceCombo_.setSelectedId(1, juce::dontSendNotification);
}

void AIPanel::updateApiControlsVisibility()
{
    const bool on = (activeTab_ == 3);
    apiKeyEditor_.setVisible(on);
    apiSaveKeyBtn_.setVisible(on);
    apiVoiceCombo_.setVisible(on);
    apiRefreshBtn_.setVisible(on);
    apiModelCombo_.setVisible(on);
    apiTextEditor_.setVisible(on);
    apiStabilitySlider_.setVisible(on);
    apiStatusLabel_.setVisible(on);
    apiGenerateBtn_.setVisible(on);
}

void AIPanel::apiSetBusy(bool busy, const juce::String& status)
{
    apiGenerateBtn_.setEnabled(!busy);
    apiRefreshBtn_.setEnabled(!busy);
    apiStatusLabel_.setText(status, juce::dontSendNotification);
    apiStatusLabel_.setColour(juce::Label::textColourId, busy ? Theme::orange1 : Theme::zinc500);
}

void AIPanel::apiDoRefreshVoices()
{
    const juce::String key = apiKeyEditor_.getText().trim();
    if (key.isEmpty()) { apiSetBusy(false, "Enter an API key first."); return; }
    ElevenLabsClient::setApiKey(key);
    apiSetBusy(true, "Fetching voices...");

    juce::Component::SafePointer<AIPanel> safe(this);
    std::thread([safe, key]
    {
        juce::String err;
        auto voices = ElevenLabsClient::fetchVoices(key, err);
        juce::MessageManager::callAsync([safe, voices, err]
        {
            if (safe == nullptr) return;
            if (!voices.empty())
            {
                safe->populateApiVoiceCombo(voices);
                safe->apiSetBusy(false, "Loaded " + juce::String((int)voices.size()) + " voices.");
            }
            else
                safe->apiSetBusy(false, err.isNotEmpty() ? err : "No voices found.");
        });
    }).detach();
}

void AIPanel::apiDoGenerate()
{
    const juce::String key  = apiKeyEditor_.getText().trim();
    const juce::String text = apiTextEditor_.getText();
    const int voiceIdx      = apiVoiceCombo_.getSelectedId() - 1;

    if (key.isEmpty())         { apiSetBusy(false, "Enter an API key first."); return; }
    if (text.trim().isEmpty()) { apiSetBusy(false, "Type some text first."); return; }
    if (voiceIdx < 0 || voiceIdx >= (int)apiVoices_.size())
                               { apiSetBusy(false, "Pick a voice."); return; }

    ElevenLabsClient::setApiKey(key);

    const juce::String voiceId   = apiVoices_[(size_t)voiceIdx].id;
    const juce::String voiceName = apiVoices_[(size_t)voiceIdx].name;
    const double stability       = apiStabilitySlider_.getValue();

    juce::String modelId = "eleven_multilingual_v2";
    switch (apiModelCombo_.getSelectedId())
    {
        case 2: modelId = "eleven_turbo_v2_5"; break;
        case 3: modelId = "eleven_flash_v2_5"; break;
        default: break;
    }

    const juce::String stamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    auto outFile = ElevenLabsClient::outputDir()
                       .getChildFile("11L_" + voiceName.retainCharacters(
                           "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
                           + "_" + stamp + ".mp3");

    addUserMessage("Generate voice: \"" + text.substring(0, 60) + (text.length() > 60 ? "..." : "") + "\"");
    apiSetBusy(true, "Generating with " + voiceName + "...");

    juce::Component::SafePointer<AIPanel> safe(this);
    std::thread([safe, key, voiceId, text, modelId, stability, outFile, voiceName]
    {
        juce::String err;
        const bool ok = ElevenLabsClient::textToSpeech(
            key, voiceId, text, modelId, stability, 0.75, outFile, err);

        juce::MessageManager::callAsync([safe, ok, err, outFile, voiceName]
        {
            if (safe == nullptr) return;
            if (ok)
            {
                safe->apiSetBusy(false, "Done -> added to Channel Rack.");
                safe->addAssistantMessage("Generated voice with " + voiceName + " and added it to the Channel Rack.");
                if (safe->onAudioGenerated)
                    safe->onAudioGenerated(outFile);
            }
            else
            {
                safe->apiSetBusy(false, err.isNotEmpty() ? err : "Generation failed.");
                safe->addAssistantMessage("Voice generation failed: " + (err.isNotEmpty() ? err : juce::String("unknown error")));
            }
        });
    }).detach();
}

void AIPanel::layoutApiControls(juce::Rectangle<int> area)
{
    area.reduce(2, 2);
    area.removeFromTop(18); // section header drawn in drawApiBrowser
    const int rowH = 26, gap = 7, labelW = 56;

    auto row = [&](int h) { auto r = area.removeFromTop(h); area.removeFromTop(gap); return r; };

    // API key
    {
        auto r = row(rowH);
        r.removeFromLeft(labelW);
        apiSaveKeyBtn_.setBounds(r.removeFromRight(64));
        r.removeFromRight(6);
        apiKeyEditor_.setBounds(r);
    }
    // Voice
    {
        auto r = row(rowH);
        r.removeFromLeft(labelW);
        apiRefreshBtn_.setBounds(r.removeFromRight(72));
        r.removeFromRight(6);
        apiVoiceCombo_.setBounds(r);
    }
    // Model
    {
        auto r = row(rowH);
        r.removeFromLeft(labelW);
        apiModelCombo_.setBounds(r);
    }
    // Stability
    {
        auto r = row(rowH);
        r.removeFromLeft(labelW);
        apiStabilitySlider_.setBounds(r);
    }
    // Generate button + status pinned to the bottom; text editor fills the rest.
    auto bottom = area.removeFromBottom(rowH + 4);
    apiGenerateBtn_.setBounds(bottom);
    area.removeFromBottom(gap);
    apiStatusLabel_.setBounds(area.removeFromBottom(rowH));
    area.removeFromBottom(4);
    area.removeFromTop(16); // "TEXT" label drawn in drawApiBrowser
    apiTextEditor_.setBounds(area.reduced(0, 2));
}

void AIPanel::drawApiBrowser(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("ELEVENLABS  -  AI VOICE / TEXT-TO-SPEECH",
               area.getX() + 4, area.getY(), area.getWidth() - 8, 18,
               juce::Justification::centredLeft);

    // Small inline labels next to each control row.
    auto rowArea = area;
    rowArea.reduce(2, 2);
    rowArea.removeFromTop(18);
    const int rowH = 26, gap = 7, labelW = 56;
    auto label = [&](const juce::String& t, int h) {
        auto r = rowArea.removeFromTop(h); rowArea.removeFromTop(gap);
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
        g.drawText(t, r.removeFromLeft(labelW), juce::Justification::centredLeft);
    };
    label("Key", rowH);
    label("Voice", rowH);
    label("Model", rowH);
    label("Stable", rowH);

    // "TEXT" header sits above the multi-line editor (see layoutApiControls).
    auto bottom = rowArea;
    bottom.removeFromBottom(rowH + 4);            // generate button
    bottom.removeFromBottom(gap);
    bottom.removeFromBottom(rowH);                // status
    bottom.removeFromBottom(4);
    auto textHeader = bottom.removeFromTop(16);
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("TEXT", textHeader, juce::Justification::centredLeft);
}

void AIPanel::rebuildDrumPathRows()
{
    const int prevScroll = drumPathScrollY_;
    drumPathRows_.clear();
    for (const auto& c : ChannelRack::getDrumPathConfigs())
    {
        DrumPathRow r;
        r.id            = c.id;
        r.label         = c.label;
        r.folders       = c.folders;
        r.mode          = (int)c.mode;
        r.specificIndex = c.specificIndex;
        drumPathRows_.push_back(std::move(r));
    }
    drumPathScrollY_ = prevScroll;
}

void AIPanel::addUserMessage(const juce::String& text)
{
    chat_.push_back({ true, text });
    repaint();
}

void AIPanel::addAssistantMessage(const juce::String& text)
{
    chat_.push_back({ false, text });
    repaint();
}

void AIPanel::rebuildArtistPatternRows()
{
    artistPatternRows_.clear();
    for (const auto& p : artistLibrary_)
    {
        if (p.genre == selectedArtist_)
            artistPatternRows_.push_back({ p, {} });
    }
    artistPatternScrollY_ = 0;
}

std::vector<int> AIPanel::visibleArtistPatternIndices() const
{
    std::vector<int> ids;
    for (int i = 0; i < (int)artistPatternRows_.size(); ++i)
        ids.push_back(i);
    return ids;
}

void AIPanel::drawTabs(juce::Graphics& g, juce::Rectangle<int> tabsArea)
{
    auto tabs = tabsArea;
    presetsTabRect_  = tabs.removeFromLeft(84).reduced(0, 4);
    artistTabRect_   = tabs.removeFromLeft(78).reduced(6, 4);
    drumPathTabRect_ = tabs.removeFromLeft(96).reduced(6, 4);
    apiTabRect_      = tabs.removeFromLeft(62).reduced(6, 4);

    auto drawTab = [&](juce::Rectangle<int> r, const juce::String& label, bool selected)
    {
        g.setColour(selected ? Theme::orange1.withAlpha(0.95f) : juce::Colour(0xff1c1c20));
        g.fillRoundedRectangle(r.toFloat(), 5.0f);
        g.setColour(selected ? juce::Colours::black.withAlpha(0.7f) : juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 5.0f, 1.0f);
        g.setColour(selected ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(label, r, juce::Justification::centred);
    };

    drawTab(presetsTabRect_,  "PRESETS",   activeTab_ == 0);
    drawTab(artistTabRect_,   "ARTIST",    activeTab_ == 1);
    drawTab(drumPathTabRect_, "DRUM PATH", activeTab_ == 2);
    drawTab(apiTabRect_,      "API",       activeTab_ == 3);
}

void AIPanel::drawPresetBrowser(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("DRUM PATTERN PRESETS",
               area.getX() + 4, area.getY(), area.getWidth() - 8, 18,
               juce::Justification::centredLeft);

    auto gridArea = area.withTrimmedTop(20).reduced(BTN_GAP, BTN_GAP);
    layoutPresetButtons(gridArea);

    for (auto& b : buttons_)
    {
        auto fr = b.rect.toFloat();
        juce::ColourGradient grad(juce::Colour(0xff2e2e34), 0.0f, fr.getY(),
                                  juce::Colour(0xff141416), 0.0f, fr.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fr, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawHorizontalLine((int)fr.getY() + 1, fr.getX() + 4, fr.getRight() - 4);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(fr, 4.0f, 1.0f);

        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
        g.drawText(b.label, b.rect, juce::Justification::centred);
    }
}

void AIPanel::drawArtistBrowser(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto left = area.removeFromLeft(ARTIST_LIST_W);
    auto right = area.withTrimmedLeft(8);

    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("ARTISTS", left.removeFromTop(18), juce::Justification::centredLeft);
    g.drawText("ARTIST PATTERNS", right.removeFromTop(18), juce::Justification::centredLeft);

    auto artistList = left.reduced(0, 2);
    for (int i = 0; i < artists_.size() && i < 24; ++i)
    {
        auto r = artistList.removeFromTop(26).reduced(0, 2);
        artistRects_[i] = r;
        const bool selected = artists_[i] == selectedArtist_;
        g.setColour(selected ? Theme::orange1.withAlpha(0.95f) : juce::Colour(0xff1c1c20));
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
        g.setColour(selected ? juce::Colours::black.withAlpha(0.65f) : juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 4.0f, 1.0f);
        g.setColour(selected ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
        g.drawText(artists_[i], r.reduced(6, 0), juce::Justification::centredLeft, true);
    }

    auto listArea = right.reduced(0, 2);
    g.setColour(juce::Colour(0xff050507));
    g.fillRoundedRectangle(listArea.toFloat(), 5.0f);
    g.setColour(juce::Colour(0xff222226));
    g.drawRoundedRectangle(listArea.toFloat(), 5.0f, 1.0f);

    g.saveState();
    g.reduceClipRegion(listArea);

    int y = listArea.getY() - artistPatternScrollY_;
    const auto visible = visibleArtistPatternIndices();
    for (int n = 0; n < (int)visible.size(); ++n)
    {
        auto& row = artistPatternRows_[(size_t)visible[(size_t)n]];
        auto r = juce::Rectangle<int>(listArea.getX() + 6, y + 4,
                                      listArea.getWidth() - 12, ARTIST_PATTERN_ROW_H - 6);
        row.rect = r;

        g.setColour(juce::Colour(0xff19191d));
        g.fillRoundedRectangle(r.toFloat(), 5.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 5.0f, 1.0f);

        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
        g.drawText(row.def.title, r.getX() + 10, r.getY() + 6, r.getWidth() - 90, 16,
                   juce::Justification::centredLeft);
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
        g.drawText(row.def.feel, r.getX() + 10, r.getY() + 24, r.getWidth() - 90, 14,
                   juce::Justification::centredLeft, true);
        g.setColour(Theme::orange2);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(juce::String(row.def.bpm) + " BPM", r.getRight() - 78, r.getCentreY() - 9, 70, 18,
                   juce::Justification::centredRight);

        y += ARTIST_PATTERN_ROW_H;
    }

    g.restoreState();
}

void AIPanel::drawDrumPathBrowser(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("DRUM KITS PER GENRE  -  click MODE to switch, + to add a folder, X to remove",
               area.getX() + 4, area.getY(), area.getWidth() - 8, 18,
               juce::Justification::centredLeft);

    auto listArea = area.withTrimmedTop(20).reduced(0, 2);
    g.setColour(juce::Colour(0xff050507));
    g.fillRoundedRectangle(listArea.toFloat(), 5.0f);
    g.setColour(juce::Colour(0xff222226));
    g.drawRoundedRectangle(listArea.toFloat(), 5.0f, 1.0f);

    g.saveState();
    g.reduceClipRegion(listArea);

    auto modeLabel = [](int mode, const juce::StringArray& folders, int specIdx) -> juce::String {
        if (mode == 0) return juce::String("Mode: All");
        if (mode == 1) return juce::String("Mode: Randomize");
        if (folders.isEmpty()) return juce::String("Mode: Specific");
        int idx = juce::jlimit(0, folders.size() - 1, specIdx);
        juce::String name = juce::File(folders[idx]).getFileName();
        if (name.length() > 28) name = name.substring(0, 26) + "...";
        return "Specific: " + name;
    };

    int y = listArea.getY() + 4 - drumPathScrollY_;
    for (auto& row : drumPathRows_)
    {
        const int rowH = DRUM_PATH_HEADER_H
                       + juce::jmax(1, row.folders.size()) * DRUM_PATH_KIT_H
                       + DRUM_PATH_FOOTER_H
                       + 8;
        auto r = juce::Rectangle<int>(listArea.getX() + 6, y,
                                      listArea.getWidth() - 12, rowH);
        row.rect = r;

        const bool linked = row.folders.size() > 0;
        g.setColour(linked ? juce::Colour(0xff19191d) : juce::Colour(0xff141417));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.0f);

        // Header: label on left, mode button on right
        auto header = r.removeFromTop(DRUM_PATH_HEADER_H).reduced(8, 4);
        g.setColour(linked ? Theme::orange2 : Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
        g.drawText(row.label, header.removeFromLeft(140), juce::Justification::centredLeft);

        auto modeR = header.removeFromRight(190);
        row.modeBtnRect = modeR;
        g.setColour(juce::Colour(0xff242429));
        g.fillRoundedRectangle(modeR.toFloat(), 4.0f);
        g.setColour(Theme::orange1.withAlpha(0.7f));
        g.drawRoundedRectangle(modeR.toFloat(), 4.0f, 1.0f);
        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(modeLabel(row.mode, row.folders, row.specificIndex),
                   modeR.reduced(6, 0), juce::Justification::centredLeft, true);

        // Kit list rows
        row.kitRowRects.clear();
        row.kitRemoveRects.clear();
        if (row.folders.isEmpty())
        {
            auto er = r.removeFromTop(DRUM_PATH_KIT_H).reduced(10, 2);
            g.setColour(Theme::zinc600);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("italic"));
            g.drawText("(no kits linked - click + Add Drum Kit below)", er,
                       juce::Justification::centredLeft);
        }
        else
        {
            for (int i = 0; i < row.folders.size(); ++i)
            {
                auto kr = r.removeFromTop(DRUM_PATH_KIT_H).reduced(8, 1);
                row.kitRowRects.push_back(kr);

                // Highlight the active "Specific" kit
                const bool isActive = (row.mode == 2 && i == row.specificIndex);
                g.setColour(isActive ? Theme::orange1.withAlpha(0.18f) : juce::Colour(0xff121215));
                g.fillRoundedRectangle(kr.toFloat(), 3.0f);

                // [X] remove button on the right
                auto xR = kr.removeFromRight(20).reduced(2, 2);
                row.kitRemoveRects.push_back(xR);
                g.setColour(juce::Colour(0xff3a1d1d));
                g.fillRoundedRectangle(xR.toFloat(), 3.0f);
                g.setColour(juce::Colours::white.withAlpha(0.8f));
                g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
                g.drawText("X", xR, juce::Justification::centred);

                // Folder name (full file name on top) and short parent
                juce::File f(row.folders[i]);
                g.setColour(isActive ? Theme::orange2 : Theme::zinc200);
                g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f).withStyle("Bold"));
                g.drawText(f.getFileName(),
                           kr.getX() + 6, kr.getY(),
                           kr.getWidth() - 8, kr.getHeight(),
                           juce::Justification::centredLeft, true);
            }
        }

        // Footer: + Add Drum Kit button
        auto footer = r.removeFromTop(DRUM_PATH_FOOTER_H).reduced(8, 4);
        auto addR = footer.removeFromLeft(140);
        row.addBtnRect = addR;
        g.setColour(juce::Colour(0xff1f2a1d));
        g.fillRoundedRectangle(addR.toFloat(), 4.0f);
        g.setColour(juce::Colour(0xff3b6033));
        g.drawRoundedRectangle(addR.toFloat(), 4.0f, 1.0f);
        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText("+ Add Drum Kit", addR, juce::Justification::centred);

        y += rowH + 6;
    }

    g.restoreState();
}

void AIPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    {
        juce::DropShadow shadow(juce::Colours::black.withAlpha(0.65f), 24, { 0, 8 });
        juce::Path p;
        p.addRoundedRectangle(bounds.reduced(2.0f), 10.0f);
        shadow.drawForPath(g, p);
    }

    juce::ColourGradient chassis(juce::Colour(0xff1a1a1d), 0.0f, 0.0f,
                                 juce::Colour(0xff0c0c0e), 0.0f, bounds.getHeight(), false);
    g.setGradientFill(chassis);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colour(0xff2a2a2e));
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);

    auto headerRect = juce::Rectangle<int>(0, 0, getWidth(), HEADER_H);
    juce::ColourGradient hg(Theme::orange1, 0.0f, 0.0f,
                              Theme::orange3, 0.0f, (float)HEADER_H, false);
    g.setGradientFill(hg);
    g.fillRect(headerRect.withTrimmedBottom(1));
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(HEADER_H - 1, 0.0f, (float)getWidth());

    sidePanelCloseBtnRect_ = { 0, 0, HEADER_H, HEADER_H };
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRect(sidePanelCloseBtnRect_.withTrimmedTop(4).withTrimmedBottom(4)
                   .withWidth(1).translated(sidePanelCloseBtnRect_.getRight() - 1, 0));
    drawSidePanelCloseIcon(g, sidePanelCloseBtnRect_);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
    g.drawText("AI Assistant", sidePanelCloseBtnRect_.getRight() + 6, 0,
               getWidth() - HEADER_H - sidePanelCloseBtnRect_.getRight() - 10, HEADER_H,
               juce::Justification::centredLeft);

    closeBtnRect_ = { getWidth() - HEADER_H, 0, HEADER_H, HEADER_H };
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(15.0f).withStyle("Bold"));
    g.drawText("X", closeBtnRect_, juce::Justification::centred);

    int btnRows  = (int)((buttons_.size() + BTN_COLS - 1) / BTN_COLS);
    int presetAreaH = btnRows * (BTN_H + BTN_GAP) + BTN_GAP * 2 + 48;
    int artistAreaH = juce::jmax(220, (int)artistPatternRows_.size() * ARTIST_PATTERN_ROW_H + 56);
    int drumPathTotal = 0;
    for (const auto& r : drumPathRows_)
        drumPathTotal += DRUM_PATH_HEADER_H
                       + juce::jmax(1, r.folders.size()) * DRUM_PATH_KIT_H
                       + DRUM_PATH_FOOTER_H + 14;
    int drumPathAreaH = juce::jmax(220, drumPathTotal + 40);
    int apiAreaH = 320;
    int browserH = (activeTab_ == 0) ? presetAreaH
                  : (activeTab_ == 1) ? artistAreaH
                  : (activeTab_ == 2) ? drumPathAreaH : apiAreaH;
    browserH = juce::jmin(browserH, getHeight() - HEADER_H - FOOTER_H - TAB_BAR_H - 96);

    auto contentArea = juce::Rectangle<int>(0, HEADER_H, getWidth(),
                                              getHeight() - HEADER_H - FOOTER_H);
    auto browserBlock = contentArea.removeFromBottom(browserH + TAB_BAR_H);
    auto chatArea = contentArea.reduced(CHAT_PAD);

    {
        auto cb = chatArea.toFloat().expanded(4.0f);
        g.setColour(juce::Colour(0xff050507));
        g.fillRoundedRectangle(cb, 6.0f);
        g.setColour(juce::Colour(0xff222226));
        g.drawRoundedRectangle(cb, 6.0f, 1.0f);
    }
    drawChat(g, chatArea);

    auto tabsArea = browserBlock.removeFromTop(TAB_BAR_H).reduced(8, 4);
    drawTabs(g, tabsArea);

    browserAreaRect_ = browserBlock.reduced(8, 0);
    if (activeTab_ == 0)
        drawPresetBrowser(g, browserAreaRect_);
    else if (activeTab_ == 1)
        drawArtistBrowser(g, browserAreaRect_);
    else if (activeTab_ == 2)
        drawDrumPathBrowser(g, browserAreaRect_);
    else
    {
        drawApiBrowser(g, browserAreaRect_);
        layoutApiControls(browserAreaRect_);
    }

    g.setColour(Theme::zinc600);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
    const char* footerText =
        (activeTab_ == 1) ? "Artist patterns use the same library as the Patterns panel." :
        (activeTab_ == 2) ? "Folders mapped to each genre preset. Empty = no kit linked." :
        (activeTab_ == 3) ? "ElevenLabs text-to-speech. Output is added as a Channel Rack sample."
                          : "Genre presets drop a full kit into the Channel Rack.";
    g.drawText(footerText,
               0, getHeight() - FOOTER_H, getWidth(), FOOTER_H,
               juce::Justification::centred);
}

void AIPanel::layoutPresetButtons(juce::Rectangle<int> area)
{
    int cols = BTN_COLS;
    int colW = (area.getWidth() - (cols - 1) * BTN_GAP) / cols;
    int x = area.getX();
    int y = area.getY();
    int col = 0;

    for (auto& b : buttons_)
    {
        b.rect = juce::Rectangle<int>(x, y, colW, BTN_H);
        if (++col >= cols)
        {
            col = 0;
            x = area.getX();
            y += BTN_H + BTN_GAP;
        }
        else
        {
            x += colW + BTN_GAP;
        }
    }
}

void AIPanel::drawChat(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);

    juce::Font userFont = juce::Font(juce::FontOptions().withName("Segoe UI").withHeight(11.5f));
    juce::Font asstFont = juce::Font(juce::FontOptions().withName("Segoe UI").withHeight(11.5f));

    int y = area.getBottom() - 6;
    for (auto it = chat_.rbegin(); it != chat_.rend(); ++it)
    {
        auto& line = *it;
        const auto& font = line.fromUser ? userFont : asstFont;
        int bubbleW  = juce::jmin(area.getWidth() - 24, 320);
        int textW    = bubbleW - 16;

        juce::AttributedString as;
        as.setJustification(juce::Justification::topLeft);
        as.append(line.text, font, line.fromUser ? juce::Colour(0xff111111) : Theme::zinc200);

        juce::TextLayout tl;
        tl.createLayout(as, (float)textW);
        int bubbleH = (int)std::ceil(tl.getHeight()) + 12;

        int bubbleX = line.fromUser ? area.getRight() - bubbleW - 4 : area.getX() + 4;
        int bubbleY = y - bubbleH;
        if (bubbleY < area.getY()) break;

        juce::Rectangle<float> bubble((float)bubbleX, (float)bubbleY, (float)bubbleW, (float)bubbleH);
        if (line.fromUser)
        {
            juce::ColourGradient g2(Theme::orange1, 0.0f, bubble.getY(),
                                      Theme::orange3, 0.0f, bubble.getBottom(), false);
            g.setGradientFill(g2);
        }
        else
        {
            g.setColour(juce::Colour(0xff1c1c20));
        }
        g.fillRoundedRectangle(bubble, 6.0f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(bubble, 6.0f, 1.0f);

        tl.draw(g, bubble.reduced(8.0f, 6.0f));
        y = bubbleY - 6;
    }

    g.restoreState();
}

void AIPanel::drawSidePanelCloseIcon(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    drawSidePanelCloseGlyph(g, bounds.toFloat(), juce::Colours::white.withAlpha(0.95f));
}

void AIPanel::resized() {}

void AIPanel::mouseDown(const juce::MouseEvent& e)
{
    isDraggingPanel_ = false;

    if (sidePanelCloseBtnRect_.contains(e.x, e.y) || closeBtnRect_.contains(e.x, e.y))
    {
        if (onClose) onClose();
        return;
    }

    if (e.y < HEADER_H)
    {
        dragger_.startDraggingComponent(this, e);
        isDraggingPanel_ = true;
        return;
    }

    if (presetsTabRect_.contains(e.x, e.y) ||
        artistTabRect_.contains(e.x, e.y) ||
        drumPathTabRect_.contains(e.x, e.y) ||
        apiTabRect_.contains(e.x, e.y))
    {
        if (apiTabRect_.contains(e.x, e.y))           activeTab_ = 3;
        else if (drumPathTabRect_.contains(e.x, e.y)) activeTab_ = 2;
        else if (artistTabRect_.contains(e.x, e.y))   activeTab_ = 1;
        else                                           activeTab_ = 0;
        updateApiControlsVisibility();
        repaint();
        return;
    }

    if (activeTab_ == 2)
    {
        for (size_t ri = 0; ri < drumPathRows_.size(); ++ri)
        {
            auto& row = drumPathRows_[ri];

            // [X] remove a kit?
            for (size_t i = 0; i < row.kitRemoveRects.size(); ++i)
            {
                if (row.kitRemoveRects[i].contains(e.x, e.y))
                {
                    ChannelRack::removeDrumPathFolder(row.id, (int)i);
                    rebuildDrumPathRows();
                    repaint();
                    return;
                }
            }

            // Mode button → popup menu
            if (row.modeBtnRect.contains(e.x, e.y))
            {
                juce::PopupMenu menu;
                menu.addItem(1, "All kits (combine samples from every linked folder)",
                             true, row.mode == 0);
                menu.addItem(2, "Randomize (pick one random folder per load)",
                             true, row.mode == 1);
                if (row.folders.size() > 0)
                {
                    menu.addSeparator();
                    menu.addSectionHeader("Specific kit:");
                    for (int k = 0; k < row.folders.size(); ++k)
                    {
                        juce::String name = juce::File(row.folders[k]).getFileName();
                        menu.addItem(100 + k, name, true,
                                     row.mode == 2 && row.specificIndex == k);
                    }
                }
                const juce::String id = row.id;
                menu.showMenuAsync(juce::PopupMenu::Options{}
                    .withTargetComponent(this)
                    .withTargetScreenArea(localAreaToGlobal(row.modeBtnRect)),
                    [this, id](int result) {
                        if (result <= 0) return;
                        if (result == 1)
                            ChannelRack::setDrumPathMode(id, ChannelRack::DrumPathMode::All, 0);
                        else if (result == 2)
                            ChannelRack::setDrumPathMode(id, ChannelRack::DrumPathMode::Randomize, 0);
                        else if (result >= 100)
                            ChannelRack::setDrumPathMode(id, ChannelRack::DrumPathMode::Specific,
                                                         result - 100);
                        rebuildDrumPathRows();
                        repaint();
                    });
                return;
            }

            // + Add Drum Kit button → folder picker
            if (row.addBtnRect.contains(e.x, e.y))
            {
                const juce::String id = row.id;
                auto chooser = std::make_shared<juce::FileChooser>(
                    "Choose a drum-kit folder for " + row.label,
                    juce::File("E:/!Storage"), juce::String());
                chooser->launchAsync(juce::FileBrowserComponent::openMode
                                   | juce::FileBrowserComponent::canSelectDirectories,
                    [this, id, chooser](const juce::FileChooser& fc)
                    {
                        auto picked = fc.getResult();
                        if (picked.isDirectory())
                        {
                            ChannelRack::addDrumPathFolder(id, picked.getFullPathName());
                            rebuildDrumPathRows();
                            repaint();
                        }
                    });
                return;
            }
        }
        return;
    }

    if (activeTab_ == 1)
    {
        for (int i = 0; i < artists_.size() && i < 24; ++i)
        {
            if (artistRects_[i].contains(e.x, e.y))
            {
                selectedArtist_ = artists_[i];
                rebuildArtistPatternRows();
                repaint();
                return;
            }
        }

        for (auto& row : artistPatternRows_)
        {
            if (row.rect.contains(e.x, e.y))
            {
                addUserMessage("Load " + row.def.genre + " - " + row.def.title);
                if (onPatternVariant)
                    onPatternVariant(row.def);
                addAssistantMessage("Done! Loaded " + row.def.title
                    + " (" + juce::String(row.def.bpm) + " BPM).");
                return;
            }
        }
        return;
    }

    for (auto& b : buttons_)
    {
        if (b.rect.contains(e.x, e.y))
        {
            if (e.mods.isMiddleButtonDown() && b.id != "empty")
            {
                auto variants = PatternsPanel::getPatternsForPreset(b.id);
                if (variants.empty())
                {
                    addAssistantMessage("No saved drum patterns found for " + b.label + ".");
                    return;
                }

                auto& patternCursor = middleClickPatternCursor_[b.id.toStdString()];
                if (patternCursor <= 0 && variants.size() > 1)
                    patternCursor = 1;

                const int index = patternCursor % (int)variants.size();
                patternCursor = (index + 1) % (int)variants.size();

                const auto& pattern = variants[(size_t)index];
                addUserMessage("Change " + b.label + " drum pattern");
                if (onCyclePatternOnly)
                    onCyclePatternOnly(pattern);
                else if (onPatternVariant)
                    onPatternVariant(pattern);
                addAssistantMessage("Done! Changed to " + pattern.title + " without changing drum sounds.");
                return;
            }

            if (e.mods.isRightButtonDown() && b.id != "empty")
            {
                auto variants = PatternsPanel::getPatternsForPreset(b.id);
                if (variants.empty())
                    return;

                juce::PopupMenu menu;
                for (int i = 0; i < (int)variants.size(); ++i)
                {
                    const auto& pat = variants[(size_t)i];
                    juce::String label = pat.title;
                    if (pat.feel.isNotEmpty())
                        label += " - " + pat.feel;
                    menu.addItem(i + 1, label);
                }

                menu.showMenuAsync(juce::PopupMenu::Options{}
                    .withTargetComponent(this)
                    .withTargetScreenArea(localAreaToGlobal(b.rect)),
                    [this, variants, label = b.label](int result) {
                        if (result <= 0 || result > (int)variants.size())
                            return;

                        const auto& pattern = variants[(size_t)result - 1];
                        addUserMessage("Change " + label + " to " + pattern.title);
                        if (onPatternVariant)
                            onPatternVariant(pattern);
                        addAssistantMessage("Done! Changed to " + pattern.title + ".");
                    });
                return;
            }

            addUserMessage("Make a " + b.label + " pattern");
            if (onPreset) onPreset(b.id, b.label);
            if (b.id == "empty")
                addAssistantMessage("Cleared all drum steps. Fresh canvas.");
            else
                addAssistantMessage("Done! Loaded a " + b.label + " drum pattern.");
            return;
        }
    }
}

void AIPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingPanel_)
        dragger_.dragComponent(this, e, nullptr);
}

void AIPanel::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (activeTab_ == 1 && !artistPatternRows_.empty())
    {
        auto listArea = browserAreaRect_;
        listArea.removeFromLeft(ARTIST_LIST_W + 8);
        listArea.removeFromTop(18);

        if (!listArea.contains(e.getPosition()))
        {
            juce::Component::mouseWheelMove(e, wheel);
            return;
        }

        const int contentH = (int)artistPatternRows_.size() * ARTIST_PATTERN_ROW_H;
        const int maxScroll = juce::jmax(0, contentH - listArea.getHeight());
        artistPatternScrollY_ = juce::jlimit(0, maxScroll,
            artistPatternScrollY_ - (int)std::round(wheel.deltaY * 140.0f));
        repaint();
        return;
    }

    if (activeTab_ == 2 && !drumPathRows_.empty())
    {
        auto listArea = browserAreaRect_;
        listArea.removeFromTop(20);

        if (!listArea.contains(e.getPosition()))
        {
            juce::Component::mouseWheelMove(e, wheel);
            return;
        }

        int contentH = 0;
        for (const auto& r : drumPathRows_)
            contentH += DRUM_PATH_HEADER_H
                      + juce::jmax(1, r.folders.size()) * DRUM_PATH_KIT_H
                      + DRUM_PATH_FOOTER_H + 14;
        const int maxScroll = juce::jmax(0, contentH - listArea.getHeight());
        drumPathScrollY_ = juce::jlimit(0, maxScroll,
            drumPathScrollY_ - (int)std::round(wheel.deltaY * 140.0f));
        repaint();
        return;
    }

    juce::Component::mouseWheelMove(e, wheel);
}
