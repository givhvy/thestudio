#include "MainComponent.h"
#include "Theme.h"
#include "PianoRoll.h"
#include <algorithm>
#include <cmath>
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
static BOOL CALLBACK collectChordifyWindowsForExternalDrop(HWND hwnd, LPARAM lParam)
{
    if (!IsWindowVisible(hwnd))
        return TRUE;

    wchar_t title[512] {};
    GetWindowTextW(hwnd, title, 512);
    if (juce::String(title).containsIgnoreCase("Chordify"))
        reinterpret_cast<std::vector<HWND>*>(lParam)->push_back(hwnd);

    return TRUE;
}

static void keepChordifyAboveStratumDuringExternalDrop()
{
    std::vector<HWND> windows;
    EnumWindows(collectChordifyWindowsForExternalDrop, reinterpret_cast<LPARAM>(&windows));

    for (auto hwnd : windows)
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if (!windows.empty())
    {
        std::thread([windows]()
        {
            Sleep(8000);
            for (auto hwnd : windows)
                if (IsWindow(hwnd))
                    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }).detach();
    }
}
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

class ProjectOpenOverlay : public juce::Component
{
public:
    explicit ProjectOpenOverlay(juce::File rootFolder)
        : rootFolder_(std::move(rootFolder))
    {
        setWantsKeyboardFocus(true);
        refreshProjects();
    }

    std::function<void(const juce::File&)> onOpen;
    std::function<void()> onClose;

    void paint(juce::Graphics& g) override
    {
        updateLayout();

        g.fillAll(juce::Colours::black.withAlpha(0.58f));
        g.setColour(juce::Colours::white.withAlpha(0.025f));
        for (int x = 0; x < getWidth(); x += 32)
            g.drawVerticalLine(x, 0.0f, (float)getHeight());
        for (int y = 0; y < getHeight(); y += 32)
            g.drawHorizontalLine(y, 0.0f, (float)getWidth());

        for (int i = 5; i > 0; --i)
        {
            g.setColour(Theme::accent.withAlpha(0.035f / (float)i));
            g.fillRoundedRectangle(panel_.toFloat().expanded((float)i * 8.0f), 22.0f);
        }

        juce::ColourGradient glass(juce::Colours::white.withAlpha(0.13f), (float)panel_.getX(), (float)panel_.getY(),
                                   juce::Colour(0xff0b0b0f).withAlpha(0.94f), (float)panel_.getRight(), (float)panel_.getBottom(), false);
        g.setGradientFill(glass);
        g.fillRoundedRectangle(panel_.toFloat(), 18.0f);
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.drawRoundedRectangle(panel_.toFloat().reduced(0.5f), 18.0f, 1.0f);
        g.setColour(Theme::accent.withAlpha(0.42f));
        g.drawRoundedRectangle(panel_.toFloat().reduced(1.5f), 17.0f, 1.0f);

        g.setColour(Theme::accent);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(18.0f).withStyle("Bold"));
        g.drawText("Open Project", titleRect_, juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f));
        g.drawText("Choose a Stratum project from " + rootFolder_.getFullPathName(),
                   subtitleRect_, juce::Justification::centredLeft, true);

        drawPill(g, refreshRect_, "REFRESH", false);
        drawPill(g, closeRect_, "X", false);

        auto listBg = listRect_.toFloat();
        g.setColour(juce::Colour(0xff050507).withAlpha(0.62f));
        g.fillRoundedRectangle(listBg, 12.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(listBg, 12.0f, 1.0f);

        if (rows_.empty())
        {
            g.setColour(Theme::zinc300);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
            g.drawText("No .stratum projects found", listRect_.reduced(20, 0), juce::Justification::centred);
            g.setColour(Theme::zinc500);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f));
            g.drawText("Save projects into D:\\stratumdaw and they will appear here.",
                       listRect_.translated(0, 22).reduced(20, 0), juce::Justification::centred);
        }

        for (int i = 0; i < (int)rows_.size(); ++i)
        {
            auto r = rows_[(size_t)i].rect.toFloat();
            const bool selected = i == selectedIndex_;
            juce::ColourGradient card(selected ? Theme::accent.withAlpha(0.30f) : juce::Colours::white.withAlpha(0.08f),
                                      r.getX(), r.getY(),
                                      juce::Colour(0xff101014).withAlpha(0.88f), r.getRight(), r.getBottom(), false);
            g.setGradientFill(card);
            g.fillRoundedRectangle(r, 10.0f);
            g.setColour(selected ? Theme::accentBright : juce::Colours::white.withAlpha(0.10f));
            g.drawRoundedRectangle(r.reduced(0.5f), 10.0f, selected ? 1.6f : 1.0f);

            const auto& file = rows_[(size_t)i].file;
            g.setColour(selected ? Theme::zinc100 : Theme::zinc200);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
            g.drawText(file.getFileNameWithoutExtension(), rows_[(size_t)i].rect.reduced(18, 8).removeFromTop(18),
                       juce::Justification::centredLeft, true);

            g.setColour(Theme::zinc500);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
            const auto mod = file.getLastModificationTime().formatted("%d %b %Y  %H:%M");
            g.drawText(mod + "    " + juce::String(file.getSize() / 1024) + " KB",
                       rows_[(size_t)i].rect.reduced(18, 8).withTrimmedTop(22),
                       juce::Justification::centredLeft, true);
        }

        drawPill(g, openRect_, selectedIndex_ >= 0 ? "OPEN SELECTED" : "SELECT A PROJECT", selectedIndex_ >= 0);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        updateLayout();
        if (closeRect_.contains(e.x, e.y))
        {
            if (onClose) onClose();
            return;
        }
        if (refreshRect_.contains(e.x, e.y))
        {
            refreshProjects();
            repaint();
            return;
        }
        if (openRect_.contains(e.x, e.y) && selectedIndex_ >= 0)
        {
            openSelected();
            return;
        }
        for (int i = 0; i < (int)rows_.size(); ++i)
        {
            if (rows_[(size_t)i].rect.contains(e.x, e.y))
            {
                selectedIndex_ = i;
                repaint();
                return;
            }
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        updateLayout();
        for (int i = 0; i < (int)rows_.size(); ++i)
        {
            if (rows_[(size_t)i].rect.contains(e.x, e.y))
            {
                selectedIndex_ = i;
                openSelected();
                return;
            }
        }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose) onClose();
            return true;
        }
        if (key == juce::KeyPress::returnKey && selectedIndex_ >= 0)
        {
            openSelected();
            return true;
        }
        return false;
    }

private:
    struct Row
    {
        juce::File file;
        juce::Rectangle<int> rect;
    };

    juce::File rootFolder_;
    std::vector<Row> rows_;
    int selectedIndex_ = -1;

    juce::Rectangle<int> panel_, titleRect_, subtitleRect_, closeRect_, refreshRect_, listRect_, openRect_;

    void refreshProjects()
    {
        rootFolder_.createDirectory();
        juce::Array<juce::File> found;
        rootFolder_.findChildFiles(found, juce::File::findFiles, false, "*.stratum");

        std::sort(found.begin(), found.end(), [](const juce::File& a, const juce::File& b)
        {
            return a.getLastModificationTime() > b.getLastModificationTime();
        });

        rows_.clear();
        rows_.reserve((size_t)found.size());
        for (auto& f : found)
            rows_.push_back({ f, {} });
        selectedIndex_ = rows_.empty() ? -1 : 0;
    }

    void updateLayout()
    {
        const int width = juce::jlimit(560, 860, getWidth() - 80);
        const int height = juce::jlimit(360, 620, getHeight() - 80);
        panel_ = juce::Rectangle<int>((getWidth() - width) / 2, (getHeight() - height) / 2, width, height);

        auto content = panel_.reduced(28, 24);
        closeRect_ = juce::Rectangle<int>(panel_.getRight() - 46, panel_.getY() + 18, 28, 26);
        refreshRect_ = juce::Rectangle<int>(panel_.getRight() - 146, panel_.getY() + 18, 86, 26);
        titleRect_ = content.removeFromTop(28);
        subtitleRect_ = content.removeFromTop(24);
        content.removeFromTop(14);
        openRect_ = content.removeFromBottom(42).removeFromRight(180);
        content.removeFromBottom(16);
        listRect_ = content;

        const int rowH = 62;
        int y = listRect_.getY() + 12;
        for (auto& row : rows_)
        {
            row.rect = juce::Rectangle<int>(listRect_.getX() + 12, y, listRect_.getWidth() - 24, rowH);
            y += rowH + 10;
        }
    }

    void drawPill(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool active)
    {
        juce::ColourGradient bg(active ? Theme::accentBright : juce::Colours::white.withAlpha(0.10f),
                                (float)r.getX(), (float)r.getY(),
                                active ? Theme::accentDim : juce::Colour(0xff15151a),
                                (float)r.getRight(), (float)r.getBottom(), false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(r.toFloat(), 8.0f);
        g.setColour(active ? Theme::accentBright : juce::Colours::white.withAlpha(0.14f));
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 8.0f, 1.0f);
        g.setColour(active ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(text, r, juce::Justification::centred, true);
    }

    void openSelected()
    {
        if (selectedIndex_ < 0 || selectedIndex_ >= (int)rows_.size())
            return;
        if (onOpen)
            onOpen(rows_[(size_t)selectedIndex_].file);
    }
};

class ProjectSaveOverlay : public juce::Component
{
public:
    ProjectSaveOverlay(juce::File rootFolder, juce::String defaultName)
        : rootFolder_(std::move(rootFolder))
    {
        setWantsKeyboardFocus(true);
        rootFolder_.createDirectory();

        if (defaultName.trim().isEmpty())
            defaultName = "Untitled";
        nameEditor_.setText(defaultName, false);
        nameEditor_.setSelectAllWhenFocused(true);
        nameEditor_.setJustification(juce::Justification::centredLeft);
        nameEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff050507).withAlpha(0.70f));
        nameEditor_.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.12f));
        nameEditor_.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accentBright);
        nameEditor_.setColour(juce::TextEditor::textColourId, Theme::zinc100);
        addAndMakeVisible(nameEditor_);
    }

    std::function<void(const juce::String&)> onSave;
    std::function<void()> onClose;

    void paint(juce::Graphics& g) override
    {
        updateLayout();

        g.fillAll(juce::Colours::black.withAlpha(0.58f));
        g.setColour(juce::Colours::white.withAlpha(0.025f));
        for (int x = 0; x < getWidth(); x += 32)
            g.drawVerticalLine(x, 0.0f, (float)getHeight());
        for (int y = 0; y < getHeight(); y += 32)
            g.drawHorizontalLine(y, 0.0f, (float)getWidth());

        for (int i = 5; i > 0; --i)
        {
            g.setColour(Theme::accent.withAlpha(0.035f / (float)i));
            g.fillRoundedRectangle(panel_.toFloat().expanded((float)i * 8.0f), 22.0f);
        }

        juce::ColourGradient glass(juce::Colours::white.withAlpha(0.13f), (float)panel_.getX(), (float)panel_.getY(),
                                   juce::Colour(0xff0b0b0f).withAlpha(0.94f), (float)panel_.getRight(), (float)panel_.getBottom(), false);
        g.setGradientFill(glass);
        g.fillRoundedRectangle(panel_.toFloat(), 18.0f);
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.drawRoundedRectangle(panel_.toFloat().reduced(0.5f), 18.0f, 1.0f);
        g.setColour(Theme::accent.withAlpha(0.42f));
        g.drawRoundedRectangle(panel_.toFloat().reduced(1.5f), 17.0f, 1.0f);

        g.setColour(Theme::accent);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(18.0f).withStyle("Bold"));
        g.drawText("Export Audio", titleRect_, juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f));
        g.drawText("Saves a .wav export and matching .stratum project into " + rootFolder_.getFullPathName(),
                   subtitleRect_, juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText("FILE NAME", labelRect_, juce::Justification::centredLeft, true);

        drawPill(g, closeRect_, "X", false);
        drawPill(g, cancelRect_, "CANCEL", false);
        drawPill(g, saveRect_, "EXPORT WAV", true);

        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
        auto preview = makeCleanName(nameEditor_.getText());
        if (preview.isEmpty()) preview = "Untitled";
        g.drawText(rootFolder_.getFullPathName() + "\\" + preview + ".wav",
                   previewRect_, juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        updateLayout();
        nameEditor_.setBounds(editorRect_);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        updateLayout();
        if (closeRect_.contains(e.x, e.y) || cancelRect_.contains(e.x, e.y))
        {
            if (onClose) onClose();
            return;
        }
        if (saveRect_.contains(e.x, e.y))
            save();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose) onClose();
            return true;
        }
        if (key == juce::KeyPress::returnKey)
        {
            save();
            return true;
        }
        return false;
    }

private:
    juce::File rootFolder_;
    juce::TextEditor nameEditor_;
    juce::Rectangle<int> panel_, titleRect_, subtitleRect_, labelRect_, editorRect_, previewRect_;
    juce::Rectangle<int> closeRect_, cancelRect_, saveRect_;

    static juce::String makeCleanName(juce::String name)
    {
        name = name.trim();
        for (auto c : juce::String(R"(\/:*?"<>|)"))
            name = name.replaceCharacter(c, '-');
        return name.trim();
    }

    void updateLayout()
    {
        const int width = juce::jlimit(440, 680, getWidth() - 80);
        const int height = 300;
        panel_ = juce::Rectangle<int>((getWidth() - width) / 2, (getHeight() - height) / 2, width, height);

        auto content = panel_.reduced(28, 24);
        closeRect_ = juce::Rectangle<int>(panel_.getRight() - 46, panel_.getY() + 18, 28, 26);
        titleRect_ = content.removeFromTop(30);
        subtitleRect_ = content.removeFromTop(42);
        content.removeFromTop(8);
        labelRect_ = content.removeFromTop(18);
        editorRect_ = content.removeFromTop(40);
        content.removeFromTop(8);
        previewRect_ = content.removeFromTop(24);
        saveRect_ = panel_.reduced(28, 24).removeFromBottom(42).removeFromRight(170);
        cancelRect_ = saveRect_.translated(-110, 0).withWidth(96);
    }

    void drawPill(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool active)
    {
        juce::ColourGradient bg(active ? Theme::accentBright : juce::Colours::white.withAlpha(0.10f),
                                (float)r.getX(), (float)r.getY(),
                                active ? Theme::accentDim : juce::Colour(0xff15151a),
                                (float)r.getRight(), (float)r.getBottom(), false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(r.toFloat(), 8.0f);
        g.setColour(active ? Theme::accentBright : juce::Colours::white.withAlpha(0.14f));
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 8.0f, 1.0f);
        g.setColour(active ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(text, r, juce::Justification::centred, true);
    }

    void save()
    {
        auto clean = makeCleanName(nameEditor_.getText());
        if (clean.isEmpty())
            clean = "Untitled";
        if (onSave)
            onSave(clean);
    }
};

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
    pluginHost_.onNativeEffectEditorRequested = [this](int effectId, const juce::String& name)
    {
        auto existing = nativeEffectWindows_.find(effectId);
        if (existing != nativeEffectWindows_.end())
        {
            existing->second->toFront(true);
            return;
        }

        auto editor = std::make_unique<NativeEffectEditor>(pluginHost_, effectId);
        auto win = std::make_unique<NativeEffectWindow>(name, std::move(editor));
        win->onClose = [this, effectId] { nativeEffectWindows_.erase(effectId); };

        const int W = win->getWidth();
        const int H = win->getHeight();
        int x = (getWidth() - W) / 2;
        int y = (getHeight() - H) / 2;
        x = juce::jlimit(0, juce::jmax(0, getWidth() - W), x);
        y = juce::jlimit(28, juce::jmax(28, getHeight() - H), y);
        win->setBounds(x, y, W, H);
        addAndMakeVisible(win.get());
        win->toFront(true);
        nativeEffectWindows_[effectId] = std::move(win);
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
        if (videoPanel_->isEmbeddedInSession())
        {
            videoPanel_->unembedPlayerFromSession();
            if (bottomDock_)
                bottomDock_->setSessionVideoMode(false);
        }
        videoPanel_->saveWindowState();
        auto& anim = juce::Desktop::getInstance().getAnimator();
        anim.fadeOut (videoPanel_.get(), 130);
    };
    bottomDock_->onSessionVideoLayout = [this]() {
        if (videoPanel_ && videoPanel_->isEmbeddedInSession())
            videoPanel_->syncWebPlayerBounds();
    };
    videoPanel_->onOpenInSessionTab = [this]() {
        if (!videoPanel_->hasVideoLoaded() || !bottomDock_)
            return;

        bottomDock_->setSessionVideoMode(true);
        bottomDock_->resized();

        juce::MessageManager::callAsync([this]()
        {
            if (!videoPanel_ || !bottomDock_)
                return;

            videoPanel_->embedPlayerInSession(bottomDock_->getSessionVideoHost());

            auto& anim = juce::Desktop::getInstance().getAnimator();
            if (videoPanel_->isVisible())
                anim.fadeOut(videoPanel_.get(), 130);
        });
    };
    bottomDock_->onRestoreSessionInfo = [this]() {
        if (!videoPanel_ || !bottomDock_)
            return;

        const bool hadVideo = videoPanel_->hasVideoLoaded();
        videoPanel_->unembedPlayerFromSession();
        bottomDock_->setSessionVideoMode(false);

        auto popOutVideoWindow = [this]()
        {
            int pw = juce::jmin(900, getWidth() - 80);
            int ph = juce::jmin(620, getHeight() - 100);
            juce::Rectangle<int> defaultTarget((getWidth() - pw) / 2, (getHeight() - ph) / 2, pw, ph);
            auto target = videoPanel_->getSavedOrDefaultBounds(getLocalBounds(), defaultTarget);
            videoPanel_->setBounds(target);
            videoPanel_->resized();
            videoPanel_->syncWebPlayerBounds();
            videoPanel_->scheduleWebLayoutSync();
            videoPanel_->setAlpha(0.0f);
            videoPanel_->setVisible(true);
            videoPanel_->toFront(true);
            juce::Desktop::getInstance().getAnimator().fadeIn(videoPanel_.get(), 180);
        };

        if (hadVideo && !videoPanel_->isVisible())
            popOutVideoWindow();
        else if (hadVideo)
            videoPanel_->scheduleWebLayoutSync();
    };

    // Default view: Playlist
    pianoRoll_->setVisible(false);
    mixer_->setVisible(false);
    
    // Channel rack floats on top
    channelRack_->toFront(false);

    auto syncMixerToChannelRack = [this]()
    {
        if (!channelRack_ || !mixer_)
            return;

        std::vector<juce::String> names;
        auto& channels = channelRack_->getChannels();
        names.reserve(channels.size());
        for (const auto& ch : channels)
            names.push_back(ch.name);

        mixer_->syncFromChannelRack(names);

        const int insertCount = juce::jmax(0, mixer_->getNumTracks() - 1);
        for (int i = 0; i < (int)channels.size(); ++i)
        {
            auto& ch = channels[(size_t)i];
            const int autoTrack = insertCount > 0 ? juce::jmin(i, insertCount - 1) : -1;
            ch.mixerTrack = autoTrack;

            if (ch.pluginSlotId >= 0 && ch.mixerTrack >= 0)
                pluginHost_.setSlotTrack(ch.pluginSlotId, ch.mixerTrack);
        }

        if (bottomDock_)
            bottomDock_->repaint();
    };

    channelRack_->onChannelsChanged = syncMixerToChannelRack;
    syncMixerToChannelRack();
    
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

    playlist_->onExtractBassMidi = [this](const juce::String& sourceName,
                                          const std::vector<Playlist::ExtractedBassNote>& extractedNotes)
    {
        if (!channelRack_ || extractedNotes.empty())
            return;

        std::vector<ChannelRack::Channel::Note> notes;
        notes.reserve(extractedNotes.size());
        for (const auto& n : extractedNotes)
            notes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });

        juce::PopupMenu menu;
        menu.addSectionHeader("Send extracted bass to");
        menu.addItem(1, "New Extracted Bass slot");
        menu.addItem(2, "New Stratum Bass slot");
        menu.addSeparator();

        const auto& channels = channelRack_->getChannels();
        for (int i = 0; i < (int)channels.size(); ++i)
            menu.addItem(100 + i, juce::String(i + 1) + "  " + channels[(size_t)i].name);

        juce::Component::SafePointer<MainComponent> safe(this);
        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(channelRack_.get()).withMinimumWidth(240),
            [safe, sourceName, notes](int chosen)
            {
                if (safe == nullptr || chosen <= 0 || !safe->channelRack_)
                    return;

                int channelIndex = -1;
                if (chosen == 1)
                {
                    channelIndex = safe->channelRack_->applyExtractedBassMidi(sourceName, notes, -1);
                }
                else if (chosen == 2)
                {
                    channelIndex = safe->channelRack_->applyExtractedBassMidi(sourceName, notes, -1);
                    if (channelIndex >= 0)
                        safe->channelRack_->setChannelToNativeBass(channelIndex);
                }
                else if (chosen >= 100)
                {
                    const int target = chosen - 100;
                    channelIndex = safe->channelRack_->applyExtractedBassMidi(sourceName, notes, target);
                }

                if (channelIndex >= 0 && safe->channelRack_->onChannelClicked)
                    safe->channelRack_->onChannelClicked(channelIndex);
            });
    };

    playlist_->onAutoExtractBassMidi = [this](const juce::String& sourceName,
                                              const std::vector<Playlist::ExtractedBassNote>& extractedNotes)
    {
        if (!channelRack_ || extractedNotes.empty())
            return;

        std::vector<ChannelRack::Channel::Note> notes;
        notes.reserve(extractedNotes.size());
        for (const auto& n : extractedNotes)
            notes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });

        channelRack_->applyExtractedBassMidi(sourceName, notes, -1);
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
                if (channelRack_->onChannelsChanged) channelRack_->onChannelsChanged();
            });
    };
    channelRack_->onToggle16_32 = [this](){
        channelRack_->toggleStepCount();
        if (playlist_)
            playlist_->setPatternDefaultSteps(channelRack_->getTotalSteps());
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
        // Build this menu from cached plugin data only. Full plugin scans can be
        // slow, so they run only from the explicit scan actions below.
        juce::PopupMenu menu;
        menu.addItem(8001, "Stratum Piano  [VST3]");
        menu.addItem(8002, "Stratum Guitar  [VST3]");
        menu.addItem(8003, "Stratum Bass  [Native]");
        menu.addSeparator();
        auto types = pluginHost_.getKnownPluginList().getTypes();
        std::sort(types.begin(), types.end(),
                  [](const juce::PluginDescription& a, const juce::PluginDescription& b)
                  { return a.name.compareIgnoreCase(b.name) < 0; });

        std::vector<juce::PluginDescription> indexed;
        juce::StringArray seen;
        int id = 1;
        for (const auto& d : types)
        {
            if (! d.isInstrument) continue;
            const auto key = d.name + "|" + d.pluginFormatName + "|" + d.fileOrIdentifier;
            if (seen.contains(key)) continue;
            seen.add(key);
            menu.addItem(id, d.name + "  [" + d.pluginFormatName + "]");
            indexed.push_back(d);
            ++id;
        }
        if (indexed.empty())
            menu.addItem(juce::PopupMenu::Item("(no cached instrument plugins - click Re-scan)").setEnabled(false));
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
                    }
                    else
                    {
                        c.builtInInstrument = "piano";
                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                            "Stratum Piano VST not loaded",
                            err + "\n\nUsing the emergency built-in piano fallback for now.");
                    }
                    const int newIdx = (int)chs.size();
                    c.mixerTrack = newIdx;
                    chs.push_back(std::move(c));
                    if (slotId >= 0)
                    {
                        pluginHost_.setSlotTrack(slotId, newIdx);
                        pluginHost_.showEditor(slotId, true);
                    }
                    channelRack_->repaint();
                    if (channelRack_->onChannelsChanged) channelRack_->onChannelsChanged();
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
                    }
                    else
                    {
                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                            "Stratum Guitar VST not loaded",
                            err);
                    }
                    const int newIdx = (int)chs.size();
                    c.mixerTrack = newIdx;
                    chs.push_back(std::move(c));
                    if (slotId >= 0)
                    {
                        pluginHost_.setSlotTrack(slotId, newIdx);
                        pluginHost_.showEditor(slotId, true);
                    }
                    channelRack_->repaint();
                    if (channelRack_->onChannelsChanged) channelRack_->onChannelsChanged();
                    return;
                }

                if (chosen == 8003) {
                    auto& chs = channelRack_->getChannels();
                    ChannelRack::Channel c;
                    c.name = "Stratum Bass";
                    c.type = ChannelRack::InstrumentType::Bass;
                    c.steps = std::vector<bool>((size_t)channelRack_->getTotalSteps(), false);
                    c.volume = 0.85f;
                    c.builtInInstrument = "bass";
                    const int newIdx = (int)chs.size();
                    c.mixerTrack = newIdx;
                    chs.push_back(std::move(c));
                    channelRack_->repaint();
                    if (channelRack_->onChannelsChanged) channelRack_->onChannelsChanged();
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
                    const int newIdx = (int)chs.size();
                    c.mixerTrack = newIdx;
                    chs.push_back(std::move(c));
                    pluginHost_.setSlotTrack(slotId, newIdx);
                    channelRack_->repaint();
                    if (channelRack_->onChannelsChanged) channelRack_->onChannelsChanged();
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

            if (ch.builtInInstrument == "bass")
            {
                const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
                const double secondsPerStep = 60.0 / juce::jmax(1.0, bpm) / 4.0;
                pluginHost_.playSynthBass(pitch, now, secondsPerStep * safeLength, channelVelocity,
                                          ch.mixerTrack >= 0 ? ch.mixerTrack : selectedChannel);
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
    playlist_->setPatternDefaultSteps(channelRack_->getTotalSteps());
    playlist_->onPlayheadSeek = [this](int absoluteStep) {
        pluginHost_.stopSamplePlaybackImmediate();
        if (channelRack_)
            channelRack_->setAbsoluteStep(absoluteStep);
        if (pianoRoll_)
            pianoRoll_->setPlayhead(absoluteStep, false, transportBar_->getBPM());
    };
    playlist_->onOpenAIAssistant = [this]() {
        if (!aiPanel_)
            return;
        if (aiPanel_->isVisible())
            aiPanel_->toFront(true);
        else
            openAiPanel(AiPanelMode::SidePanel);
    };
    playlist_->isAiAssistantOpen = [this]() {
        return aiPanel_ && aiPanel_->isVisible();
    };
    pianoRoll_->onPlayheadSeek = [this](int absoluteStep) {
        pluginHost_.stopSamplePlaybackImmediate();
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
    channelRack_->getPlaybackStepForChannel = [this](int absoluteStep, int patternSteps, int channelIndex) {
        if (playbackMode_ == TransportBar::PlaybackMode::Rack || centerView_ == CenterView::PianoRoll)
            return patternSteps > 0 ? absoluteStep % patternSteps : 0;
        return playlist_ ? playlist_->patternLocalStepForChannelAt(absoluteStep, patternSteps, channelIndex) : -1;
    };
    channelRack_->shouldPlayChannelAtStep = [this](int absoluteStep, int channelIndex) {
        if (playbackMode_ == TransportBar::PlaybackMode::Rack || centerView_ == CenterView::PianoRoll)
            return true;
        return playlist_ ? playlist_->patternAllowsChannelAtStep(absoluteStep, channelIndex) : true;
    };
    channelRack_->isPlaylistPlaybackActive = [this]() {
        return playbackMode_ == TransportBar::PlaybackMode::Playlist
            && centerView_ != CenterView::PianoRoll;
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
    
    // Wire up SAVE, OPEN, EXPORT, NEW PROJECT buttons
    transportBar_->onSave = [this](){
        if (currentProjectFile_.existsAsFile())
            saveProject(currentProjectFile_);
        else
            saveProjectAs();
    };
    transportBar_->onOpen = [this](){ openProjectFile(); };
    transportBar_->onExport = [this](){
        // Export menu: project files and WAV bounces are implemented; MIDI is
        // queued for a future build.
        juce::PopupMenu m;
        m.addItem (1, "Export Project (.stratum)");
        m.addItem (2, "Export Pattern as MIDI (.mid)", false);   // disabled
        m.addItem (3, "Export Audio (.wav)");
        m.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent (transportBar_.get()),
            [this](int chosen){
                if (chosen == 1) saveProjectAs();
                if (chosen == 3) exportAudioAs();
            });
    };
    transportBar_->onNewProject = [this](){
        newProject();
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
        if (videoPanel_->isEmbeddedInSession())
        {
            videoPanel_->unembedPlayerFromSession();
            bottomDock_->setSessionVideoMode(false);
        }
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
        if (aiPanel_->isVisible())
            closeAiPanel();
        else
            openAiPanel(AiPanelMode::SidePanel);
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
    channelRack_->onDrumGenreButtonClicked = [this](const juce::String& presetId, const juce::String& presetLabel) {
        if (presetId.isEmpty() || presetId.equalsIgnoreCase("none") || presetId.equalsIgnoreCase("empty"))
            return;

        const juce::String label = presetLabel.isNotEmpty() ? presetLabel : presetId;
        if (aiPanel_)
            aiPanel_->addUserMessage("Make a " + label + " pattern");

        if (aiPanel_ && aiPanel_->onPreset)
            aiPanel_->onPreset(presetId, label);

        if (aiPanel_)
            aiPanel_->addAssistantMessage("Done! Loaded a " + label + " drum pattern.");
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
        closeAiPanel();
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
            // Clear leftover preview/scrub voices before the rack fires step 0.
            pluginHost_.stopSamplePlaybackImmediate();
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
            pluginHost_.stopSamplePlaybackImmediate();
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
        exportAudioAs();
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

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (!videoPanel_ || !videoPanel_->isVisible())
        return false;
    return VideoPanel::canAcceptVideoFiles(files);
}

void MainComponent::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    if (!videoPanel_ || !videoPanel_->isVisible())
        return;

    const auto pos = juce::Point<int>(x, y);
    if (videoPanel_->getBounds().expanded(12).contains(pos))
    {
        const auto local = videoPanel_->getLocalPoint(this, pos);
        videoPanel_->fileDragEnter(files, local.x, local.y);
    }
}

void MainComponent::fileDragExit(const juce::StringArray&)
{
    notifyVideoFileDragExit();
}

void MainComponent::filesDropped(const juce::StringArray& files, int x, int y)
{
    deliverVideoFileDrop(files, { x, y });
}

void MainComponent::deliverVideoFileDrop(const juce::StringArray& files, juce::Point<int> localPos)
{
    if (!videoPanel_ || !videoPanel_->isVisible())
        return;
    if (!VideoPanel::canAcceptVideoFiles(files))
        return;

    if (videoPanel_->getBounds().expanded(16).contains(localPos))
        videoPanel_->handleFileDrop(files);
}

void MainComponent::notifyVideoFileDragEnter(const juce::StringArray& files)
{
    if (videoPanel_ && videoPanel_->isVisible())
        videoPanel_->fileDragEnter(files, 0, 0);
}

void MainComponent::notifyVideoFileDragExit()
{
    if (videoPanel_)
        videoPanel_->fileDragExit({});
}

bool MainComponent::shouldDropFilesWhenDraggedExternally(
    const juce::DragAndDropTarget::SourceDetails& sourceDetails,
    juce::StringArray& files,
    bool& canMoveFiles)
{
    juce::StringArray lines;
    lines.addLines(sourceDetails.description.toString());

    if (lines.size() < 3 || !lines[0].equalsIgnoreCase("audio"))
        return false;

    const auto file = juce::File(lines[2]);
    if (!file.existsAsFile())
        return false;

#ifdef _WIN32
    keepChordifyAboveStratumDuringExternalDrop();
#endif

    const bool isLoopLibrary = lines[1].equalsIgnoreCase("LOOPS")
                            || lines[1].equalsIgnoreCase("Loops");
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto path = file.getFullPathName();
    if (playlist_ != nullptr
        && (path != lastExternalBrowserAudioPath_ || nowMs - lastExternalBrowserAudioMs_ > 1000.0))
    {
        playlist_->addAudioFileFromExternalBrowserDrag(file, isLoopLibrary);
        lastExternalBrowserAudioPath_ = path;
        lastExternalBrowserAudioMs_ = nowMs;
    }

    files.add(file.getFullPathName());
    canMoveFiles = false;
    return true;
}

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

    if (projectOpenOverlay_)
        projectOpenOverlay_->setBounds(getLocalBounds());

    if (aiPanel_ && aiPanel_->isVisible())
    {
        if (aiPanelMode_ == AiPanelMode::SidePanel)
            aiPanel_->setBounds(getAiSidePanelBounds());
        else
            aiPanel_->setBounds(getAiFloatingBounds());
        aiPanel_->toFront(false);
    }

    if (projectOpenOverlay_)
        projectOpenOverlay_->setBounds(getLocalBounds());
    if (projectSaveOverlay_)
        projectSaveOverlay_->setBounds(getLocalBounds());
}

juce::Rectangle<int> MainComponent::getAiFloatingBounds() const
{
    const int pw = juce::jmin(880, juce::jmax(360, getWidth() - 160));
    const int ph = juce::jmin(640, juce::jmax(420, getHeight() - 120));
    return { (getWidth() - pw) / 2, (getHeight() - ph) / 2, pw, ph };
}

juce::Rectangle<int> MainComponent::getAiSidePanelBounds() const
{
    const int titleH = 28;
    const int transportH = 60;
    const int dockH = juce::jmax(130, (int)(getHeight() * 0.15));
    const int top = titleH + transportH;
    const int bottom = getHeight() - dockH;
    const int sideW = juce::jlimit(340, 520, (int)std::round(getWidth() * 0.28));
    return { getWidth() - sideW - 10, top + 8, sideW, juce::jmax(360, bottom - top - 16) };
}

void MainComponent::showAiOpenModeMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("Open AI Assistant");
    menu.addItem(1, "Floating window");
    menu.addItem(2, "Right side panel");

    menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(bottomDock_.get()),
        [this](int chosen)
        {
            if (chosen == 1) openAiPanel(AiPanelMode::Floating);
            if (chosen == 2) openAiPanel(AiPanelMode::SidePanel);
        });
}

void MainComponent::openAiPanel(AiPanelMode mode)
{
    aiPanelMode_ = mode;
    auto& anim = juce::Desktop::getInstance().getAnimator();
    const auto target = mode == AiPanelMode::SidePanel ? getAiSidePanelBounds() : getAiFloatingBounds();
    const auto start = mode == AiPanelMode::SidePanel ? target.translated(40, 0) : target.translated(0, 30);

    aiPanel_->setBounds(start);
    aiPanel_->setAlpha(0.0f);
    aiPanel_->setVisible(true);
    aiPanel_->toFront(true);
    if (bottomDock_)
        bottomDock_->setSelectedButton(5);
    if (playlist_)
        playlist_->repaint();
    anim.animateComponent(aiPanel_.get(), target, 1.0f, 210, false, 1.0, 0.0);
}

void MainComponent::closeAiPanel()
{
    if (!aiPanel_ || !aiPanel_->isVisible())
        return;

    if (bottomDock_)
        bottomDock_->setSelectedButton(-1);
    if (playlist_)
        playlist_->repaint();

    auto& anim = juce::Desktop::getInstance().getAnimator();
    const auto target = aiPanelMode_ == AiPanelMode::SidePanel
        ? aiPanel_->getBounds().translated(40, 0)
        : aiPanel_->getBounds().translated(0, 30);
    anim.animateComponent(aiPanel_.get(), target, 0.0f, 160, false, 0.0, 1.0);
    juce::Timer::callAfterDelay(170, [this]
    {
        if (aiPanel_)
            aiPanel_->setVisible(false);
        if (playlist_)
            playlist_->repaint();
    });
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

void MainComponent::newProject()
{
    if (transportBar_)
        transportBar_->stop();
    pluginHost_.clearTransientPlayback();
    pluginWindows_.clear();
    nativeEffectWindows_.clear();

    if (channelRack_)
    {
        for (const auto& ch : channelRack_->getChannels())
            if (ch.pluginSlotId >= 0)
                pluginHost_.unloadPlugin(ch.pluginSlotId);
    }

    auto makeTransport = []()
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("bpm", 130.0);
        juce::Array<juce::var> patterns;
        patterns.add("Pattern 1");
        obj->setProperty("patterns", patterns);
        obj->setProperty("currentPattern", 0);
        obj->setProperty("playbackMode", "rack");
        return juce::var(obj);
    };

    auto makeChannel = [](const juce::String& name, int type, std::initializer_list<int> activeSteps)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", name);
        obj->setProperty("type", type);
        obj->setProperty("muted", false);
        obj->setProperty("solo", false);
        obj->setProperty("volume", 1.0);
        obj->setProperty("pan", 0.0);
        obj->setProperty("mixerTrack", -1);
        obj->setProperty("sampleFile", "");
        obj->setProperty("pluginSlotId", -1);
        obj->setProperty("builtInInstrument", "");

        juce::Array<juce::var> steps;
        for (int i = 0; i < 16; ++i)
            steps.add(std::find(activeSteps.begin(), activeSteps.end(), i) != activeSteps.end());
        obj->setProperty("steps", steps);
        obj->setProperty("notes", juce::Array<juce::var>());
        return juce::var(obj);
    };

    auto* rack = new juce::DynamicObject();
    rack->setProperty("totalSteps", 16);
    rack->setProperty("drumPresetId", "none");
    rack->setProperty("drumSwingId", "none");
    juce::Array<juce::var> channels;
    channels.add(makeChannel("Kick", 0, { 0, 4, 8, 12 }));
    channels.add(makeChannel("Snare", 1, { 4, 12 }));
    channels.add(makeChannel("Hihat", 2, { 2, 6, 10, 14 }));
    channels.add(makeChannel("Clap", 3, {}));
    rack->setProperty("channels", channels);

    auto makeMixerTrack = [](const juce::String& name, float volume)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", name);
        obj->setProperty("volume", volume);
        obj->setProperty("pan", 0.0);
        obj->setProperty("reverbSend", 0.0);
        obj->setProperty("muted", false);
        obj->setProperty("solo", false);
        return juce::var(obj);
    };

    auto* mixerObj = new juce::DynamicObject();
    juce::Array<juce::var> tracks;
    tracks.add(makeMixerTrack("Kick", 0.8f));
    tracks.add(makeMixerTrack("Snare", 0.8f));
    tracks.add(makeMixerTrack("Hihat", 0.8f));
    tracks.add(makeMixerTrack("Clap", 0.8f));
    tracks.add(makeMixerTrack("Master", 0.8f));
    mixerObj->setProperty("tracks", tracks);

    auto* playlistObj = new juce::DynamicObject();
    playlistObj->setProperty("numTracks", 30);
    playlistObj->setProperty("zoomX", 1.0);
    playlistObj->setProperty("patternStripCollapsed", true);
    playlistObj->setProperty("currentPatternName", "Pattern 1");
    playlistObj->setProperty("patternDefaultSteps", 16);
    playlistObj->setProperty("clips", juce::Array<juce::var>());

    if (transportBar_) transportBar_->fromJson(makeTransport());
    if (channelRack_)  channelRack_->fromJson(juce::var(rack));
    if (mixer_)        mixer_->fromJson(juce::var(mixerObj));
    if (playlist_)     playlist_->fromJson(juce::var(playlistObj));
    if (channelRack_ && channelRack_->onChannelsChanged)
        channelRack_->onChannelsChanged();

    currentProjectFile_ = {};
    undoStack_.clear();
    redoStack_.clear();
    lastSnapshotJson_ = captureSnapshotJson();
    setCenterView(CenterView::Playlist);
    repaint();
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
    if (channelRack_ && channelRack_->onChannelsChanged)
        channelRack_->onChannelsChanged();

    currentProjectFile_ = f;
    repaint();
    return true;
}

void MainComponent::exportAudioAs()
{
    showExportAudioModal();
}

void MainComponent::showExportAudioModal()
{
    auto root = juce::File("D:\\stratumdaw");
    root.createDirectory();

    auto baseName = currentProjectFile_.existsAsFile()
        ? currentProjectFile_.getFileNameWithoutExtension()
        : juce::String("Untitled");

    auto overlay = std::make_unique<ProjectSaveOverlay>(root, baseName);
    overlay->setBounds(getLocalBounds());
    overlay->onClose = [this]()
    {
        projectSaveOverlay_.reset();
        repaint();
    };
    overlay->onSave = [this, root](const juce::String& cleanName)
    {
        const auto wavFile = root.getChildFile(cleanName).withFileExtension(".wav");
        const auto projectFile = root.getChildFile(cleanName).withFileExtension(kProjectExt);

        if (exportAudioToFile(wavFile))
        {
            saveProject(projectFile);
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Export complete",
                "WAV exported:\n" + wavFile.getFullPathName() + "\n\nProject saved:\n" +
                projectFile.getFullPathName());
        }

        projectSaveOverlay_.reset();
    };

    addAndMakeVisible(overlay.get());
    overlay->toFront(true);
    overlay->grabKeyboardFocus();
    projectSaveOverlay_ = std::move(overlay);
}

bool MainComponent::exportAudioToFile(const juce::File& wavFile)
{
    if (!transportBar_ || !channelRack_)
        return false;

    const bool wasPlaying = transportBar_->isPlaying();
    if (wasPlaying)
        transportBar_->stop();

    auto& channels = channelRack_->getChannels();
    for (auto& ch : channels)
    {
        if (ch.builtInInstrument == "piano" && ch.pluginSlotId < 0)
        {
            juce::String err;
            const int slotId = loadBundledStratumPiano(pluginHost_, err);
            if (slotId >= 0)
            {
                ch.pluginSlotId = slotId;
                ch.builtInInstrument.clear();
                ch.name = "Stratum Piano";
            }
        }
        else if (ch.builtInInstrument == "guitar" && ch.pluginSlotId < 0)
        {
            juce::String err;
            const int slotId = loadBundledStratumGuitar(pluginHost_, err);
            if (slotId >= 0)
            {
                ch.pluginSlotId = slotId;
                ch.builtInInstrument.clear();
                ch.name = "Stratum Guitar";
            }
        }
    }

    const double sampleRate = juce::jmax(1.0, audioEngine_.getSampleRate());
    const int blockSize = juce::jlimit(64, 4096, audioEngine_.getBufferSize() > 0 ? audioEngine_.getBufferSize() : 512);
    const double bpm = juce::jlimit(20.0, 999.0, transportBar_->getBPM());
    const double secondsPerStep = 60.0 / bpm / 4.0;
    const int samplesPerStep = juce::jmax(1, (int)std::llround(secondsPerStep * sampleRate));
    const int totalSteps = juce::jmax(1, channelRack_->getTotalSteps());
    const bool exportPlaylist = playlist_ != nullptr
                             && playbackMode_ == TransportBar::PlaybackMode::Playlist
                             && playlist_->getContentEndBar() > 0.01f;

    auto isMelodicChannel = [](const ChannelRack::Channel& ch)
    {
        return ch.pluginSlotId >= 0
            || ch.builtInInstrument == "piano"
            || ch.builtInInstrument == "guitar"
            || ch.builtInInstrument == "bass"
            || ch.type == ChannelRack::InstrumentType::Lead
            || ch.type == ChannelRack::InstrumentType::Pad
            || ch.type == ChannelRack::InstrumentType::Bass;
    };

    auto channelPatternLength = [&](const ChannelRack::Channel& ch)
    {
        int len = juce::jmax(totalSteps, (int)ch.steps.size());
        if (isMelodicChannel(ch))
            for (const auto& n : ch.pianoRollNotes)
                len = juce::jmax(len, n.startStep + juce::jmax(1, n.lengthSteps));
        return juce::jmax(1, len);
    };

    float endBars = exportPlaylist && playlist_ ? playlist_->getContentEndBar()
                                                : (float)totalSteps / 16.0f;
    if (!exportPlaylist)
        for (const auto& ch : channels)
            endBars = juce::jmax(endBars, (float)channelPatternLength(ch) / 16.0f);

    endBars = juce::jlimit(0.25f, 512.0f, endBars);
    const double contentSeconds = (double)endBars * 240.0 / bpm;
    const double tailSeconds = 2.0;
    const juce::int64 totalSamples64 = (juce::int64)std::ceil((contentSeconds + tailSeconds) * sampleRate);
    if (totalSamples64 <= 0)
        return false;

    juce::TemporaryFile temp(wavFile);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream(temp.getFile().createOutputStream());
    if (!stream)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Export failed", "Couldn't write to:\n" + wavFile.getFullPathName());
        return false;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(stream.get(), sampleRate, 2, 24, {}, 0));
    if (!writer)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Export failed", "Couldn't create a WAV writer.");
        return false;
    }
    stream.release();

    struct PendingOff { juce::int64 sample = 0; int slot = -1; int pitch = 60; };
    std::vector<PendingOff> pendingOffs;

    auto sendDueMidiOffs = [&](juce::int64 samplePos)
    {
        for (auto& off : pendingOffs)
        {
            if (off.slot >= 0 && off.sample <= samplePos)
            {
                pluginHost_.sendMidiNote(off.slot, 1, off.pitch, 0, false);
                off.slot = -1;
            }
        }
        pendingOffs.erase(std::remove_if(pendingOffs.begin(), pendingOffs.end(),
                         [](const PendingOff& o){ return o.slot < 0; }),
                         pendingOffs.end());
    };

    struct ScheduledExportHit { juce::int64 sample = 0; int absoluteStep = 0; int channelIdx = -1; };
    std::vector<ScheduledExportHit> scheduledHits;

    auto triggerExportChannel = [&](int i, int absoluteStep, juce::int64 hitSample)
    {
        int localStep = absoluteStep % totalSteps;
        bool stepAllowed = true;
        if (exportPlaylist && playlist_)
        {
            localStep = playlist_->patternLocalStepAt(absoluteStep, totalSteps);
            stepAllowed = localStep >= 0;
        }
        if (!stepAllowed)
            return;
        if (exportPlaylist && playlist_ && !playlist_->patternAllowsChannelAtStep(absoluteStep, i))
            return;

        const auto& ch = channels[(size_t)i];
        if (ch.muted)
            return;

        const bool melodic = isMelodicChannel(ch);
        const int patternLen = channelPatternLength(ch);
        const int channelStep = melodic
            ? (exportPlaylist ? localStep : (absoluteStep % patternLen))
            : (ch.steps.empty() ? localStep : (localStep % (int)ch.steps.size()));

        std::vector<ChannelRack::Channel::Note> notes;
        if (melodic)
        {
            for (const auto& n : ch.pianoRollNotes)
                if (n.startStep == channelStep)
                    notes.push_back(n);
        }
        else
        {
            if (channelStep >= 0 && channelStep < (int)ch.steps.size() && ch.steps[(size_t)channelStep])
                notes.push_back({ ChannelRack::DEFAULT_DRUM_PITCH, channelStep, 1, 100 });
        }

        if (notes.empty())
            return;

        const int track = (ch.mixerTrack >= 0) ? ch.mixerTrack : i;
        if (ch.pluginSlotId >= 0)
        {
            const int minRealFeelSteps = pianoRealFeel_ ? 6 : 1;
            for (const auto& n : notes)
            {
                const int pitch = juce::jlimit(0, 127, n.pitch);
                const int velocity = juce::jlimit(1, 127, (int)std::round(ch.volume * (float)n.velocity));
                const int holdSteps = juce::jmax(minRealFeelSteps, n.lengthSteps);
                pluginHost_.setSlotTrack(ch.pluginSlotId, track);
                pluginHost_.sendMidiNote(ch.pluginSlotId, 1, pitch, velocity, true);
                pendingOffs.push_back({ hitSample + (juce::int64)holdSteps * samplesPerStep, ch.pluginSlotId, pitch });
            }
            return;
        }

        if (ch.builtInInstrument == "piano" || ch.builtInInstrument == "guitar")
        {
            const int minRealFeelSteps = pianoRealFeel_ ? 6 : 1;
            for (const auto& n : notes)
            {
                const float velocity = juce::jlimit(0.0f, 1.0f, ch.volume * ((float)n.velocity / 127.0f));
                pluginHost_.playSynthPiano(n.pitch, 0.0, secondsPerStep * juce::jmax(minRealFeelSteps, n.lengthSteps), velocity, track);
            }
            return;
        }

        if (ch.builtInInstrument == "bass")
        {
            for (const auto& n : notes)
            {
                const float velocity = juce::jlimit(0.0f, 1.0f, ch.volume * ((float)n.velocity / 127.0f));
                pluginHost_.playSynthBass(n.pitch, 0.0, secondsPerStep * juce::jmax(1, n.lengthSteps), velocity, track);
            }
            return;
        }

        if (ch.sampleFile.existsAsFile())
        {
            const double swingOffset = channelRack_
                ? channelRack_->getSwingDelaySeconds(channelStep, ch) : 0.0;
            pluginHost_.playSampleFile(ch.sampleFile, track, swingOffset, ch.volume);
        }
    };

    auto queueStepHits = [&](int absoluteStep, juce::int64 stepSample)
    {
        int localStep = absoluteStep % totalSteps;
        bool stepAllowed = true;
        if (exportPlaylist && playlist_)
        {
            localStep = playlist_->patternLocalStepAt(absoluteStep, totalSteps);
            stepAllowed = localStep >= 0;
        }
        if (!stepAllowed)
            return;

        for (int i = 0; i < (int)channels.size(); ++i)
        {
            if (exportPlaylist && playlist_ && !playlist_->patternAllowsChannelAtStep(absoluteStep, i))
                continue;

            const auto& ch = channels[(size_t)i];
            if (ch.muted)
                continue;

            const bool melodic = isMelodicChannel(ch);
            const int patternLen = channelPatternLength(ch);
            const int channelStep = melodic
                ? (exportPlaylist ? localStep : (absoluteStep % patternLen))
                : (ch.steps.empty() ? localStep : (localStep % (int)ch.steps.size()));

            bool hasHit = false;
            if (melodic)
            {
                for (const auto& n : ch.pianoRollNotes)
                    if (n.startStep == channelStep) { hasHit = true; break; }
            }
            else if (channelStep >= 0 && channelStep < (int)ch.steps.size())
                hasHit = ch.steps[(size_t)channelStep];

            if (!hasHit)
                continue;

            const double swingDelay = channelRack_
                ? channelRack_->getSwingDelaySeconds(channelStep, ch) : 0.0;
            const juce::int64 hitSample = stepSample
                + (juce::int64)std::llround(swingDelay * sampleRate);
            scheduledHits.push_back({ hitSample, absoluteStep, i });
        }
    };

    const juce::ScopedLock renderGuard(pluginHost_.getRenderLock());
    pluginHost_.clearTransientPlayback();
    if (playlist_)
    {
        playlist_->setPlaybackEnabled(exportPlaylist);
        playlist_->setAbsoluteStep(0);
    }
    if (channelRack_)
        channelRack_->setAbsoluteStep(0);

    juce::AudioBuffer<float> block(2, blockSize);
    juce::int64 samplePos = 0;
    int nextStep = 0;
    while (samplePos < totalSamples64)
    {
        const int n = (int)juce::jmin<juce::int64>(blockSize, totalSamples64 - samplePos);
        block.setSize(2, n, false, false, true);
        block.clear();

        while ((juce::int64)nextStep * samplesPerStep <= samplePos && (double)nextStep * secondsPerStep <= contentSeconds + 0.0001)
        {
            const juce::int64 stepSample = (juce::int64)nextStep * samplesPerStep;
            if (exportPlaylist && playlist_)
                playlist_->setPlayhead(nextStep, true, bpm);
            queueStepHits(nextStep, stepSample);
            ++nextStep;
        }

        for (const auto& hit : scheduledHits)
        {
            if (hit.sample >= samplePos && hit.sample < samplePos + n)
                triggerExportChannel(hit.channelIdx, hit.absoluteStep, hit.sample);
        }
        scheduledHits.erase(
            std::remove_if(scheduledHits.begin(), scheduledHits.end(),
                [&](const ScheduledExportHit& h) { return h.sample < samplePos + n; }),
            scheduledHits.end());

        sendDueMidiOffs(samplePos);
        float* outs[2] = { block.getWritePointer(0), block.getWritePointer(1) };
        pluginHost_.renderAudioBlock(outs, 2, n);
        writer->writeFromAudioSampleBuffer(block, 0, n);
        samplePos += n;
    }

    for (auto& off : pendingOffs)
        if (off.slot >= 0)
            pluginHost_.sendMidiNote(off.slot, 1, off.pitch, 0, false);
    pluginHost_.clearTransientPlayback();

    if (playlist_)
        playlist_->setPlayhead(0, false, bpm);
    if (channelRack_)
        channelRack_->setAbsoluteStep(0);

    writer.reset();
    if (!temp.overwriteTargetFileWithTemporary())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Export failed", "Couldn't replace:\n" + wavFile.getFullPathName());
        return false;
    }

    channelRack_->repaint();
    if (playlist_) playlist_->repaint();
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
    auto overlay = std::make_unique<ProjectOpenOverlay>(juce::File("D:\\stratumdaw"));
    overlay->setBounds(getLocalBounds());
    overlay->onClose = [this]()
    {
        projectOpenOverlay_.reset();
        repaint();
    };
    overlay->onOpen = [this](const juce::File& file)
    {
        if (file.existsAsFile())
            loadProject(file);
        projectOpenOverlay_.reset();
    };

    addAndMakeVisible(overlay.get());
    overlay->toFront(true);
    overlay->grabKeyboardFocus();
    projectOpenOverlay_ = std::move(overlay);
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
    if (channelRack_ && channelRack_->onChannelsChanged)
        channelRack_->onChannelsChanged();
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

