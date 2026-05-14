#include "MainComponent.h"
#include "Theme.h"
#include "PianoRoll.h"
#ifdef _WIN32
#include <windows.h>
#endif

MainComponent::MainComponent(PluginHost& pluginHost, AudioEngine& audioEngine)
    : pluginHost_(pluginHost), audioEngine_(audioEngine)
{
    transportBar_ = std::make_unique<TransportBar>(pluginHost_);
    channelRack_ = std::make_unique<ChannelRack>(pluginHost_);
    mixer_ = std::make_unique<Mixer>(pluginHost_);
    browser_ = std::make_unique<Browser>(pluginHost_);
    playlist_ = std::make_unique<Playlist>(pluginHost_);
    bottomDock_ = std::make_unique<BottomDock>();
    pianoRoll_ = std::make_unique<PianoRoll>(pluginHost_);
    aiPanel_ = std::make_unique<AIPanel>();
    videoPanel_ = std::make_unique<VideoPanel>();
    
    addAndMakeVisible(*transportBar_);
    addAndMakeVisible(*playlist_);
    addAndMakeVisible(*pianoRoll_);
    addAndMakeVisible(*mixer_);
    addAndMakeVisible(*channelRack_);
    addAndMakeVisible(*bottomDock_);
    addAndMakeVisible(*browser_);
    addChildComponent(*aiPanel_);     // hidden until user clicks AI button
    addChildComponent(*videoPanel_);  // hidden until user clicks VIDEO button
    videoPanel_->onClose = [this](){ videoPanel_->setVisible(false); };
    
    // Default view: Playlist
    pianoRoll_->setVisible(false);
    mixer_->setVisible(false);
    
    // Channel rack floats on top
    channelRack_->toFront(false);
    
    // Connect channel click to open Piano Roll
    channelRack_->onChannelClicked = [this](int channelIndex) {
        setCenterView(CenterView::PianoRoll);
        // Load channel notes into Piano Roll
        auto& channels = channelRack_->getChannels();
        if (channelIndex >= 0 && channelIndex < (int)channels.size()) {
            pianoRoll_->setChannelName(channels[channelIndex].name);
            // Convert ChannelRack::Channel::Note to PianoRollNote
            std::vector<PianoRollNote> pianoNotes;
            for (const auto& n : channels[channelIndex].pianoRollNotes)
                pianoNotes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
            pianoRoll_->setNotes(pianoNotes);
        }
    };
    
    // Wire up Channel Rack header buttons (placeholder functionality)
    channelRack_->onAddChannel = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Add Channel", "Add new channel (placeholder)");
    };
    channelRack_->onToggle16_32 = [this](){
        // 16/32 toggle already handled in ChannelRack, just visual feedback
    };
    channelRack_->onStepGraph = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Step/Graph", "Toggle Step/Graph view (placeholder)");
    };
    channelRack_->onAddPattern = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Add Pattern", "Create new pattern (placeholder)");
    };
    channelRack_->onAddInstrument = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Add Instrument", "Add new instrument channel (placeholder)");
    };
    
    // Connect Piano Roll notes changed to save back to channel + sync steps
    pianoRoll_->onNotesChanged = [this]() {
        int selectedChannel = channelRack_->getSelectedChannel();
        auto& channels = channelRack_->getChannels();
        if (selectedChannel >= 0 && selectedChannel < (int)channels.size()) {
            auto& ch = channels[selectedChannel];
            
            // Save piano-roll notes back to the channel
            auto pianoNotes = pianoRoll_->getNotes();
            ch.pianoRollNotes.clear();
            for (const auto& n : pianoNotes)
                ch.pianoRollNotes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
            
            // Re-derive step grid from piano-roll notes (a step is active if any note starts there)
            std::fill(ch.steps.begin(), ch.steps.end(), false);
            for (const auto& n : ch.pianoRollNotes) {
                if (n.startStep >= 0 && n.startStep < (int)ch.steps.size())
                    ch.steps[n.startStep] = true;
            }
            channelRack_->repaint();
        }
    };
    
    // Drive the Piano Roll and Playlist playheads from the channel rack's step clock.
    channelRack_->onPlayheadTick = [this](int step, bool playing) {
        double bpm = transportBar_->getBPM();
        pianoRoll_->setPlayhead(playing ? step : -1, playing, bpm);
        playlist_->setPlayhead(playing ? step : -1, playing, bpm);
    };

    // Feed Playlist the live channel-rack step grid so it can render
    // a "what's inside" preview inside each Pattern clip.
    playlist_->getPatternGrid = [this]() -> std::vector<std::vector<bool>> {
        std::vector<std::vector<bool>> grid;
        if (!channelRack_) return grid;
        for (const auto& ch : channelRack_->getChannels())
            grid.push_back(ch.steps);
        return grid;
    };

    // When the channel rack toggles a step, push the change to piano roll if shown
    channelRack_->onChannelDataChanged = [this](int channelIdx) {
        auto& channels = channelRack_->getChannels();
        if (channelRack_->getSelectedChannel() == channelIdx
            && channelIdx >= 0 && channelIdx < (int)channels.size())
        {
            std::vector<PianoRollNote> notes;
            for (const auto& n : channels[channelIdx].pianoRollNotes)
                notes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
            pianoRoll_->setNotes(notes);
        }
    };
    
    // Connect transport button events to view switching
    transportBar_->onPianoToggle    = [this](){ setCenterView(CenterView::PianoRoll); };
    transportBar_->onMixerToggle    = [this](){ setCenterView(CenterView::Mixer); };
    transportBar_->onPlaylistToggle = [this](){ setCenterView(CenterView::Playlist); };

    // Pattern-name sync: keep the Playlist's left strip label in step with the dropdown
    auto syncPatternName = [this]() {
        auto names = transportBar_->getPatterns();
        int idx = transportBar_->getCurrentPattern();
        if (idx >= 0 && idx < names.size())
            playlist_->setCurrentPatternName(names[idx]);
    };
    transportBar_->onPatternSelected = [syncPatternName](int){ syncPatternName(); };
    transportBar_->onPatternAdded    = [syncPatternName](juce::String){ syncPatternName(); };
    
    // Wire up SAVE, OPEN, EXPORT, LOG buttons (placeholder functionality)
    transportBar_->onSave = [this](){
        if (currentProjectFile_.existsAsFile())
            saveProject(currentProjectFile_);
        else
            saveProjectAs();
    };
    transportBar_->onOpen = [this](){ openProjectFile(); };
    transportBar_->onExport = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Export", "Export audio dialog (placeholder)");
    };
    transportBar_->onLog = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Log", "Log window (placeholder)");
    };
    
    // Wire up BottomDock Quick Tools buttons
    bottomDock_->onMixer = [this](){
        setCenterView(CenterView::Mixer);
    };
    bottomDock_->onPianoRoll = [this](){
        setCenterView(CenterView::PianoRoll);
    };
    bottomDock_->onChannelRack = [this](){
        // Toggle: hide the channel rack if it's already visible & on top, otherwise show & raise it.
        bool wantShow = !channelRack_->isVisible();
        channelRack_->setVisible(wantShow);
        if (wantShow) channelRack_->toFront(false);
    };
    bottomDock_->onPlugins = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Plugins", "Plugin browser (placeholder)");
    };
    bottomDock_->onVideo = [this](){
        bool show = !videoPanel_->isVisible();
        videoPanel_->setVisible(show);
        if (show) {
            int pw = juce::jmin(900, getWidth()  - 80);
            int ph = juce::jmin(620, getHeight() - 100);
            videoPanel_->setBounds((getWidth() - pw) / 2, (getHeight() - ph) / 2, pw, ph);
            videoPanel_->toFront(true);
        }
    };
    bottomDock_->onAI = [this](){
        bool show = !aiPanel_->isVisible();
        aiPanel_->setVisible(show);
        if (show) {
            // Centre the panel and bring it to the front.
            int pw = juce::jmin(460, getWidth() - 80);
            int ph = juce::jmin(560, getHeight() - 80);
            aiPanel_->setBounds((getWidth() - pw) / 2, (getHeight() - ph) / 2, pw, ph);
            aiPanel_->toFront(true);
        }
    };

    // ── Wire AI panel actions ──
    aiPanel_->onPreset = [this](const juce::String& presetId, const juce::String& /*label*/) {
        juce::StringArray missing;
        channelRack_->applyDrumPreset(presetId, &missing);

        // Sync BPM to the genre's natural tempo (skip for "empty" preset).
        double presetBpm = ChannelRack::getPresetBPM(presetId);
        if (presetBpm > 0.0) {
            transportBar_->setBPM(presetBpm);
            aiPanel_->addAssistantMessage("Set BPM to " + juce::String((int)presetBpm) + ".");
        }

        // Report any sounds the sample folder didn't have.
        if (!missing.isEmpty()) {
            aiPanel_->addAssistantMessage(
                "Couldn't find a sample for: " + missing.joinIntoString(", ")
                + ". The folder doesn't contain these sounds.");
        }

        // Reveal the channel rack if it's hidden.
        if (!channelRack_->isVisible()) channelRack_->setVisible(true);
        channelRack_->toFront(false);
    };
    aiPanel_->onClose = [this]() { aiPanel_->setVisible(false); };

    // ── Sync MIXER PREVIEW (in BottomDock) with the actual Mixer ──
    bottomDock_->getMixerTrackCount  = [this]()        { return mixer_->getNumTracks(); };
    bottomDock_->getMixerTrackName   = [this](int i)   { return mixer_->getTrackName(i); };
    bottomDock_->getMixerTrackVolume = [this](int i)   { return mixer_->getTrackVolume(i); };
    bottomDock_->getMixerTrackMuted  = [this](int i)   { return mixer_->isTrackMuted(i); };
    bottomDock_->setMixerTrackVolume = [this](int i, float v) { mixer_->setTrackVolume(i, v); };
    mixer_->onTracksChanged = [this]() { bottomDock_->repaint(); };

    // ── Browser → Mixer plugin loading ──
    browser_->onLoadPlugin = [this](const juce::String& name,
                                    const juce::String& fileOrIdentifier) {
        int sel = mixer_->getSelectedTrack();
        if (sel < 0) sel = 0;

        juce::String err;
        int slotId = pluginHost_.loadPlugin(fileOrIdentifier, err);
        if (slotId < 0) {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Plugin load failed",
                err.isNotEmpty() ? err : juce::String("Could not load ") + name);
            return;
        }
        mixer_->addFxToTrack(sel, slotId, name, false);
        pluginHost_.showEditor(slotId, true);
    };
    browser_->onLoadVstPicker = [this]() {
        fileChooser_.reset(new juce::FileChooser(
            "Pick a VST3 / DLL plugin",
            juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory),
            "*.vst3;*.dll"));
        fileChooser_->launchAsync(
            juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                int sel = mixer_->getSelectedTrack();
                if (sel < 0) sel = 0;
                juce::String err;
                int slotId = pluginHost_.loadPlugin(f.getFullPathName(), err);
                if (slotId < 0) {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                        "Plugin load failed",
                        err.isNotEmpty() ? err : juce::String("Could not load ") + f.getFileName());
                    return;
                }
                mixer_->addFxToTrack(sel, slotId, f.getFileNameWithoutExtension(), false);
                pluginHost_.showEditor(slotId, true);
                // Refresh browser list — the newly-scanned plugin should appear.
                browser_->refreshPluginList();
            });
    };
    
    transportBar_->onPlayStateChanged = [this](bool playing) {
        channelRack_->setPlaying(playing);
        repaint(); // refresh STOPPED/PLAYING pill in title bar
    };
    transportBar_->onBPMChanged = [this](double bpm) {
        channelRack_->setBPM(bpm);
        repaint(); // refresh BPM pill in title bar
    };
    
    titleLabel_.setText("STRATUM", juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, Theme::accent);
    titleLabel_.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);
    
    minimizeBtn_.setButtonText("-");
    minimizeBtn_.setColour(juce::TextButton::buttonColourId, Theme::bg2);
    minimizeBtn_.setColour(juce::TextButton::textColourOffId, Theme::text2);
    minimizeBtn_.setLookAndFeel(nullptr);
    minimizeBtn_.onClick = []() {
        if (auto* tlw = juce::TopLevelWindow::getActiveTopLevelWindow())
            if (auto* peer = tlw->getPeer())
                peer->setMinimised(true);
    };
    addAndMakeVisible(minimizeBtn_);
    
    maximizeBtn_.setButtonText("[ ]");
    maximizeBtn_.setColour(juce::TextButton::buttonColourId, Theme::bg2);
    maximizeBtn_.setColour(juce::TextButton::textColourOffId, Theme::text2);
    maximizeBtn_.onClick = [this]() {
        toggleMaximize();
    };
    addAndMakeVisible(maximizeBtn_);
    
    closeBtn_.setButtonText("X");
    closeBtn_.setColour(juce::TextButton::buttonColourId, Theme::bg2);
    closeBtn_.setColour(juce::TextButton::textColourOffId, Theme::red);
    closeBtn_.onClick = []() {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    };
    addAndMakeVisible(closeBtn_);
    
    setSize(1280, 800);
    
    // Keyboard focus for spacebar shortcut
    setWantsKeyboardFocus(true);

    // Capture the initial state and start the undo-polling timer.
    lastSnapshotJson_ = captureSnapshotJson();
    startTimer(400);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        if (transportBar_) transportBar_->togglePlay();
        return true;
    }

    using KP = juce::KeyPress;
    using MK = juce::ModifierKeys;
    // Ctrl+Alt+Z → redo  (check BEFORE plain Ctrl+Z)
    if (key == KP('z', MK::ctrlModifier | MK::altModifier, 0)
     || key == KP('y', MK::ctrlModifier, 0))
    {
        redo();
        return true;
    }
    if (key == KP('z', MK::ctrlModifier, 0))
    {
        undo();
        return true;
    }
    if (key == KP('s', MK::ctrlModifier, 0))
    {
        if (transportBar_ && transportBar_->onSave) transportBar_->onSave();
        return true;
    }
    if (key == KP('o', MK::ctrlModifier, 0))
    {
        openProjectFile();
        return true;
    }
    return false;
}

MainComponent::~MainComponent() = default;

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff09090b));
    
    int w = getWidth();
    constexpr int TB_H = 28;
    
    // ── Top Title Bar — engineered control panel chassis ──────
    auto titleBar = juce::Rectangle<float>(0, 0, (float)w, (float)TB_H);
    
    // Vertical brushed-metal gradient
    juce::ColourGradient tbg(juce::Colour(0xff1a1a1d), 0.0f, 0.0f,
                             juce::Colour(0xff121214), 0.0f, (float)TB_H, false);
    g.setGradientFill(tbg);
    g.fillRect(titleBar);
    
    // Subtle vertical pinstripe (brushed-metal feel)
    g.setColour(juce::Colours::white.withAlpha(0.012f));
    for (int sx = 0; sx < w; sx += 4)
        g.drawVerticalLine(sx, 0.0f, (float)TB_H);
    
    // Top inset highlight (1px white-alpha)
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawHorizontalLine(0, 0.0f, (float)w);
    
    // Bottom etched border (1px zinc-800 + 1px black)
    g.setColour(juce::Colour(0xff27272a));
    g.drawHorizontalLine(TB_H - 2, 0.0f, (float)w);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(TB_H - 1, 0.0f, (float)w);
    
    // ── STRATUM wordmark (engraved) ──
    int x = 14;
    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f).withStyle("Bold"));
    g.drawText("STRATUM", x, 1, 70, TB_H, juce::Justification::centredLeft);
    // Main
    g.setColour(juce::Colour(0xffe4e4e7));
    g.drawText("STRATUM", x, 0, 70, TB_H, juce::Justification::centredLeft);
    
    // ── SYS.01 recessed badge ──
    auto sysBadge = juce::Rectangle<float>((float)x + 64, 7.0f, 38.0f, 14.0f);
    g.setColour(juce::Colour(0xff0a0a0c));
    g.fillRoundedRectangle(sysBadge, 2.5f);
    // Inset shadow top
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawHorizontalLine((int)sysBadge.getY() + 1, sysBadge.getX() + 2, sysBadge.getRight() - 2);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(sysBadge.reduced(0.5f), 2.5f, 0.6f);
    g.setColour(juce::Colour(0xff71717a));
    g.setFont(juce::FontOptions().withName("Consolas").withHeight(7.5f).withStyle("Bold"));
    g.drawText("SYS.01", sysBadge.toNearestInt(), juce::Justification::centred);
    
    // (Menu items + STOPPED/BPM pills removed for a cleaner title bar)
    
    // ── Transport divider ─────────────────────────────────────────
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(TB_H + 60, 0.0f, (float)w);
    
    // ── Vertical divider browser↔main ──────────────────────────
    int browserX = juce::jmax(220, (int)(getWidth() * 0.18));
    g.setColour(juce::Colours::black);
    g.drawVerticalLine(browserX, 28 + 61, (float)getHeight() - juce::jmax(130, (int)(getHeight() * 0.15)));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    int w = getWidth();
    int h = getHeight();
    
    // Title bar (28px)
    auto titleBar = area.removeFromTop(28);
    
    // Window controls (right of title bar)
    closeBtn_.setBounds(titleBar.removeFromRight(30).reduced(2, 4));
    maximizeBtn_.setBounds(titleBar.removeFromRight(30).reduced(2, 4));
    minimizeBtn_.setBounds(titleBar.removeFromRight(30).reduced(2, 4));
    
    // Hide title label (drawn in paint)
    titleLabel_.setBounds(0, 0, 0, 0);
    
    // Transport bar (60px)
    transportBar_->setBounds(area.removeFromTop(60));
    
    // Browser on left - responsive width (18% minimum 220px)
    int browserW = juce::jmax(220, (int)(w * 0.18));
    browser_->setBounds(area.removeFromLeft(browserW));
    
    // Bottom dock - responsive height (15% minimum 130px)
    int dockH = juce::jmax(130, (int)(h * 0.15));
    bottomDock_->setBounds(area.removeFromBottom(dockH));
    
    // All center views share the same bounds; visibility toggles
    mixer_->setBounds(area);
    playlist_->setBounds(area);
    pianoRoll_->setBounds(area);
    
    // Channel rack as floating window centered in main area - responsive
    int crW = juce::jmin(area.getWidth() - 40, (int)(w * 0.55));
    int crH = juce::jmin(area.getHeight() - 40, (int)(h * 0.28));
    int crX = area.getX() + (area.getWidth() - crW) / 2;
    int crY = area.getY() + 20;
    channelRack_->setBounds(crX, crY, crW, crH);
}

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    // Title bar drag (top 28px, but not over window controls)
    if (e.y < 28 && e.x < getWidth() - 100)
    {
        if (auto* topWindow = getTopLevelComponent())
        {
            windowDragger_.startDraggingComponent(topWindow, e);
            isDraggingWindow_ = true;
        }
    }
}

void MainComponent::toggleMaximize()
{
    auto* topWindow = getTopLevelComponent();
    if (!topWindow) return;

    if (!isMaximized_)
    {
        preMaxBounds_ = topWindow->getBounds();

        // Use JUCE's DPI-aware user area (already excludes the taskbar).
        // Pick the display the window is currently on for multi-monitor support.
        auto& displays = juce::Desktop::getInstance().getDisplays();
        auto windowCentre = topWindow->getBounds().getCentre();
        const auto* display = displays.getDisplayForPoint(windowCentre);
        if (display == nullptr)
            display = displays.getPrimaryDisplay();
        if (display == nullptr) return;

        topWindow->setBounds(display->userArea);
        isMaximized_ = true;
    }
    else
    {
        topWindow->setBounds(preMaxBounds_);
        isMaximized_ = false;
    }
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingWindow_)
    {
        if (auto* topWindow = getTopLevelComponent())
        {
            if (isMaximized_) return;
            windowDragger_.dragComponent(topWindow, e, nullptr);
        }
    }
}

void MainComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.y < 28 && e.x < getWidth() - 100)
        toggleMaximize();
}

void MainComponent::setCenterView(CenterView v)
{
    centerView_ = v;
    playlist_->setVisible(v == CenterView::Playlist);
    pianoRoll_->setVisible(v == CenterView::PianoRoll);
    mixer_->setVisible(v == CenterView::Mixer);
    channelRack_->setVisible(v == CenterView::Playlist);
    if (v == CenterView::Playlist) channelRack_->toFront(false);

    // Sync the transport bar's PIANO/MIXER/PLAYLIST pill highlight.
    if (transportBar_)
        transportBar_->setSelectedView(v == CenterView::PianoRoll ? 0
                                       : v == CenterView::Mixer     ? 1 : 2);
    repaint();
}

// ════════════════════════════════════════════════════════════════════
//  Project I/O — .stratum project files
// ════════════════════════════════════════════════════════════════════
bool MainComponent::saveProject(const juce::File& f)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("format",       "stratum-project");
    root->setProperty("version",      1);
    root->setProperty("savedAt",      juce::Time::getCurrentTime().toISO8601(true));
    if (transportBar_) root->setProperty("transport",   transportBar_->toJson());
    if (channelRack_)  root->setProperty("channelRack", channelRack_->toJson());
    if (mixer_)        root->setProperty("mixer",       mixer_->toJson());
    if (playlist_)     root->setProperty("playlist",    playlist_->toJson());

    juce::String json = juce::JSON::toString(juce::var(root), /*allOnOneLine*/ false);
    if (!f.replaceWithText(json))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Save failed", "Couldn't write to:\n" + f.getFullPathName());
        return false;
    }
    currentProjectFile_ = f;
    return true;
}

bool MainComponent::loadProject(const juce::File& f)
{
    if (!f.existsAsFile()) return false;
    auto txt = f.loadFileAsString();
    auto v   = juce::JSON::parse(txt);
    if (!v.isObject() || v.getProperty("format", "").toString() != "stratum-project")
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Open failed", "Not a valid Stratum project file:\n" + f.getFullPathName());
        return false;
    }

    if (transportBar_) transportBar_->fromJson(v.getProperty("transport",   juce::var()));
    if (channelRack_)  channelRack_->fromJson(v.getProperty("channelRack", juce::var()));
    if (mixer_)        mixer_->fromJson(v.getProperty("mixer",       juce::var()));
    if (playlist_)     playlist_->fromJson(v.getProperty("playlist",    juce::var()));

    currentProjectFile_ = f;
    repaint();
    return true;
}

void MainComponent::saveProjectAs()
{
    auto initial = currentProjectFile_.existsAsFile()
                        ? currentProjectFile_
                        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                              .getChildFile("Untitled" + juce::String(kProjectExt));

    fileChooser_.reset(new juce::FileChooser(
        "Save Stratum Project", initial,
        juce::String("*") + kProjectExt));

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::saveMode |
        juce::FileBrowserComponent::canSelectFiles |
        juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            if (f.getFileExtension().compareIgnoreCase(kProjectExt) != 0)
                f = f.withFileExtension(kProjectExt);
            saveProject(f);
        });
}

void MainComponent::openProjectFile()
{
    auto initial = currentProjectFile_.existsAsFile()
                        ? currentProjectFile_.getParentDirectory()
                        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    fileChooser_.reset(new juce::FileChooser(
        "Open Stratum Project", initial,
        juce::String("*") + kProjectExt));

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile()) loadProject(f);
        });
}

// ════════════════════════════════════════════════════════════════════
//  Undo / Redo
// ════════════════════════════════════════════════════════════════════
juce::String MainComponent::captureSnapshotJson() const
{
    auto* root = new juce::DynamicObject();
    if (transportBar_) root->setProperty("transport",   transportBar_->toJson());
    if (channelRack_)  root->setProperty("channelRack", channelRack_->toJson());
    if (mixer_)        root->setProperty("mixer",       mixer_->toJson());
    if (playlist_)     root->setProperty("playlist",    playlist_->toJson());
    return juce::JSON::toString(juce::var(root), /*allOnOneLine*/ true);
}

void MainComponent::applySnapshotJson(const juce::String& json)
{
    auto v = juce::JSON::parse(json);
    if (!v.isObject()) return;
    restoringSnapshot_ = true;
    if (transportBar_) transportBar_->fromJson(v.getProperty("transport",   juce::var()));
    if (channelRack_)  channelRack_->fromJson(v.getProperty("channelRack",  juce::var()));
    if (mixer_)        mixer_->fromJson(v.getProperty("mixer",       juce::var()));
    if (playlist_)     playlist_->fromJson(v.getProperty("playlist",    juce::var()));
    restoringSnapshot_ = false;
    repaint();
}

void MainComponent::timerCallback()
{
    if (restoringSnapshot_) return;

    auto current = captureSnapshotJson();
    if (current == lastSnapshotJson_) return;

    // State changed since last poll → push the PREVIOUS state onto the undo stack
    undoStack_.push_back(lastSnapshotJson_);
    if (undoStack_.size() > kMaxUndo)
        undoStack_.erase(undoStack_.begin(),
                          undoStack_.begin() + (undoStack_.size() - kMaxUndo));

    // A fresh edit invalidates any pending redo history
    redoStack_.clear();
    lastSnapshotJson_ = current;
}

void MainComponent::undo()
{
    if (undoStack_.empty()) return;
    // Save current as redo target, then restore the top of the undo stack
    redoStack_.push_back(lastSnapshotJson_);
    auto prev = undoStack_.back();
    undoStack_.pop_back();
    lastSnapshotJson_ = prev;
    applySnapshotJson(prev);
}

void MainComponent::redo()
{
    if (redoStack_.empty()) return;
    undoStack_.push_back(lastSnapshotJson_);
    auto next = redoStack_.back();
    redoStack_.pop_back();
    lastSnapshotJson_ = next;
    applySnapshotJson(next);
}

