#include "MainComponent.h"
#include "Theme.h"
#include "PianoRoll.h"
#ifdef _WIN32
#include <windows.h>
#endif

static juce::File getBundledStratumPianoVst3()
{
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto repoRoot = exeDir.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
    return repoRoot.getChildFile("vst-plugins")
                   .getChildFile("_installed")
                   .getChildFile("VST3")
                   .getChildFile("Stratum Piano.vst3");
}

static juce::File getBundledStratumGuitarVst3()
{
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto repoRoot = exeDir.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
    return repoRoot.getChildFile("vst-plugins")
                   .getChildFile("_installed")
                   .getChildFile("VST3")
                   .getChildFile("Stratum Guitar.vst3");
}

static int loadBundledStratumPiano(PluginHost& pluginHost, juce::String& errorOut)
{
    auto vst = getBundledStratumPianoVst3();
    if (!vst.exists())
    {
        errorOut = "Bundled Stratum Piano VST3 was not found at: " + vst.getFullPathName();
        return -1;
    }

    return pluginHost.loadPlugin(vst.getFullPathName(), errorOut);
}

static int loadBundledStratumGuitar(PluginHost& pluginHost, juce::String& errorOut)
{
    auto vst = getBundledStratumGuitarVst3();
    if (!vst.exists())
    {
        errorOut = "Bundled Stratum Guitar VST3 was not found at: " + vst.getFullPathName();
        return -1;
    }

    return pluginHost.loadPlugin(vst.getFullPathName(), errorOut);
}

MainComponent::MainComponent(PluginHost& pluginHost, AudioEngine& audioEngine)
    : pluginHost_(pluginHost), audioEngine_(audioEngine)
{
    // ── Embed plugin editors INSIDE this app (FL Studio-style) ─────────
    // PluginHost normally pops a native DocumentWindow per plugin. With
    // these hooks set, the editor is handed to us instead and we host it
    // in a floating PluginWindow child component, so the plugin GUI stays
    // inside the main app window.
    pluginHost_.onEditorReady = [this](int slotId, juce::AudioProcessorEditor* ed,
                                       const juce::String& name)
    {
        if (ed == nullptr) return;
        auto pw = std::make_unique<PluginWindow>(name, ed);
        pw->onClose = [this, slotId]() { pluginHost_.showEditor(slotId, false); };

        // Center the window over the visible client area, clamped on-screen.
        const int W = pw->getWidth();
        const int H = pw->getHeight();
        int x = (getWidth()  - W) / 2;
        int y = (getHeight() - H) / 2;
        x = juce::jlimit(0, juce::jmax(0, getWidth()  - W), x);
        y = juce::jlimit(28, juce::jmax(28, getHeight() - H), y);
        pw->setBounds(x, y, W, H);
        addAndMakeVisible(pw.get());
        pw->toFront(true);
        pluginWindows_[slotId] = std::move(pw);
    };
    pluginHost_.onEditorClosed = [this](int slotId)
    {
        // Drop our wrapper FIRST so it un-parents the editor before
        // PluginHost deletes it.
        pluginWindows_.erase(slotId);
    };

    transportBar_ = std::make_unique<TransportBar>(pluginHost_);
    channelRack_ = std::make_unique<ChannelRack>(pluginHost_);
    mixer_ = std::make_unique<Mixer>(pluginHost_);
    browser_ = std::make_unique<Browser>(pluginHost_);
    playlist_ = std::make_unique<Playlist>(pluginHost_);
    bottomDock_ = std::make_unique<BottomDock>();
    pianoRoll_ = std::make_unique<PianoRoll>(pluginHost_);
    aiPanel_ = std::make_unique<AIPanel>();
    patternsPanel_ = std::make_unique<PatternsPanel>();
    videoPanel_ = std::make_unique<VideoPanel>();
    
    addAndMakeVisible(*transportBar_);
    addAndMakeVisible(*playlist_);
    addAndMakeVisible(*pianoRoll_);
    addAndMakeVisible(*mixer_);
    addAndMakeVisible(*channelRack_);
    addAndMakeVisible(*bottomDock_);
    addAndMakeVisible(*browser_);
    addChildComponent(*aiPanel_);     // hidden until user clicks AI button
    addChildComponent(*patternsPanel_); // hidden until user clicks PATTERNS
    addChildComponent(*videoPanel_);  // hidden until user clicks VIDEO button
    videoPanel_->onClose = [this](){
        videoPanel_->saveWindowState();
        auto& anim = juce::Desktop::getInstance().getAnimator();
        anim.fadeOut (videoPanel_.get(), 130);
    };
    
    // Default view: Playlist
    pianoRoll_->setVisible(false);
    mixer_->setVisible(false);
    
    // Channel rack floats on top
    channelRack_->toFront(false);
    
    // FL Studio-style: clicking the channel index number jumps to the mixer
    // and selects the track this channel is routed through.
    channelRack_->onChannelIndexClicked = [this](int channelIndex) {
        auto& channels = channelRack_->getChannels();
        if (channelIndex < 0 || channelIndex >= (int)channels.size()) return;
        int track = channels[channelIndex].mixerTrack;
        if (track < 0) track = channelIndex; // -1 = auto-route to row index
        if (track >= mixer_->getNumTracks()) track = mixer_->getNumTracks() - 1;
        mixer_->setSelectedTrack(track);
        setCenterView(CenterView::Mixer);
    };

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
    
    // Wire up Channel Rack header buttons
    channelRack_->onAddChannel = [this](){
        // Add a blank percussion channel via popup so user can pick a type.
        juce::PopupMenu m;
        m.addItem (1, "Kick");
        m.addItem (2, "Snare");
        m.addItem (3, "Hihat");
        m.addItem (4, "Clap");
        m.addItem (5, "Bass");
        m.addItem (6, "Lead");
        m.addItem (7, "Pad");
        m.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent (channelRack_.get()),
            [this](int chosen){
                if (chosen <= 0) return;
                using IT = ChannelRack::InstrumentType;
                static const std::pair<juce::String, IT> map[] = {
                    {"Kick", IT::Kick}, {"Snare", IT::Snare}, {"Hihat", IT::Hihat},
                    {"Clap", IT::Clap}, {"Bass", IT::Bass},   {"Lead", IT::Lead}, {"Pad", IT::Pad}
                };
                auto& mp = map[chosen - 1];
                auto& chs = channelRack_->getChannels();
                ChannelRack::Channel c;
                c.name = mp.first;
                c.type = mp.second;
                c.steps = std::vector<bool>(16, false);
                chs.push_back(std::move(c));
                channelRack_->repaint();
            });
    };
    channelRack_->onToggle16_32 = [this](){
        channelRack_->toggleStepCount();
    };
    channelRack_->onStepGraph = [this](){
        // Toggle between step (default) and graph editor for the selected channel:
        // we route that to the Piano Roll for the selected channel.
        int sel = channelRack_->getSelectedChannel();
        if (sel < 0) sel = 0;
        if (channelRack_->onChannelClicked) channelRack_->onChannelClicked(sel);
    };
    channelRack_->onAddPattern = [this](){
        int idx = transportBar_->addPattern();
        transportBar_->setCurrentPattern(idx);
    };
    channelRack_->onAddInstrument = [this](){
        // Open the plugin browser (same as bottom-dock PLUGINS button) so user
        // can pick a VST/synth to add as a new channel.
        if (browser_) browser_->setVisible(true);
        if (browser_) browser_->toFront(true);
    };

    // Bottom "+" button — FL Studio-style add VST instrument channel.
    channelRack_->onAddVstChannel = [this]() {
        auto& known = pluginHost_.getKnownPluginList();
        if (known.getNumTypes() == 0)
            pluginHost_.scanDefaultLocations();

        // Build a popup of instrument plugins only (Kontakt, Serum, etc.).
        juce::PopupMenu menu;
        menu.addItem(8001, "Stratum Piano  [VST3]");
        menu.addItem(8002, "Stratum Guitar  [VST3]");
        menu.addSeparator();
        auto types = pluginHost_.getKnownPluginList().getTypes();
        std::sort(types.begin(), types.end(),
                  [](const juce::PluginDescription& a, const juce::PluginDescription& b)
                  { return a.name.compareIgnoreCase(b.name) < 0; });

        std::vector<juce::PluginDescription> indexed;
        int id = 1;
        for (const auto& d : types)
        {
            if (! d.isInstrument) continue;
            menu.addItem(id, d.name + "  [" + d.pluginFormatName + "]");
            indexed.push_back(d);
            ++id;
        }
        if (indexed.empty())
            menu.addItem(juce::PopupMenu::Item("(no instrument plugins found - re-scan)").setEnabled(false));
        menu.addSeparator();
        menu.addItem(9001, "Browse for .vst3 / .dll...");
        menu.addItem(9003, "Scan a folder...");
        menu.addItem(9002, "Re-scan plugin folders");

        // Anchor the popup right at the "+" button so it appears next to it.
        auto btnLocal = channelRack_->getAddVstButtonBounds();
        auto target   = btnLocal.isEmpty()
                          ? channelRack_->localAreaToGlobal(channelRack_->getBounds()).removeFromBottom(1)
                          : channelRack_->localAreaToGlobal(btnLocal);
        menu.showMenuAsync(
            juce::PopupMenu::Options{}
                .withTargetScreenArea(target)
                .withMinimumWidth(220)
                .withStandardItemHeight(24),
            [this, indexed](int chosen) {
                if (chosen <= 0) return;

                if (chosen == 9002) { pluginHost_.scanDefaultLocations(); return; }

                if (chosen == 8001) {
                    juce::String err;
                    int slotId = loadBundledStratumPiano(pluginHost_, err);
                    auto& chs = channelRack_->getChannels();
                    ChannelRack::Channel c;
                    c.name = "Stratum Piano";
                    c.type = ChannelRack::InstrumentType::Lead;
                    c.steps = std::vector<bool>((size_t)channelRack_->getTotalSteps(), false);
                    c.volume = 0.85f;
                    if (slotId >= 0)
                    {
                        c.pluginSlotId = slotId;
                        pluginHost_.showEditor(slotId, true);
                    }
                    else
                    {
                        c.builtInInstrument = "piano";
                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                            "Stratum Piano VST not loaded",
                            err + "\n\nUsing the emergency built-in piano fallback for now.");
                    }
                    chs.push_back(std::move(c));
                    channelRack_->repaint();
                    return;
                }

                if (chosen == 8002) {
                    juce::String err;
                    int slotId = loadBundledStratumGuitar(pluginHost_, err);
                    auto& chs = channelRack_->getChannels();
                    ChannelRack::Channel c;
                    c.name = "Stratum Guitar";
                    c.type = ChannelRack::InstrumentType::Lead;
                    c.steps = std::vector<bool>((size_t)channelRack_->getTotalSteps(), false);
                    c.volume = 0.85f;
                    if (slotId >= 0)
                    {
                        c.pluginSlotId = slotId;
                        pluginHost_.showEditor(slotId, true);
                    }
                    else
                    {
                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                            "Stratum Guitar VST not loaded",
                            err);
                    }
                    chs.push_back(std::move(c));
                    channelRack_->repaint();
                    return;
                }

                if (chosen == 9003) {
                    // Scan a folder for plugins (e.g., Kontakt Portable location)
                    instrumentChooser_ = std::make_unique<juce::FileChooser>(
                        "Select plugin folder to scan",
                        juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory),
                        "");
                    instrumentChooser_->launchAsync(
                        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                        [this](const juce::FileChooser& fc) {
                            auto dir = fc.getResult();
                            if (! dir.isDirectory()) return;
                            pluginHost_.addPluginScanPath(dir.getFullPathName());
                        });
                    return;
                }

                auto pushChannel = [this](int slotId, const juce::String& name) {
                    auto& chs = channelRack_->getChannels();
                    ChannelRack::Channel c;
                    c.name = name;
                    c.type = ChannelRack::InstrumentType::Lead;
                    c.steps = std::vector<bool>(16, false);
                    c.pluginSlotId = slotId;
                    chs.push_back(std::move(c));
                    channelRack_->repaint();
                    pluginHost_.showEditor(slotId, true);
                };

                if (chosen == 9001) {
                    instrumentChooser_ = std::make_unique<juce::FileChooser>(
                        "Load VST3 / VST plugin",
                        juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory),
                        "*.vst3;*.dll");
                    instrumentChooser_->launchAsync(
                        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                        [this, pushChannel](const juce::FileChooser& fc) {
                            auto file = fc.getResult();
                            if (! file.existsAsFile()) return;
                            juce::String err;
                            int slotId = pluginHost_.loadPlugin(file.getFullPathName(), err);
                            if (slotId < 0) {
                                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                    "Plugin load failed", err);
                                return;
                            }
                            pushChannel(slotId, file.getFileNameWithoutExtension());
                        });
                    return;
                }

                int idx = chosen - 1;
                if (idx < 0 || idx >= (int)indexed.size()) return;
                juce::String err;
                int slotId = pluginHost_.loadPlugin(indexed[idx].fileOrIdentifier, err);
                if (slotId < 0) {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                        "Plugin load failed", err);
                    return;
                }
                pushChannel(slotId, indexed[idx].name);
            });
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

    pianoRoll_->onAuditionNote = [this](int pitch, int lengthSteps, int velocity) {
        const int selectedChannel = channelRack_->getSelectedChannel();
        auto& channels = channelRack_->getChannels();
        const double bpm = transportBar_ ? transportBar_->getBPM() : 120.0;
        const int safeLength = juce::jmax(1, lengthSteps);
        const int holdLength = pianoRealFeel_ ? juce::jmax(6, safeLength) : safeLength;
        const int offDelayMs = juce::jmax(50, (int)std::round((60000.0 / juce::jmax(1.0, bpm) / 4.0) * holdLength));
        const float normalizedVelocity = juce::jlimit(0.0f, 1.0f, (float)velocity / 127.0f);

        if (selectedChannel >= 0 && selectedChannel < (int)channels.size())
        {
            auto& ch = channels[(size_t)selectedChannel];
            const float channelVelocity = juce::jlimit(0.0f, 1.0f, ch.volume * normalizedVelocity);

            if (ch.builtInInstrument == "piano" && ch.pluginSlotId < 0)
            {
                juce::String err;
                int slotId = loadBundledStratumPiano(pluginHost_, err);
                if (slotId >= 0)
                {
                    ch.pluginSlotId = slotId;
                    ch.builtInInstrument.clear();
                    ch.name = "Stratum Piano";
                    channelRack_->repaint();
                }
            }

            if (ch.pluginSlotId >= 0)
            {
                const int slot = ch.pluginSlotId;
                const int note = juce::jlimit(0, 127, pitch);
                const int midiVelocity = juce::jlimit(1, 127, (int)std::round(channelVelocity * 127.0f));
                pluginHost_.sendMidiNote(slot, 1, note, midiVelocity, true);
                juce::Timer::callAfterDelay(offDelayMs, [this, slot, note]() {
                    pluginHost_.sendMidiNote(slot, 1, note, 0, false);
                });
                return;
            }

            if (ch.builtInInstrument == "piano")
            {
                const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
                const double secondsPerStep = 60.0 / juce::jmax(1.0, bpm) / 4.0;
                pluginHost_.playSynthPiano(pitch, now, secondsPerStep * holdLength, channelVelocity);
                return;
            }
        }

        pluginHost_.playSynthTone(440.0 * std::pow(2.0, ((double)pitch - 69.0) / 12.0), 0, 0.2, normalizedVelocity);
    };

    pianoRoll_->onGeneratedMidiBpm = [this](int bpm) {
        if (transportBar_ && bpm > 0)
            transportBar_->setBPM((double)bpm);
    };

    pianoRoll_->onRealFeelChanged = [this](bool enabled) {
        pianoRealFeel_ = enabled;
        if (channelRack_)
            channelRack_->setPianoRealFeel(enabled);
    };
    
    // Drive the Piano Roll and Playlist playheads from the channel rack's step clock.
    channelRack_->onPlayheadTick = [this](int absoluteStep, bool playing) {
        double bpm = transportBar_->getBPM();
        pianoRoll_->setPlayhead(absoluteStep, playing, bpm);
        playlist_->setPlayhead(absoluteStep, playing, bpm);
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
    playlist_->onPlayheadSeek = [this](int absoluteStep) {
        pluginHost_.stopSamplePlayback();
        if (channelRack_)
            channelRack_->setAbsoluteStep(absoluteStep);
        if (pianoRoll_)
            pianoRoll_->setPlayhead(absoluteStep, false, transportBar_->getBPM());
    };
    pianoRoll_->onPlayheadSeek = [this](int absoluteStep) {
        pluginHost_.stopSamplePlayback();
        if (playlist_)
            playlist_->setAbsoluteStep(absoluteStep);
        if (channelRack_)
            channelRack_->setAbsoluteStep(absoluteStep);
    };
    channelRack_->shouldPlayStep = [this](int absoluteStep) {
        if (playbackMode_ == TransportBar::PlaybackMode::Rack || centerView_ == CenterView::PianoRoll)
            return true;
        return playlist_ && playlist_->hasPatternClipAtStep(absoluteStep);
    };
    channelRack_->getPlaybackStep = [this](int absoluteStep, int patternSteps) {
        if (playbackMode_ == TransportBar::PlaybackMode::Rack || centerView_ == CenterView::PianoRoll)
            return patternSteps > 0 ? absoluteStep % patternSteps : 0;
        return playlist_ ? playlist_->patternLocalStepAt(absoluteStep, patternSteps) : -1;
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

    // Mixer X (close) → return to Playlist view
    mixer_->onClose = [this](){ setCenterView(CenterView::Playlist); };

    // Pattern-name sync: keep Playlist and Channel Rack labels in step with the dropdown
    auto syncPatternName = [this]() {
        auto names = transportBar_->getPatterns();
        int idx = transportBar_->getCurrentPattern();
        if (idx >= 0 && idx < names.size())
        {
            playlist_->setCurrentPatternName(names[idx]);
            channelRack_->setCurrentPatternName(names[idx]);
        }
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
        // Export menu: project file (.stratum) is fully implemented; audio
        // bounce is captured live, MIDI is queued for a future build.
        juce::PopupMenu m;
        m.addItem (1, "Export Project (.stratum)");
        m.addItem (2, "Export Pattern as MIDI (.mid)", false);   // disabled
        m.addItem (3, "Export Audio (.wav)",          false);    // disabled
        m.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent (transportBar_.get()),
            [this](int chosen){
                if (chosen == 1) saveProjectAs();
            });
    };
    transportBar_->onLog = [this](){
        // Show a quick status snapshot: BPM, transport, channel & plugin counts.
        juce::String body;
        body << "Build:        Release "  << juce::String(__DATE__) << "\n"
             << "BPM:          "         << juce::String(transportBar_->getBPM(), 2) << "\n"
             << "Transport:    "         << (transportBar_->isPlaying() ? "Playing" : "Stopped") << "\n"
             << "Channels:     "         << juce::String((int) channelRack_->getChannels().size()) << "\n"
             << "Mixer tracks: "         << juce::String(mixer_->getNumTracks()) << "\n"
             << "Patterns:     "         << juce::String(transportBar_->getPatterns().size());
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                "Stratum DAW — Status", body);
    };
    
    // Wire up BottomDock Quick Tools buttons
    bottomDock_->onMixer = [this](){
        setCenterView(centerView_ == CenterView::Mixer ? CenterView::Playlist : CenterView::Mixer);
    };
    bottomDock_->onPianoRoll = [this](){
        setCenterView(centerView_ == CenterView::PianoRoll ? CenterView::Playlist : CenterView::PianoRoll);
    };
    bottomDock_->onChannelRack = [this](){
        auto& anim = juce::Desktop::getInstance().getAnimator();
        if (channelRack_->isVisible())
        {
            anim.fadeOut (channelRack_.get(), 130);
        }
        else
        {
            channelRack_->setAlpha (0.0f);
            channelRack_->setVisible (true);
            anim.fadeIn (channelRack_.get(), 180);
            channelRack_->toFront (false);
        }
    };
    bottomDock_->onPatterns = [this](){
        auto& anim = juce::Desktop::getInstance().getAnimator();
        if (patternsPanel_->isVisible())
        {
            anim.animateComponent (patternsPanel_.get(),
                patternsPanel_->getBounds().translated (0, 30), 0.0f, 160, false, 0.0, 1.0);
            juce::Timer::callAfterDelay (170, [this]{ patternsPanel_->setVisible (false); });
        }
        else
        {
            int pw = juce::jmin(760, getWidth() - 80);
            int ph = juce::jmin(560, getHeight() - 80);
            juce::Rectangle<int> target ((getWidth() - pw) / 2, (getHeight() - ph) / 2, pw, ph);
            patternsPanel_->setBounds (target.translated (0, 30));
            patternsPanel_->setAlpha (0.0f);
            patternsPanel_->setVisible (true);
            patternsPanel_->toFront (true);
            anim.animateComponent (patternsPanel_.get(), target, 1.0f, 200, false, 1.0, 0.0);
        }
    };
    bottomDock_->onVideo = [this](){
        auto& anim = juce::Desktop::getInstance().getAnimator();
        if (videoPanel_->isVisible())
        {
            videoPanel_->saveWindowState();
            anim.fadeOut (videoPanel_.get(), 130);
        }
        else
        {
            int pw = juce::jmin(900, getWidth()  - 80);
            int ph = juce::jmin(620, getHeight() - 100);
            juce::Rectangle<int> defaultTarget ((getWidth() - pw) / 2, (getHeight() - ph) / 2, pw, ph);
            auto target = videoPanel_->getSavedOrDefaultBounds(getLocalBounds(), defaultTarget);
            videoPanel_->setBounds (target);
            videoPanel_->setAlpha (0.0f);
            videoPanel_->setVisible (true);
            videoPanel_->toFront (true);
            anim.fadeIn (videoPanel_.get(), 180);
        }
    };
    bottomDock_->onAI = [this](){
        auto& anim = juce::Desktop::getInstance().getAnimator();
        if (aiPanel_->isVisible())
        {
            anim.animateComponent (aiPanel_.get(),
                aiPanel_->getBounds().translated (0, 30), 0.0f, 160, false, 0.0, 1.0);
            juce::Timer::callAfterDelay (170, [this]{ aiPanel_->setVisible (false); });
        }
        else
        {
            int pw = juce::jmin(640, getWidth() - 80);
            int ph = juce::jmin(640, getHeight() - 80);
            juce::Rectangle<int> target ((getWidth() - pw) / 2, (getHeight() - ph) / 2, pw, ph);
            aiPanel_->setBounds (target.translated (0, 30));
            aiPanel_->setAlpha (0.0f);
            aiPanel_->setVisible (true);
            aiPanel_->toFront (true);
            anim.animateComponent (aiPanel_.get(), target, 1.0f, 200, false, 1.0, 0.0);
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
    auto applyPatternDefinition = [this](const PatternsPanel::PatternDefinition& pattern) {
        ChannelRack::PatternGrid rackGrid {};
        for (size_t r = 0; r < rackGrid.size(); ++r)
            for (size_t s = 0; s < rackGrid[r].size(); ++s)
                rackGrid[r][s] = pattern.rows[r][s];

        if (pattern.useFullPresetRows)
        {
            juce::StringArray missing;
            channelRack_->applyDrumPreset(pattern.presetId, &missing);
            if (!pattern.id.containsIgnoreCase("_default"))
                channelRack_->applyStepPatternToExistingRows(rackGrid);
        }
        else
        {
            channelRack_->applyStepPattern(pattern.title, rackGrid);
        }

        if (pattern.bpm > 0)
        {
            transportBar_->setBPM((double)pattern.bpm);
            aiPanel_->addAssistantMessage("Set BPM to " + juce::String(pattern.bpm) + ".");
        }

        if (!channelRack_->isVisible())
            channelRack_->setVisible(true);
        channelRack_->toFront(false);
    };
    aiPanel_->onPatternVariant = [this, applyPatternDefinition](const PatternsPanel::PatternDefinition& pattern) {
        applyPatternDefinition(pattern);
    };
    aiPanel_->onRerollSounds = [this](const juce::String& presetId, const juce::String& /*label*/) {
        juce::StringArray missing;
        const bool changed = channelRack_->rerollDrumSamples(presetId, &missing);

        if (!missing.isEmpty())
            aiPanel_->addAssistantMessage("Couldn't find a replacement for: " + missing.joinIntoString(", ") + ".");

        if (!channelRack_->isVisible())
            channelRack_->setVisible(true);
        channelRack_->toFront(false);
        return changed;
    };
    aiPanel_->onClose = [this]() {
        auto& anim = juce::Desktop::getInstance().getAnimator();
        anim.animateComponent (aiPanel_.get(),
            aiPanel_->getBounds().translated (0, 30), 0.0f, 160, false, 0.0, 1.0);
        juce::Timer::callAfterDelay (170, [this]{ aiPanel_->setVisible (false); });
    };

    // ── Sync MIXER PREVIEW (in BottomDock) with the actual Mixer ──
    patternsPanel_->onApplyPattern = [this, applyPatternDefinition](const PatternsPanel::PatternDefinition& pattern) {
        applyPatternDefinition(pattern);

        if (aiPanel_)
            aiPanel_->addAssistantMessage("Patterns loaded: " + pattern.title + ".");
    };
    patternsPanel_->onClose = [this]() {
        auto& anim = juce::Desktop::getInstance().getAnimator();
        anim.animateComponent (patternsPanel_.get(),
            patternsPanel_->getBounds().translated (0, 30), 0.0f, 160, false, 0.0, 1.0);
        juce::Timer::callAfterDelay (170, [this]{ patternsPanel_->setVisible (false); });
    };

    bottomDock_->getMixerTrackCount  = [this]()        { return mixer_->getNumTracks(); };
    bottomDock_->getMixerTrackName   = [this](int i)   { return mixer_->getTrackName(i); };
    bottomDock_->getMixerTrackVolume = [this](int i)   { return mixer_->getTrackVolume(i); };
    bottomDock_->getMixerTrackMuted  = [this](int i)   { return mixer_->isTrackMuted(i); };
    bottomDock_->getMixerTrackActivity = [this](int i)
    {
        if (!channelRack_ || !channelRack_->getIsPlaying())
            return 0.0f;

        const auto& channels = channelRack_->getChannels();
        if (i < 0 || i >= (int)channels.size())
            return 0.0f;

        const auto& ch = channels[(size_t)i];
        const int step = channelRack_->getCurrentStep();
        if (step < 0 || step >= (int)ch.steps.size() || !ch.steps[(size_t)step])
            return 0.0f;

        const float mixVol = mixer_ ? mixer_->getTrackVolume(i) : 1.0f;
        const float voiceWeight = (i == 0 ? 1.0f : (i == 1 || i == 3 ? 0.9f : 0.68f));
        return juce::jlimit(0.0f, 1.0f, ch.volume * mixVol * voiceWeight);
    };
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
    
    transportBar_->onPlaybackModeChanged = [this](TransportBar::PlaybackMode mode) {
        applyPlaybackMode(mode);
    };
    applyPlaybackMode(transportBar_->getPlaybackMode());

    transportBar_->onPlayStateChanged = [this](bool playing) {
        applyPlaybackMode(transportBar_->getPlaybackMode());
        if (playing)
        {
            auto& channels = channelRack_->getChannels();
            for (auto& ch : channels)
            {
                if (ch.builtInInstrument == "piano" && ch.pluginSlotId < 0)
                {
                    juce::String err;
                    int slotId = loadBundledStratumPiano(pluginHost_, err);
                    if (slotId >= 0)
                    {
                        ch.pluginSlotId = slotId;
                        ch.builtInInstrument.clear();
                        ch.name = "Stratum Piano";
                    }
                }
            }
            channelRack_->repaint();
        }
        channelRack_->setPlaying(playing);
        if (!playing)
            pluginHost_.stopSamplePlayback(); // kill any browser preview / channel hits
        repaint(); // refresh STOPPED/PLAYING pill in title bar
    };
    transportBar_->onBPMChanged = [this](double bpm) {
        channelRack_->setBPM(bpm);
        playlist_->setBPM(bpm);
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

    themeBtn_.setColour(juce::TextButton::buttonColourId, Theme::bg2);
    themeBtn_.setColour(juce::TextButton::textColourOffId, Theme::accentBright);
    themeBtn_.setColour(juce::TextButton::buttonOnColourId, Theme::accentDim);
    themeBtn_.onClick = [this]() { showThemeMenu(); };
    addAndMakeVisible(themeBtn_);

    auto themeText = themeStateFile().loadFileAsString().trim().toLowerCase();
    if (themeText == "blue") applyThemePreset(Theme::Preset::Blue, false);
    else if (themeText == "purple") applyThemePreset(Theme::Preset::Purple, false);
    else if (themeText == "emerald") applyThemePreset(Theme::Preset::Emerald, false);
    else if (themeText == "crimson") applyThemePreset(Theme::Preset::Crimson, false);
    else if (themeText == "gold") applyThemePreset(Theme::Preset::Gold, false);
    else applyThemePreset(Theme::Preset::Default, false);
    
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
    if (key == KP('s', MK::ctrlModifier | MK::shiftModifier, 0))
    {
        saveProjectAs();
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

juce::File MainComponent::themeStateFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stratum DAW");
    dir.createDirectory();
    return dir.getChildFile("theme.state");
}

void MainComponent::refreshThemeButton()
{
    juce::String label = "THEME";
    switch (Theme::currentPreset)
    {
        case Theme::Preset::Blue: label = "BLUE"; break;
        case Theme::Preset::Purple: label = "PURPLE"; break;
        case Theme::Preset::Emerald: label = "EMERALD"; break;
        case Theme::Preset::Crimson: label = "CRIMSON"; break;
        case Theme::Preset::Gold: label = "GOLD"; break;
        case Theme::Preset::Default:
        default: label = "THEME"; break;
    }

    themeBtn_.setButtonText(label);
    themeBtn_.setColour(juce::TextButton::buttonColourId, Theme::bg2);
    themeBtn_.setColour(juce::TextButton::textColourOffId, Theme::accentBright);
    minimizeBtn_.setColour(juce::TextButton::buttonColourId, Theme::bg2);
    minimizeBtn_.setColour(juce::TextButton::textColourOffId, Theme::text2);
    maximizeBtn_.setColour(juce::TextButton::buttonColourId, Theme::bg2);
    maximizeBtn_.setColour(juce::TextButton::textColourOffId, Theme::text2);
    closeBtn_.setColour(juce::TextButton::buttonColourId, Theme::bg2);
    closeBtn_.setColour(juce::TextButton::textColourOffId, Theme::red);
}

void MainComponent::applyThemePreset(Theme::Preset preset, bool persist)
{
    Theme::applyPreset(preset);
    refreshThemeButton();

    if (persist)
    {
        juce::String id = "default";
        if (preset == Theme::Preset::Blue) id = "blue";
        else if (preset == Theme::Preset::Purple) id = "purple";
        else if (preset == Theme::Preset::Emerald) id = "emerald";
        else if (preset == Theme::Preset::Crimson) id = "crimson";
        else if (preset == Theme::Preset::Gold) id = "gold";
        themeStateFile().replaceWithText(id);
    }

    repaint();
    for (auto* child : getChildren())
        if (child) child->repaint();
}

void MainComponent::showThemeMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("Themes");
    menu.addItem(1, "Default Theme", true, Theme::currentPreset == Theme::Preset::Default);
    menu.addItem(2, "Blue Steel", true, Theme::currentPreset == Theme::Preset::Blue);
    menu.addItem(3, "Purple Neon", true, Theme::currentPreset == Theme::Preset::Purple);
    menu.addItem(4, "Emerald Matrix", true, Theme::currentPreset == Theme::Preset::Emerald);
    menu.addItem(5, "Crimson Heat", true, Theme::currentPreset == Theme::Preset::Crimson);
    menu.addItem(6, "Gold", true, Theme::currentPreset == Theme::Preset::Gold);

    menu.showMenuAsync(
        juce::PopupMenu::Options{}
            .withTargetComponent(this)
            .withTargetScreenArea(localAreaToGlobal(themeBtn_.getBounds())),
        [this](int result)
        {
            if (result == 1) applyThemePreset(Theme::Preset::Default, true);
            else if (result == 2) applyThemePreset(Theme::Preset::Blue, true);
            else if (result == 3) applyThemePreset(Theme::Preset::Purple, true);
            else if (result == 4) applyThemePreset(Theme::Preset::Emerald, true);
            else if (result == 5) applyThemePreset(Theme::Preset::Crimson, true);
            else if (result == 6) applyThemePreset(Theme::Preset::Gold, true);
        });
}

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
    themeBtn_.setBounds(168, 5, 78, 18);
    
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
    if (e.y < 28 && e.x < getWidth() - 100 && !themeBtn_.getBounds().contains(e.x, e.y))
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

   #ifdef _WIN32
    // Use Win32's native maximize so the borderless window snaps exactly to
    // the work area Windows reports (no black strip near the taskbar) and
    // restores cleanly on toggle.
    if (auto* peer = topWindow->getPeer())
    {
        auto hwnd = (HWND) peer->getNativeHandle();
        if (! isMaximized_)
        {
            preMaxBounds_ = topWindow->getBounds();
            ShowWindow (hwnd, SW_MAXIMIZE);
            isMaximized_ = true;
        }
        else
        {
            ShowWindow (hwnd, SW_RESTORE);
            // Win32 will restore to its remembered position; force ours to be safe.
            topWindow->setBounds (preMaxBounds_);
            isMaximized_ = false;
        }
        return;
    }
   #endif

    // Non-Windows fallback: JUCE work-area.
    if (!isMaximized_)
    {
        preMaxBounds_ = topWindow->getBounds();
        auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* display = displays.getDisplayForPoint (topWindow->getBounds().getCentre());
        if (display == nullptr) display = displays.getPrimaryDisplay();
        if (display == nullptr) return;
        topWindow->setBounds (display->userArea);
        isMaximized_ = true;
    }
    else
    {
        topWindow->setBounds (preMaxBounds_);
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
    if (centerView_ == v) return;
    centerView_ = v;

    auto& anim = juce::Desktop::getInstance().getAnimator();

    auto crossfade = [&] (juce::Component* show,
                          std::initializer_list<juce::Component*> hide)
    {
        for (auto* c : hide)
            if (c->isVisible()) anim.fadeOut (c, 110);

        if (! show->isVisible())
        {
            show->setAlpha (0.0f);
            show->setVisible (true);
        }
        anim.fadeIn (show, 170);
    };

    switch (v)
    {
        case CenterView::Playlist:
            crossfade (playlist_.get(),  { pianoRoll_.get(), mixer_.get() }); break;
        case CenterView::PianoRoll:
            crossfade (pianoRoll_.get(), { playlist_.get(), mixer_.get() }); break;
        case CenterView::Mixer:
            crossfade (mixer_.get(),     { playlist_.get(), pianoRoll_.get() }); break;
    }

    // Channel rack rides with the Playlist view.
    bool wantRack = (v == CenterView::Playlist);
    if (wantRack)
    {
        if (! channelRack_->isVisible())
        {
            channelRack_->setAlpha (0.0f);
            channelRack_->setVisible (true);
        }
        anim.fadeIn (channelRack_.get(), 170);
        channelRack_->toFront (false);
    }
    else if (channelRack_->isVisible())
    {
        anim.fadeOut (channelRack_.get(), 110);
    }

    if (transportBar_)
        transportBar_->setSelectedView (v == CenterView::PianoRoll ? 0
                                       : v == CenterView::Mixer    ? 1 : 2);
    repaint();
}

// ════════════════════════════════════════════════════════════════════
//  Project I/O — .stratum project files
// ════════════════════════════════════════════════════════════════════
void MainComponent::applyPlaybackMode(TransportBar::PlaybackMode mode)
{
    playbackMode_ = mode;
    const bool playlistMode = (mode == TransportBar::PlaybackMode::Playlist);

    if (channelRack_)
        channelRack_->setPlaybackAudible(true);

    if (playlist_)
        playlist_->setPlaybackEnabled(playlistMode);
}

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

