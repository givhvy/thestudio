#include "MainComponent.h"
#include "Theme.h"
#include "PianoRoll.h"

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
    
    addAndMakeVisible(*transportBar_);
    addAndMakeVisible(*playlist_);
    addAndMakeVisible(*pianoRoll_);
    addAndMakeVisible(*mixer_);
    addAndMakeVisible(*channelRack_);
    addAndMakeVisible(*bottomDock_);
    addAndMakeVisible(*browser_);
    
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
    
    // Connect Piano Roll notes changed to save back to channel
    pianoRoll_->onNotesChanged = [this]() {
        int selectedChannel = channelRack_->getSelectedChannel();
        auto& channels = channelRack_->getChannels();
        if (selectedChannel >= 0 && selectedChannel < (int)channels.size()) {
            // Convert PianoRollNote back to ChannelRack::Channel::Note
            auto pianoNotes = pianoRoll_->getNotes();
            channels[selectedChannel].pianoRollNotes.clear();
            for (const auto& n : pianoNotes)
                channels[selectedChannel].pianoRollNotes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
        }
    };
    
    // Connect transport button events to view switching
    transportBar_->onPianoToggle = [this](){ setCenterView(CenterView::PianoRoll); };
    transportBar_->onMixerToggle = [this](){ setCenterView(centerView_ == CenterView::Mixer ? CenterView::Playlist : CenterView::Mixer); };
    
    // Wire up SAVE, OPEN, EXPORT, LOG buttons (placeholder functionality)
    transportBar_->onSave = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Save", "Project saved!");
    };
    transportBar_->onOpen = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Open", "Open project dialog (placeholder)");
    };
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
        setCenterView(CenterView::Playlist);
        channelRack_->toFront(false);
    };
    bottomDock_->onPlugins = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Plugins", "Plugin browser (placeholder)");
    };
    bottomDock_->onVideo = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Video", "Video editor (placeholder)");
    };
    bottomDock_->onAI = [this](){
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "AI", "AI assistant (placeholder)");
    };
    
    transportBar_->onPlayStateChanged = [this](bool playing) {
        channelRack_->setPlaying(playing);
    };
    transportBar_->onBPMChanged = [this](double bpm) {
        channelRack_->setBPM(bpm);
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
    maximizeBtn_.onClick = []() {
        if (auto* tlw = juce::TopLevelWindow::getActiveTopLevelWindow())
            if (auto* peer = tlw->getPeer())
                peer->setFullScreen(!peer->isFullScreen());
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
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        if (transportBar_) transportBar_->togglePlay();
        return true;
    }
    return false;
}

MainComponent::~MainComponent() = default;

void MainComponent::paint(juce::Graphics& g)
{
    // Solid dark background
    g.fillAll(juce::Colour(0xff0a0a0c));
    
    int w = getWidth();
    
    // ── Top Title Bar (28px) ───────────────────────────────────
    auto titleBar = juce::Rectangle<float>(0, 0, (float)w, 28);
    g.setColour(juce::Colour(0xff141417));
    g.fillRect(titleBar);
    
    // Bottom border (1px black + 1px subtle)
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(27, 0.0f, (float)w);
    g.setColour(juce::Colour(0xff222226));
    g.drawHorizontalLine(28, 0.0f, (float)w);
    
    // Glowing orange logo dot
    auto dotRect = juce::Rectangle<float>(10, 10, 8, 8);
    Theme::drawGlowLED(g, dotRect, Theme::orange2, true);
    
    // STRATUM title (bold white)
    g.setColour(juce::Colour(0xffe4e4e7));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
    g.drawText("STRATUM", 24, 0, 64, 28, juce::Justification::centredLeft);
    
    // "Untitled" subtitle (gray italic)
    g.setColour(juce::Colour(0xff71717a));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Italic"));
    g.drawText("Untitled", 88, 0, 60, 28, juce::Justification::centredLeft);
    
    // Menu items: File Edit Add Channels View Options Tools Help
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
    const char* items[] = { "File", "Edit", "Add", "Channels", "View", "Options", "Tools", "Help" };
    int x = 158;
    for (int i = 0; i < 8; ++i)
    {
        juce::String item(items[i]);
        int iw = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), item) + 16;
        g.setColour(Theme::zinc300);
        g.drawText(item, x, 0, iw, 28, juce::Justification::centred);
        x += iw;
    }
    
    // Right-side: window controls (close/max/min are buttons, drawn over)
    // Status: STOPPED·130 BPM (right of menu, before window controls)
    int statusX = w - 220;
    if (statusX > x + 20)
    {
        // Red dot
        auto statusDot = juce::Rectangle<float>((float)statusX, 11.0f, 6.0f, 6.0f);
        Theme::drawGlowLED(g, statusDot, Theme::red2, true);
        // STOPPED text
        g.setColour(Theme::red2);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText("STOPPED", statusX + 11, 0, 60, 28, juce::Justification::centredLeft);
        // Separator dot
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
        g.drawText("· 130 BPM", statusX + 73, 0, 80, 28, juce::Justification::centredLeft);
    }
    
    // ── Transport divider ─────────────────────────────────────────
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(28 + 60, 0.0f, (float)w);
    
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
            windowDragger_.startDraggingComponent(topWindow, e.getEventRelativeTo(topWindow));
    }
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (e.getMouseDownY() < 28 && e.getMouseDownX() < getWidth() - 100)
    {
        if (auto* topWindow = getTopLevelComponent())
        {
            // Don't drag if window is fullscreen/maximised
            if (auto* peer = topWindow->getPeer())
                if (peer->isFullScreen()) return;
            windowDragger_.dragComponent(topWindow, e.getEventRelativeTo(topWindow), nullptr);
        }
    }
}

void MainComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    // Double-click title bar = toggle fullscreen
    if (e.y < 28 && e.x < getWidth() - 100)
    {
        if (auto* topWindow = getTopLevelComponent())
            if (auto* peer = topWindow->getPeer())
                peer->setFullScreen(!peer->isFullScreen());
    }
}

void MainComponent::setCenterView(CenterView v)
{
    centerView_ = v;
    playlist_->setVisible(v == CenterView::Playlist);
    pianoRoll_->setVisible(v == CenterView::PianoRoll);
    mixer_->setVisible(v == CenterView::Mixer);
    channelRack_->setVisible(v == CenterView::Playlist);
    if (v == CenterView::Playlist) channelRack_->toFront(false);
    repaint();
}

