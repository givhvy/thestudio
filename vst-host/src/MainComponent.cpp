#include "MainComponent.h"
#include "Theme.h"
#include "PianoRoll.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <thread>
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

namespace
{
juce::Font titleBarBadgeFont()
{
    return juce::Font(juce::FontOptions().withName("Consolas").withHeight(7.5f).withStyle("Bold"));
}

int titleBarBadgeWidthForText(const juce::String& text)
{
    return juce::jmax(38, (int)std::ceil(titleBarBadgeFont().getStringWidthFloat(text)) + 10);
}

void drawTitleBarBadgeBackground(juce::Graphics& g, juce::Rectangle<float> badge,
                                 bool highlighted = false, bool down = false)
{
    g.setColour(juce::Colour(0xff0a0a0c));
    g.fillRoundedRectangle(badge, 2.5f);
    if (highlighted || down)
    {
        g.setColour(juce::Colour(0xff161618));
        g.fillRoundedRectangle(badge, 2.5f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawHorizontalLine((int)badge.getY() + 1, badge.getX() + 2, badge.getRight() - 2);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(badge.reduced(0.5f), 2.5f, 0.6f);
}

void drawTitleBarBadgeText(juce::Graphics& g, juce::Rectangle<float> badge, const juce::String& text)
{
    g.setColour(juce::Colour(0xff71717a));
    g.setFont(titleBarBadgeFont());
    g.drawText(text, badge.toNearestInt(), juce::Justification::centred);
}

juce::File cloudUploadConfigFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stratum DAW")
        .getChildFile("cloud-upload.json");
}

juce::String getCloudUploadEndpoint()
{
    const auto cfg = cloudUploadConfigFile();
    if (cfg.existsAsFile())
    {
        const auto parsed = juce::JSON::parse(cfg);
        if (auto* obj = parsed.getDynamicObject())
        {
            const auto endpoint = obj->getProperty("endpoint").toString().trim();
            if (endpoint.isNotEmpty())
                return endpoint;
        }
    }
    return "http://localhost:8080/api/beatstars/publish";
}

juce::String getBeatstarsJobEndpoint(const juce::String& jobId)
{
    return "http://localhost:8080/api/jobs/" + jobId;
}

juce::File getBrowserControlDir()
{
    const auto cfg = cloudUploadConfigFile();
    if (cfg.existsAsFile())
    {
        const auto parsed = juce::JSON::parse(cfg);
        if (auto* obj = parsed.getDynamicObject())
        {
            const auto dir = obj->getProperty("browserControlDir").toString().trim();
            if (dir.isNotEmpty())
                return juce::File(dir);
        }
    }
    return juce::File("F:\\PlaygroundTest\\BrowserControl");
}

juce::File getBrowserControlLauncher()
{
    const auto cfg = cloudUploadConfigFile();
    if (cfg.existsAsFile())
    {
        const auto parsed = juce::JSON::parse(cfg);
        if (auto* obj = parsed.getDynamicObject())
        {
            const auto launcher = obj->getProperty("browserControlVbs").toString().trim();
            if (launcher.isNotEmpty())
                return juce::File(launcher);
        }
    }
    return juce::File("C:\\Users\\ADMIN\\Desktop\\BrowserControl.vbs");
}

bool pingBrowserControl()
{
    auto url = juce::URL("http://localhost:8080/api/health");
    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs(2500)
                             .withExtraHeaders("Accept: application/json");
    if (auto stream = url.createInputStream(options))
        return stream->readEntireStreamAsString().contains("\"ok\":true");
    return false;
}

void launchBrowserControlIfNeeded()
{
    if (pingBrowserControl())
        return;

    const auto dir = getBrowserControlDir();
    juce::MessageManager::callAsync([dir]()
    {
        juce::Process::openDocument("cmd.exe",
            "/c start /min cmd /k \"cd /d \"" + dir.getFullPathName() + "\" && npm start\"");
    });

    for (int i = 0; i < 45; ++i)
    {
        juce::Thread::sleep(1000);
        if (pingBrowserControl())
            return;
    }
}

bool cloudUploadResponseOk(const juce::String& response)
{
    const auto parsed = juce::JSON::parse(response);
    if (auto* obj = parsed.getDynamicObject())
        return static_cast<bool>(obj->getProperty("ok"));
    return response.contains("\"ok\":true") || response.contains("\"ok\": true");
}

struct BeatstarsUploadResult
{
    bool ok = false;
    juce::String error;
};

BeatstarsUploadResult uploadBeatViaBrowserControl(const juce::String& endpoint,
                                                  const juce::File& wavFile,
                                                  const juce::String& title,
                                                  double bpm,
                                                  const juce::StringArray& stemPaths,
                                                  const std::function<void(const juce::String&)>& onStatus)
{
    BeatstarsUploadResult result;

    if (!wavFile.existsAsFile())
    {
        result.error = "WAV file not found: " + wavFile.getFullPathName();
        return result;
    }

    launchBrowserControlIfNeeded();
    if (!pingBrowserControl())
    {
        result.error = "BrowserControl is not running on port 8080.\nStart BrowserControl and try again.";
        return result;
    }

    juce::DynamicObject::Ptr payload(new juce::DynamicObject());
    payload->setProperty("filePath", wavFile.getFullPathName());
    payload->setProperty("title", title);
    payload->setProperty("bpm", bpm);
    payload->setProperty("publish", true);

    juce::Array<juce::var> stems;
    for (const auto& stem : stemPaths)
        stems.add(stem);
    payload->setProperty("stemPaths", stems);

    const auto body = juce::JSON::toString(juce::var(payload.get()));
    auto submitUrl = juce::URL(endpoint).withPOSTData(body);
    const auto submitOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                   .withConnectionTimeoutMs(30000)
                                   .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json");

    juce::String jobId;
    if (auto stream = submitUrl.createInputStream(submitOptions))
    {
        const auto response = stream->readEntireStreamAsString();
        const auto parsed = juce::JSON::parse(response);
        if (auto* obj = parsed.getDynamicObject())
            jobId = obj->getProperty("jobId").toString();
        if (jobId.isEmpty() && cloudUploadResponseOk(response))
        {
            result.ok = true;
            return result;
        }
    }

    if (jobId.isEmpty())
    {
        result.error = "Could not queue publish job at " + endpoint
                       + "\nCheck that BrowserControl server is running.";
        return result;
    }

    onStatus("BeatStars job queued...");

    for (int attempt = 0; attempt < 300; ++attempt)
    {
        juce::Thread::sleep(2000);

        auto statusUrl = juce::URL(getBeatstarsJobEndpoint(jobId));
        const auto pollOptions = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                     .withConnectionTimeoutMs(10000)
                                     .withExtraHeaders("Accept: application/json");

        if (auto pollStream = statusUrl.createInputStream(pollOptions))
        {
            const auto pollResponse = pollStream->readEntireStreamAsString();
            const auto pollParsed = juce::JSON::parse(pollResponse);
            if (auto* jobObj = pollParsed.getDynamicObject())
            {
                if (auto* job = jobObj->getProperty("job").getDynamicObject())
                {
                    const auto status = job->getProperty("status").toString();
                    const auto message = job->getProperty("message").toString();
                    const int progress = (int) job->getProperty("progress");

                    if (message.isNotEmpty())
                        onStatus(message + (progress > 0 ? " (" + juce::String(progress) + "%)" : ""));

                    if (status == "completed")
                    {
                        result.ok = true;
                        return result;
                    }
                    if (status == "failed")
                    {
                        const auto err = job->getProperty("error").toString();
                        result.error = err.isNotEmpty() ? err : juce::String("BeatStars job failed");
                        onStatus(result.error);
                        return result;
                    }
                }
            }
        }
    }

    result.error = "BeatStars upload timed out after 10 minutes.";
    onStatus(result.error);
    return result;
}

class TitleBarBadgeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                              bool highlighted, bool down) override
    {
        drawTitleBarBadgeBackground(g, button.getLocalBounds().toFloat().reduced(0.5f),
                                    highlighted, down);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        drawTitleBarBadgeText(g, button.getLocalBounds().toFloat(), button.getButtonText());
    }
};

static TitleBarBadgeLookAndFeel titleBarBadgeLaf;

class LoopPickerCallout final : public juce::Component
{
public:
    LoopPickerCallout(std::vector<juce::File> loops,
                      double targetBpm,
                      PluginHost& host,
                      std::function<void(const juce::File&)> onConfirm)
        : loops_(std::move(loops)),
          targetBpm_(targetBpm),
          pluginHost_(host),
          onConfirm_(std::move(onConfirm))
    {
        const int h = juce::jlimit(260, 560, 120 + (int)loops_.size() * rowH_);
        setSize(520, h);
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        const int maxScroll = juce::jmax(0, (int)loops_.size() * rowH_ - listRect_.getHeight());
        scrollY_ = juce::jlimit(0, maxScroll, scrollY_ - (int)std::round(wheel.deltaY * 80.0f));
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (confirmRect_.contains(e.getPosition()))
        {
            if (selectedIndex_ >= 0 && selectedIndex_ < (int)loops_.size() && onConfirm_)
                onConfirm_(loops_[(size_t)selectedIndex_]);
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
            return;
        }

        if (!listRect_.contains(e.getPosition()))
            return;

        const int idx = (e.y - listRect_.getY() + scrollY_) / rowH_;
        if (idx < 0 || idx >= (int)loops_.size())
            return;

        selectedIndex_ = idx;
        pluginHost_.playSamplePreview(loops_[(size_t)idx]);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff121216));

        auto area = getLocalBounds().reduced(14);
        g.setColour(Theme::accentBright);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
        g.drawText("Loops near " + juce::String((int)targetBpm_) + " BPM",
                   area.removeFromTop(22), juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
        g.drawText("Click a loop to preview · Confirm to add to Playlist",
                   area.removeFromTop(20), juce::Justification::centredLeft, true);

        confirmRect_ = area.removeFromBottom(34);
        area.removeFromBottom(8);
        listRect_ = area;

        if (loops_.empty())
        {
            g.setColour(Theme::zinc500);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
            g.drawText("No loops found with BPM near " + juce::String((int)targetBpm_) + ".",
                       listRect_, juce::Justification::centred, true);
        }
        else
        {
            g.saveState();
            g.reduceClipRegion(listRect_);

            int y = listRect_.getY() - scrollY_;
            for (int i = 0; i < (int)loops_.size(); ++i)
            {
                const auto row = juce::Rectangle<int>(listRect_.getX(), y, listRect_.getWidth(), rowH_);
                if (row.getBottom() >= listRect_.getY() && row.getY() <= listRect_.getBottom())
                {
                    if (i == selectedIndex_)
                    {
                        g.setColour(Theme::accentBright.withAlpha(0.18f));
                        g.fillRoundedRectangle(row.toFloat().reduced(0, 1), 4.0f);
                    }

                    g.setColour(i == selectedIndex_ ? Theme::accentBright : Theme::zinc300);
                    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
                    g.drawText(loops_[(size_t)i].getFileName(), row.reduced(8, 0),
                               juce::Justification::centredLeft, true);
                }
                y += rowH_;
            }

            g.restoreState();
        }

        const bool canConfirm = selectedIndex_ >= 0 && selectedIndex_ < (int)loops_.size();
        g.setColour(canConfirm ? Theme::accentBright.withAlpha(0.22f) : juce::Colour(0xff1b1b20));
        g.fillRoundedRectangle(confirmRect_.toFloat(), 6.0f);
        g.setColour(canConfirm ? Theme::accentBright : Theme::zinc600);
        g.drawRoundedRectangle(confirmRect_.toFloat(), 6.0f, 1.2f);
        g.setColour(canConfirm ? juce::Colours::black : Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
        g.drawText("Add to Playlist", confirmRect_, juce::Justification::centred, false);
    }

private:
    std::vector<juce::File> loops_;
    double targetBpm_ = 120.0;
    PluginHost& pluginHost_;
    std::function<void(const juce::File&)> onConfirm_;
    int selectedIndex_ = -1;
    int scrollY_ = 0;
    int rowH_ = 24;
    juce::Rectangle<int> listRect_;
    juce::Rectangle<int> confirmRect_;
};
}

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
    ProjectSaveOverlay(juce::File rootFolder, juce::String defaultName, bool defaultStems = false)
        : rootFolder_(std::move(rootFolder)),
          exportStems_(defaultStems)
    {
        setWantsKeyboardFocus(true);
        rootFolder_.createDirectory();

        if (defaultName.trim().isEmpty() || defaultName.equalsIgnoreCase("Untitled"))
            defaultName = makeBeatName();
        nameEditor_.setText(defaultName, false);
        nameEditor_.setSelectAllWhenFocused(true);
        nameEditor_.setJustification(juce::Justification::centredLeft);
        nameEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff050507).withAlpha(0.70f));
        nameEditor_.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.12f));
        nameEditor_.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accentBright);
        nameEditor_.setColour(juce::TextEditor::textColourId, Theme::zinc100);
        addAndMakeVisible(nameEditor_);
    }

    std::function<void(const juce::String&, bool)> onSave;
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
        g.drawText(exportStems_ ? "Exports rack channels and loops into a stems folder in " + rootFolder_.getFullPathName()
                                : "Exports a .wav file into " + rootFolder_.getFullPathName(),
                   subtitleRect_, juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText("FILE NAME", labelRect_, juce::Justification::centredLeft, true);

        drawPill(g, closeRect_, "X", false);
        drawPill(g, refreshRect_, "REFRESH NAME", false);
        drawPill(g, wavModeRect_, "WAV", !exportStems_);
        drawPill(g, stemsModeRect_, "STEMS", exportStems_);
        drawPill(g, cancelRect_, "CANCEL", false);
        drawPill(g, saveRect_, exportStems_ ? "EXPORT STEMS" : "EXPORT WAV", true);

        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
        auto preview = makeCleanName(nameEditor_.getText());
        if (preview.isEmpty()) preview = "Untitled";
        g.drawText(exportStems_
                       ? rootFolder_.getFullPathName() + "\\" + preview + " stems\\"
                       : rootFolder_.getFullPathName() + "\\" + preview + ".wav",
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
        if (refreshRect_.contains(e.x, e.y))
        {
            nameEditor_.setText(makeBeatName(), false);
            repaint();
            return;
        }
        if (wavModeRect_.contains(e.x, e.y) || stemsModeRect_.contains(e.x, e.y))
        {
            exportStems_ = stemsModeRect_.contains(e.x, e.y);
            repaint();
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
    juce::Rectangle<int> closeRect_, refreshRect_, wavModeRect_, stemsModeRect_, cancelRect_, saveRect_;
    bool exportStems_ = false;

    static juce::String makeCleanName(juce::String name)
    {
        name = name.trim();
        for (auto c : juce::String(R"(\/:*?"<>|)"))
            name = name.replaceCharacter(c, '-');
        return name.trim();
    }

    static juce::String makeBeatName()
    {
        static const char* moods[] = { "Midnight", "Dusty", "Golden", "Velvet", "Neon", "Lowkey", "Afterhours", "Soulful", "Raw", "Cinematic" };
        static const char* nouns[] = { "Pocket", "Bounce", "Loop", "Knock", "Groove", "Motion", "Tape", "Drift", "Chops", "Pulse" };
        static const char* tags[] = { "Beat", "Idea", "Session", "Flip", "Draft", "Sketch" };
        auto& rng = juce::Random::getSystemRandom();
        return juce::String(moods[rng.nextInt((int)(sizeof(moods) / sizeof(moods[0])))]) + " "
             + nouns[rng.nextInt((int)(sizeof(nouns) / sizeof(nouns[0])))] + " "
             + tags[rng.nextInt((int)(sizeof(tags) / sizeof(tags[0])))];
    }

    void updateLayout()
    {
        const int width = juce::jlimit(440, 680, getWidth() - 80);
        const int height = 330;
        panel_ = juce::Rectangle<int>((getWidth() - width) / 2, (getHeight() - height) / 2, width, height);

        auto content = panel_.reduced(28, 24);
        closeRect_ = juce::Rectangle<int>(panel_.getRight() - 46, panel_.getY() + 18, 28, 26);
        titleRect_ = content.removeFromTop(30);
        subtitleRect_ = content.removeFromTop(42);
        auto modeRow = content.removeFromTop(26);
        refreshRect_ = modeRow.removeFromRight(128);
        wavModeRect_ = modeRow.removeFromLeft(72);
        stemsModeRect_ = modeRow.removeFromLeft(82).reduced(6, 0);
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
            onSave(clean, exportStems_);
    }
};

class CloudUploadOverlay : public juce::Component
{
public:
    struct UploadRequest
    {
        juce::String name;
        double bpm = 120.0;
        bool includeStems = false;
    };

    CloudUploadOverlay(juce::String defaultName, double defaultBpm, juce::String endpointHint)
        : endpointHint_(std::move(endpointHint))
    {
        setWantsKeyboardFocus(true);

        if (defaultName.trim().isEmpty() || defaultName.equalsIgnoreCase("Untitled"))
            defaultName = makeBeatName();

        auto styleEditor = [](juce::TextEditor& ed)
        {
            ed.setSelectAllWhenFocused(true);
            ed.setJustification(juce::Justification::centredLeft);
            ed.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff050507).withAlpha(0.70f));
            ed.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.12f));
            ed.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accentBright);
            ed.setColour(juce::TextEditor::textColourId, Theme::zinc100);
        };

        nameEditor_.setText(defaultName, false);
        styleEditor(nameEditor_);
        addAndMakeVisible(nameEditor_);

        bpmEditor_.setText(juce::String(defaultBpm, 1), false);
        bpmEditor_.setInputRestrictions(6, "0123456789.");
        styleEditor(bpmEditor_);
        addAndMakeVisible(bpmEditor_);
    }

    std::function<void(const UploadRequest&)> onUpload;
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
        g.drawText("Publish to BeatStars", titleRect_, juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f));
        g.drawText(includeStems_
                       ? "STEMS ON: BrowserControl zips stems and uploads to BeatStars Stem Files (.zip)."
                       : "BrowserControl uploads + creates track with your title and BPM.",
                   subtitleRect_, juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText("TRACK TITLE", labelRect_, juce::Justification::centredLeft, true);
        g.drawText("BPM", bpmLabelRect_, juce::Justification::centredLeft, true);

        drawPill(g, closeRect_, "X", false);
        drawPill(g, refreshRect_, "REFRESH NAME", false);
        drawPill(g, stemsRect_, includeStems_ ? "STEMS: ON" : "STEMS: OFF", includeStems_);
        drawPill(g, cancelRect_, "CANCEL", false);
        drawPill(g, uploadRect_, "PUBLISH ON BEATSTARS", true);

        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
        auto preview = makeCleanName(nameEditor_.getText());
        if (preview.isEmpty()) preview = "Untitled";
        g.drawText("BrowserControl: " + endpointHint_, endpointRect_, juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        updateLayout();
        nameEditor_.setBounds(titleEditorRect_);
        bpmEditor_.setBounds(bpmEditorRect_);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        updateLayout();
        if (closeRect_.contains(e.x, e.y) || cancelRect_.contains(e.x, e.y))
        {
            if (onClose) onClose();
            return;
        }
        if (refreshRect_.contains(e.x, e.y))
        {
            nameEditor_.setText(makeBeatName(), false);
            repaint();
            return;
        }
        if (stemsRect_.contains(e.x, e.y))
        {
            includeStems_ = !includeStems_;
            repaint();
            return;
        }
        if (uploadRect_.contains(e.x, e.y))
            upload();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey)
        {
            upload();
            return true;
        }
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose) onClose();
            return true;
        }
        return false;
    }

private:
    juce::TextEditor nameEditor_;
    juce::TextEditor bpmEditor_;
    juce::String endpointHint_;
    bool includeStems_ = false;
    juce::Rectangle<int> panel_, titleRect_, subtitleRect_, labelRect_, bpmLabelRect_;
    juce::Rectangle<int> titleEditorRect_, bpmEditorRect_, endpointRect_;
    juce::Rectangle<int> closeRect_, refreshRect_, stemsRect_, cancelRect_, uploadRect_;

    static juce::String makeCleanName(juce::String name)
    {
        name = name.trim();
        for (auto c : juce::String(R"(\/:*?"<>|)"))
            name = name.replaceCharacter(c, '-');
        return name.trim();
    }

    static juce::String makeBeatName()
    {
        static const char* moods[] = { "Midnight", "Dusty", "Golden", "Velvet", "Neon", "Lowkey", "Afterhours", "Soulful", "Raw", "Cinematic" };
        static const char* nouns[] = { "Pocket", "Bounce", "Loop", "Knock", "Groove", "Motion", "Tape", "Drift", "Chops", "Pulse" };
        static const char* tags[] = { "Beat", "Idea", "Session", "Flip", "Draft", "Sketch" };
        auto& rng = juce::Random::getSystemRandom();
        return juce::String(moods[rng.nextInt((int)(sizeof(moods) / sizeof(moods[0])))]) + " "
             + nouns[rng.nextInt((int)(sizeof(nouns) / sizeof(nouns[0])))] + " "
             + tags[rng.nextInt((int)(sizeof(tags) / sizeof(tags[0])))];
    }

    void updateLayout()
    {
        const int width = juce::jlimit(460, 700, getWidth() - 80);
        const int height = 340;
        panel_ = juce::Rectangle<int>((getWidth() - width) / 2, (getHeight() - height) / 2, width, height);

        auto content = panel_.reduced(28, 24);
        closeRect_ = juce::Rectangle<int>(panel_.getRight() - 46, panel_.getY() + 18, 28, 26);
        titleRect_ = content.removeFromTop(30);
        subtitleRect_ = content.removeFromTop(42);
        auto refreshRow = content.removeFromTop(26);
        refreshRect_ = refreshRow.removeFromRight(128);
        stemsRect_ = refreshRow.removeFromRight(110);
        content.removeFromTop(8);

        auto labelRow = content.removeFromTop(18);
        labelRect_ = labelRow.removeFromLeft((labelRow.getWidth() * 2) / 3);
        bpmLabelRect_ = labelRow;
        content.removeFromTop(6);
        auto editorRow = content.removeFromTop(40);
        titleEditorRect_ = editorRow.removeFromLeft((editorRow.getWidth() * 2) / 3);
        bpmEditorRect_ = editorRow.reduced(6, 0);
        content.removeFromTop(8);
        endpointRect_ = content.removeFromTop(24);
        uploadRect_ = panel_.reduced(28, 24).removeFromBottom(42).removeFromRight(210);
        cancelRect_ = uploadRect_.translated(-110, 0).withWidth(96);
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

    void upload()
    {
        UploadRequest req;
        req.name = makeCleanName(nameEditor_.getText());
        if (req.name.isEmpty())
            req.name = "Untitled";
        req.bpm = bpmEditor_.getText().getDoubleValue();
        if (req.bpm <= 0.0)
            req.bpm = 120.0;
        req.includeStems = includeStems_;
        if (onUpload)
            onUpload(req);
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
        if (bottomDock_)
            bottomDock_->setButtonActive(4, false);
        videoPanel_->saveWindowState();
        auto& anim = juce::Desktop::getInstance().getAnimator();
        anim.fadeOut (videoPanel_.get(), 130);
    };
    bottomDock_->onSessionVideoLayout = [this]() {
        if (videoPanel_ && videoPanel_->isEmbeddedInSession())
            videoPanel_->syncWebPlayerBounds();
    };
    videoPanel_->onOpenInSessionTab = [this]() { openVideoInSessionTab(); };
    bottomDock_->onOpenSessionVideo = [this]() { openVideoInSessionTab(); };
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

    auto getPatternContentSteps = [this]()
    {
        if (!channelRack_)
            return 16;

        int steps = juce::jmax(16, channelRack_->getTotalSteps());
        for (const auto& ch : channelRack_->getChannels())
        {
            steps = juce::jmax(steps, (int)ch.steps.size());
            for (const auto& note : ch.pianoRollNotes)
                steps = juce::jmax(steps, note.startStep + juce::jmax(1, note.lengthSteps));
        }

        return steps;
    };

    auto syncPlaylistPatternLength = [this, getPatternContentSteps]()
    {
        if (playlist_)
            playlist_->setPatternDefaultSteps(getPatternContentSteps());
    };

    auto syncMixerToChannelRack = [this, syncPlaylistPatternLength]()
    {
        if (!channelRack_ || !mixer_)
            return;

        std::vector<juce::String> names;
        juce::StringArray stripNumbers;
        auto& channels = channelRack_->getChannels();
        names.reserve(channels.size());
        stripNumbers.ensureStorageAllocated((int)channels.size());
        for (int i = 0; i < (int)channels.size(); ++i)
        {
            auto& ch = channels[(size_t)i];
            names.push_back(ch.name);
            stripNumbers.add(channelRack_->getChannelStripNumber(i));

            const int loopCount = channelRack_->getMusicLoopChannelCount();
            if (ch.isMusicLoop)
                ch.mixerTrack = ch.loopSlot;
            else
            {
                const int drumIndex = channelRack_->getDrumChannelIndexAmongDrums(i);
                ch.mixerTrack = loopCount + juce::jmax(0, drumIndex);
            }
        }

        mixer_->syncFromChannelRack(names, stripNumbers);

        for (int i = 0; i < (int)channels.size(); ++i)
        {
            auto& ch = channels[(size_t)i];
            if (ch.pluginSlotId >= 0 && ch.mixerTrack >= 0)
                pluginHost_.setSlotTrack(ch.pluginSlotId, ch.mixerTrack);
        }

        if (bottomDock_)
            bottomDock_->repaint();

        syncPlaylistPatternLength();
    };

    auto isKickChannelForPianoRoll = [](const ChannelRack::Channel& ch)
    {
        return ch.type == ChannelRack::InstrumentType::Kick
            || ch.name.containsIgnoreCase("kick");
    };

    auto is808ChannelForPianoRoll = [](const ChannelRack::Channel& ch)
    {
        return ch.type == ChannelRack::InstrumentType::Bass
            || ch.builtInInstrument == "bass"
            || ch.name.containsIgnoreCase("808")
            || ch.name.containsIgnoreCase("bass");
    };

    auto isAkaiMpcChannel = [](const ChannelRack::Channel& ch)
    {
        return ch.builtInInstrument == "akai_mpc"
            || ch.name.equalsIgnoreCase("Akai MPC Drums")
            || ch.name.containsIgnoreCase("mpc drums");
    };

    auto refreshPianoRollChannel = [this, isKickChannelForPianoRoll, is808ChannelForPianoRoll, isAkaiMpcChannel](int channelIndex)
    {
        auto& channels = channelRack_->getChannels();
        if (channelIndex < 0 || channelIndex >= (int)channels.size())
            return;

        const auto& ch = channels[(size_t)channelIndex];
        pianoRoll_->setChannelName(ch.name);
        pianoRoll_->setChannelContext(isKickChannelForPianoRoll(ch), is808ChannelForPianoRoll(ch));

        std::vector<PianoRollNote> pianoNotes;
        if (isAkaiMpcChannel(ch))
        {
            juce::StringArray laneNames;
            constexpr int topPitch = 84;
            int lane = 0;
            for (int i = 0; i < (int)channels.size(); ++i)
            {
                if (i == channelIndex || isAkaiMpcChannel(channels[(size_t)i]))
                    continue;

                const auto& src = channels[(size_t)i];
                laneNames.add(src.name);
                const int lanePitch = topPitch - lane;
                if (!src.pianoRollNotes.empty())
                {
                    for (const auto& n : src.pianoRollNotes)
                        pianoNotes.push_back({ lanePitch, n.startStep, n.lengthSteps, n.velocity });
                }
                else
                {
                    for (int step = 0; step < (int)src.steps.size(); ++step)
                        if (src.steps[(size_t)step])
                            pianoNotes.push_back({ lanePitch, step, 1, 100 });
                }
                ++lane;
            }
            pianoRoll_->setNotes(pianoNotes);
            pianoRoll_->setDrumLaneNames(laneNames, topPitch);
            return;
        }

        for (const auto& n : ch.pianoRollNotes)
            pianoNotes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
        pianoRoll_->setNotes(pianoNotes);
    };

    channelRack_->onChannelsChanged = syncMixerToChannelRack;
    syncMixerToChannelRack();

    playlist_->onClipsChanged = [this]() { syncPlaylistLoopsToChannelRack(); };
    
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
    channelRack_->onChannelClicked = [this, refreshPianoRollChannel](int channelIndex) {
        setCenterView(CenterView::PianoRoll);
        refreshPianoRollChannel(channelIndex);
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

        const int channelIndex = channelRack_->applyExtractedChordifyMidi(sourceName, notes, -1);
        if (channelIndex >= 0 && channelRack_->onChannelClicked)
            channelRack_->onChannelClicked(channelIndex);
    };

    playlist_->onRequestBassExtraction = [this](Playlist::BassExtractionRequest request,
                                               std::function<void(std::vector<Playlist::ExtractedBassNote>)> deliverNotes)
    {
        const bool autoApply = request.autoApply;
        handleBassExtractionRequest(std::move(request), std::move(deliverNotes), autoApply);
    };

    playlist_->onImportChordifyMidiForClip = [this](Playlist::BassExtractionRequest request)
    {
        handleChordifyMidiImport(std::move(request));
    };

    playlist_->onChordifyMidiDroppedFor808 = [this, syncPlaylistPatternLength](const juce::File& midiFile)
    {
        if (!channelRack_ || !midiFile.existsAsFile())
            return;

        const double bpmHint = transportBar_ ? transportBar_->getBPM() : 0.0;
        const auto imported = ChordifyMidiImporter::import(midiFile, bpmHint, 0);
        if (imported.empty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Chordify MIDI",
                "Could not read 808 bass roots from this MIDI file.");
            return;
        }

        std::vector<ChannelRack::Channel::Note> notes;
        notes.reserve(imported.size());
        for (const auto& n : imported)
            notes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });

        const int channelIndex = channelRack_->applyPlaylist808Midi(midiFile.getFileNameWithoutExtension(), notes);
        if (channelIndex >= 0)
        {
            syncPlaylistPatternLength();
            if (bottomDock_)
                bottomDock_->setSessionStatus("808 MIDI loaded from " + midiFile.getFileNameWithoutExtension());
        }
    };
    
    // Wire up Channel Rack header buttons
    channelRack_->onSplitPatternToPlaylist = [this]()
    {
        if (!playlist_ || !channelRack_)
            return;

        juce::String patternName = "Pattern 1";
        if (transportBar_)
        {
            const auto names = transportBar_->getPatterns();
            const int idx = transportBar_->getCurrentPattern();
            if (idx >= 0 && idx < names.size())
                patternName = names[idx];
        }

        juce::StringArray channelNames;
        for (const auto& ch : channelRack_->getChannels())
            channelNames.add(ch.name);

        playlist_->splitPatternChannelsToTracks(patternName, channelNames, channelRack_->getTotalSteps());
        setCenterView(CenterView::Playlist);
        if (bottomDock_)
            bottomDock_->setSessionStatus("Split " + patternName + " to playlist");
    };

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
        m.addSeparator();
        m.addItem (8, "Akai MPC Drums");
        m.showMenuAsync (juce::PopupMenu::Options{}.withTargetComponent (channelRack_.get()),
            [this](int chosen){
                if (chosen <= 0) return;
                using IT = ChannelRack::InstrumentType;
                if (chosen == 8)
                {
                    auto& chs = channelRack_->getChannels();
                    ChannelRack::Channel c;
                    c.name = "Akai MPC Drums";
                    c.type = IT::Pad;
                    c.steps = std::vector<bool>((size_t)channelRack_->getTotalSteps(), false);
                    c.builtInInstrument = "akai_mpc";
                    c.volume = 0.0f;
                    chs.push_back(std::move(c));
                    channelRack_->setSelectedChannel((int)chs.size() - 1);
                    channelRack_->repaint();
                    if (channelRack_->onChannelsChanged) channelRack_->onChannelsChanged();
                    if (channelRack_->onChannelClicked) channelRack_->onChannelClicked((int)chs.size() - 1);
                    return;
                }
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
    channelRack_->onToggle16_32 = [this, syncPlaylistPatternLength](){
        channelRack_->toggleStepCount();
        syncPlaylistPatternLength();
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
        menu.addItem(8004, "Akai MPC Drums  [Drum MIDI Hub]");
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

                if (chosen == 8004) {
                    auto& chs = channelRack_->getChannels();
                    ChannelRack::Channel c;
                    c.name = "Akai MPC Drums";
                    c.type = ChannelRack::InstrumentType::Pad;
                    c.steps = std::vector<bool>((size_t)channelRack_->getTotalSteps(), false);
                    c.builtInInstrument = "akai_mpc";
                    c.volume = 0.0f;
                    const int newIdx = (int)chs.size();
                    c.mixerTrack = -1;
                    chs.push_back(std::move(c));
                    channelRack_->setSelectedChannel(newIdx);
                    channelRack_->repaint();
                    if (channelRack_->onChannelsChanged) channelRack_->onChannelsChanged();
                    if (channelRack_->onChannelClicked) channelRack_->onChannelClicked(newIdx);
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
    pianoRoll_->onNotesChanged = [this, syncPlaylistPatternLength, isAkaiMpcChannel]() {
        int selectedChannel = channelRack_->getSelectedChannel();
        auto& channels = channelRack_->getChannels();
        if (selectedChannel >= 0 && selectedChannel < (int)channels.size()) {
            auto& ch = channels[selectedChannel];
            
            // Save piano-roll notes back to the channel
            auto pianoNotes = pianoRoll_->getNotes();
            if (isAkaiMpcChannel(ch))
            {
                constexpr int topPitch = 84;
                std::vector<int> laneToChannel;
                for (int i = 0; i < (int)channels.size(); ++i)
                    if (i != selectedChannel && !isAkaiMpcChannel(channels[(size_t)i]))
                        laneToChannel.push_back(i);

                for (int target : laneToChannel)
                {
                    auto& laneCh = channels[(size_t)target];
                    laneCh.pianoRollNotes.clear();
                    laneCh.steps.assign((size_t)juce::jmax(channelRack_->getTotalSteps(), (int)laneCh.steps.size()), false);
                }

                for (const auto& n : pianoNotes)
                {
                    const int lane = topPitch - n.pitch;
                    if (lane < 0 || lane >= (int)laneToChannel.size())
                        continue;

                    auto& laneCh = channels[(size_t)laneToChannel[(size_t)lane]];
                    const int start = juce::jmax(0, n.startStep);
                    const int len = juce::jmax(1, n.lengthSteps);
                    if (start + len > (int)laneCh.steps.size())
                        laneCh.steps.resize((size_t)(start + len), false);
                    laneCh.pianoRollNotes.push_back({ ChannelRack::DEFAULT_DRUM_PITCH, start, len, juce::jlimit(1, 127, n.velocity) });
                    laneCh.steps[(size_t)start] = true;
                }

                channelRack_->repaint();
                syncPlaylistPatternLength();
                return;
            }

            ch.pianoRollNotes.clear();
            for (const auto& n : pianoNotes)
                ch.pianoRollNotes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
            
            int contentSteps = channelRack_->getTotalSteps();
            for (const auto& n : ch.pianoRollNotes)
                contentSteps = juce::jmax(contentSteps, n.startStep + juce::jmax(1, n.lengthSteps));
            if ((int)ch.steps.size() < contentSteps)
                ch.steps.resize((size_t)contentSteps, false);

            // Re-derive step grid from piano-roll notes (a step is active if any note starts there)
            std::fill(ch.steps.begin(), ch.steps.end(), false);
            for (const auto& n : ch.pianoRollNotes) {
                if (n.startStep >= 0 && n.startStep < (int)ch.steps.size())
                    ch.steps[n.startStep] = true;
            }
            channelRack_->repaint();
            syncPlaylistPatternLength();
        }
    };

    pianoRoll_->onPasteFrom808Requested = [this, syncPlaylistPatternLength, isKickChannelForPianoRoll, is808ChannelForPianoRoll]()
    {
        if (!channelRack_ || !pianoRoll_)
            return;

        auto& channels = channelRack_->getChannels();
        const int selectedChannel = channelRack_->getSelectedChannel();
        if (selectedChannel < 0 || selectedChannel >= (int)channels.size())
            return;
        if (!isKickChannelForPianoRoll(channels[(size_t)selectedChannel]))
            return;

        const ChannelRack::Channel* source808 = nullptr;
        for (int i = 0; i < (int)channels.size(); ++i)
        {
            if (i == selectedChannel)
                continue;
            const auto& candidate = channels[(size_t)i];
            if (is808ChannelForPianoRoll(candidate) && !candidate.pianoRollNotes.empty())
            {
                source808 = &candidate;
                break;
            }
        }

        if (source808 == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                "Paste from 808 MIDI",
                "No 808 or bass channel with MIDI notes was found.");
            return;
        }

        auto& kick = channels[(size_t)selectedChannel];
        kick.pianoRollNotes.clear();
        kick.steps.assign((size_t)juce::jmax(channelRack_->getTotalSteps(), (int)kick.steps.size()), false);

        std::vector<PianoRollNote> pasted;
        int maxEnd = channelRack_->getTotalSteps();
        for (const auto& n : source808->pianoRollNotes)
        {
            ChannelRack::Channel::Note k { 72, // C5
                                           juce::jmax(0, n.startStep),
                                           juce::jmax(1, n.lengthSteps),
                                           juce::jlimit(1, 127, n.velocity) };
            kick.pianoRollNotes.push_back(k);
            pasted.push_back({ k.pitch, k.startStep, k.lengthSteps, k.velocity });
            maxEnd = juce::jmax(maxEnd, k.startStep + k.lengthSteps);
        }

        if (maxEnd > (int)kick.steps.size())
            kick.steps.resize((size_t)maxEnd, false);
        std::fill(kick.steps.begin(), kick.steps.end(), false);
        for (const auto& n : kick.pianoRollNotes)
            if (n.startStep >= 0 && n.startStep < (int)kick.steps.size())
                kick.steps[(size_t)n.startStep] = true;

        pianoRoll_->setNotes(pasted);
        channelRack_->repaint();
        syncPlaylistPatternLength();
    };

    pianoRoll_->onAuditionNote = [this, isAkaiMpcChannel](int pitch, int lengthSteps, int velocity) {
        const int selectedChannel = channelRack_->getSelectedChannel();
        auto& channels = channelRack_->getChannels();

        if (selectedChannel >= 0 && selectedChannel < (int)channels.size())
        {
            auto& ch = channels[(size_t)selectedChannel];

            // Akai MPC pads: each visual lane maps to a different drum channel.
            if (isAkaiMpcChannel(ch))
            {
                constexpr int topPitch = 84;
                int lane = topPitch - pitch;
                for (int i = 0; i < (int)channels.size(); ++i)
                {
                    if (i == selectedChannel || isAkaiMpcChannel(channels[(size_t)i]))
                        continue;
                    if (lane-- == 0)
                    {
                        channelRack_->auditionChannel(i);
                        return;
                    }
                }
                return;
            }

            // Lazy-load the bundled Stratum Piano plugin on first audition.
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

            // Delegate to the shared trigger pathway used by the step sequencer.
            // This guarantees the audition sounds identical to playback.
            channelRack_->auditionPianoRollNote(selectedChannel, pitch, lengthSteps, velocity);
            return;
        }

        const float normalizedVelocity = juce::jlimit(0.0f, 1.0f, (float)velocity / 127.0f);
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
    syncPlaylistPatternLength();
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
    playlist_->onHeadphoneFlatToggled = [this](bool enabled) {
        pluginHost_.setHeadphoneFlatEnabled(enabled);
    };
    playlist_->isHeadphoneFlatEnabled = [this]() {
        return pluginHost_.isHeadphoneFlatEnabled();
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
        if (!playlist_ || !channelRack_)
            return -1;
        const auto& rackChannels = channelRack_->getChannels();
        if (channelIndex < 0 || channelIndex >= (int)rackChannels.size())
            return -1;
        if (rackChannels[(size_t)channelIndex].isMusicLoop)
            return -1;
        const int patternCh = channelRack_->getDrumChannelIndexAmongDrums(channelIndex);
        return playlist_->patternLocalStepForChannelAt(absoluteStep, patternSteps,
                                                       patternCh >= 0 ? patternCh : channelIndex);
    };
    channelRack_->shouldPlayChannelAtStep = [this](int absoluteStep, int channelIndex) {
        if (playbackMode_ == TransportBar::PlaybackMode::Rack || centerView_ == CenterView::PianoRoll)
            return true;
        if (!playlist_ || !channelRack_)
            return true;
        const auto& rackChannels = channelRack_->getChannels();
        if (channelIndex < 0 || channelIndex >= (int)rackChannels.size())
            return false;
        if (rackChannels[(size_t)channelIndex].isMusicLoop)
            return false;
        const int patternCh = channelRack_->getDrumChannelIndexAmongDrums(channelIndex);
        return playlist_->patternAllowsChannelAtStep(absoluteStep,
                                                     patternCh >= 0 ? patternCh : channelIndex);
    };
    channelRack_->isPlaylistPlaybackActive = [this]() {
        return playbackMode_ == TransportBar::PlaybackMode::Playlist
            && centerView_ != CenterView::PianoRoll;
    };

    // When the channel rack toggles a step, push the change to piano roll if shown
    channelRack_->onChannelDataChanged = [this, syncPlaylistPatternLength, refreshPianoRollChannel, isAkaiMpcChannel](int channelIdx) {
        syncPlaylistPatternLength();

        auto& channels = channelRack_->getChannels();
        if (channelRack_->getSelectedChannel() == channelIdx
            && channelIdx >= 0 && channelIdx < (int)channels.size())
        {
            refreshPianoRollChannel(channelIdx);
        }
        else
        {
            const int selected = channelRack_->getSelectedChannel();
            if (selected >= 0 && selected < (int)channels.size() && isAkaiMpcChannel(channels[(size_t)selected]))
                refreshPianoRollChannel(selected);
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
    syncPatternName();
    
    // Wire up SAVE, OPEN, EXPORT, NEW PROJECT buttons
    transportBar_->onSave = [this](){
        showExportAudioModal(false);
    };
    transportBar_->onOpen = [this](){ openProjectFile(); };
    transportBar_->onUploadToCloud = [this](){
        showCloudUploadModal();
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
            bottomDock_->setButtonActive(2, false);
            anim.fadeOut (channelRack_.get(), 130);
        }
        else
        {
            bottomDock_->setButtonActive(2, true);
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
            bottomDock_->setButtonActive(3, false);
            anim.animateComponent (patternsPanel_.get(),
                patternsPanel_->getBounds().translated (0, 30), 0.0f, 160, false, 0.0, 1.0);
            juce::Timer::callAfterDelay (170, [this]{ patternsPanel_->setVisible (false); });
        }
        else
        {
            bottomDock_->setButtonActive(3, true);
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
            bottomDock_->setButtonActive(4, false);
            videoPanel_->saveWindowState();
            anim.fadeOut (videoPanel_.get(), 130);
        }
        else
        {
            bottomDock_->setButtonActive(4, true);
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
        if (presetId.isEmpty() || presetId.equalsIgnoreCase("none"))
            return;

        const juce::String label = presetLabel.isNotEmpty() ? presetLabel : presetId;
        if (aiPanel_)
            aiPanel_->addUserMessage(presetId.equalsIgnoreCase("empty") ? "Clear drum pattern" : "Make a " + label + " pattern");

        if (aiPanel_ && aiPanel_->onPreset)
            aiPanel_->onPreset(presetId, label);

        if (aiPanel_)
            aiPanel_->addAssistantMessage(presetId.equalsIgnoreCase("empty")
                ? "Cleared the drum pattern."
                : "Done! Loaded a " + label + " drum pattern.");
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
    channelRack_->onDrumPatternVariantClicked = [this, applyPatternDefinition](const PatternsPanel::PatternDefinition& pattern) {
        applyPatternDefinition(pattern);
        if (aiPanel_)
            aiPanel_->addAssistantMessage("Done! Changed to " + pattern.title + ".");
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
    patternsPanel_->onApplyChannelPattern = [this](const PatternsPanel::PatternDefinition& pattern, int rowIndex) {
        if (!channelRack_)
            return;

        channelRack_->applyPatternLaneToExistingRows(pattern.title, rowIndex, pattern.rows[(size_t)juce::jlimit(0, 3, rowIndex)]);

        if (aiPanel_)
            aiPanel_->addAssistantMessage("MIDI lane loaded: " + pattern.title + ".");

        if (!channelRack_->isVisible())
            channelRack_->setVisible(true);
        channelRack_->toFront(false);
    };
    patternsPanel_->onClose = [this]() {
        auto& anim = juce::Desktop::getInstance().getAnimator();
        if (bottomDock_)
            bottomDock_->setButtonActive(3, false);
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
    mixer_->onCreateFxAutomation = [this](int trackIdx, int pluginSlotId, const juce::String& name)
    {
        if (!playlist_)
            return;
        playlist_->addEffectAutomationClip(pluginSlotId, name, trackIdx);
        setCenterView(CenterView::Playlist);
        if (bottomDock_)
            bottomDock_->setSessionStatus("Created automation for " + name);
    };
    playlist_->onEffectAutomationValue = [this](int pluginSlotId, float value)
    {
        if (mixer_)
            mixer_->setFxSlotEnabledById(pluginSlotId, value >= 0.5f);
    };

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
    transportBar_->onFindLoopsInBpmRange = [this](double bpm, juce::Rectangle<int> anchor) {
        showLoopsInBpmRangePicker(bpm, anchor);
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

    themeBtn_.setLookAndFeel(&titleBarBadgeLaf);
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
    if (key == juce::KeyPress::tabKey)
    {
        if (bottomDock_ && bottomDock_->onChannelRack)
            bottomDock_->onChannelRack();
        return true;
    }

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
        exportAudioAs();
        return true;
    }
    if (key == KP('o', MK::ctrlModifier, 0))
    {
        openProjectFile();
        return true;
    }
    return false;
}

MainComponent::~MainComponent()
{
    themeBtn_.setLookAndFeel(nullptr);
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    if (videoPanel_ && videoPanel_->isVisible() && VideoPanel::canAcceptVideoFiles(files))
        return true;

    for (const auto& path : files)
    {
        const auto ext = juce::File(path).getFileExtension().toLowerCase();
        if (ext == ".mid" || ext == ".midi")
            return true;
    }

    return false;
}

void MainComponent::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    const auto pos = juce::Point<int>(x, y);

    if (videoPanel_ && videoPanel_->isVisible()
        && VideoPanel::canAcceptVideoFiles(files)
        && videoPanel_->getBounds().expanded(12).contains(pos))
    {
        const auto local = videoPanel_->getLocalPoint(this, pos);
        videoPanel_->fileDragEnter(files, local.x, local.y);
        return;
    }

    if (playlist_)
    {
        for (const auto& path : files)
        {
            const auto ext = juce::File(path).getFileExtension().toLowerCase();
            if (ext == ".mid" || ext == ".midi")
            {
                playlist_->fileDragEnter(files, 0, 0);
                return;
            }
        }
    }
}

void MainComponent::fileDragExit(const juce::StringArray&)
{
    notifyVideoFileDragExit();
    if (playlist_)
        playlist_->fileDragExit({});
}

void MainComponent::filesDropped(const juce::StringArray& files, int x, int y)
{
    for (const auto& path : files)
    {
        const auto ext = juce::File(path).getFileExtension().toLowerCase();
        if (ext == ".mid" || ext == ".midi")
        {
            if (playlist_)
                playlist_->filesDropped(files, x, y);
            return;
        }
    }

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

void MainComponent::showLoopsInBpmRangePicker(double bpm, juce::Rectangle<int> anchorScreenArea)
{
    if (!Browser::resolveLoopsRootFolder().isDirectory())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Loops folder not found",
            "Could not find the Loops library folder.\nCheck Browser.cpp paths (F:\\1500 LOOPS FOLDER).");
        return;
    }

    if (browser_)
        browser_->focusLoopsLibrary();

    setCenterView(CenterView::Playlist);
    if (transportBar_)
        transportBar_->setSelectedView(2);

    const double targetBpm = bpm;
    const auto anchor = anchorScreenArea;
    juce::Component::SafePointer<MainComponent> safeThis(this);

    std::thread([safeThis, targetBpm, anchor]()
    {
        auto matches = Browser::findLoopsMatchingBpm(targetBpm, 5.0);

        juce::MessageManager::callAsync([safeThis, matches = std::move(matches), targetBpm, anchor]() mutable
        {
            if (safeThis == nullptr)
                return;
            safeThis->openLoopPickerCallout(std::move(matches), targetBpm, anchor);
        });
    }).detach();
}

void MainComponent::openLoopPickerCallout(std::vector<juce::File> loops,
                                          double bpm,
                                          juce::Rectangle<int> anchorScreenArea)
{
    juce::Component::SafePointer<MainComponent> safeThis(this);

    auto panel = std::make_unique<LoopPickerCallout>(
        std::move(loops),
        bpm,
        pluginHost_,
        [safeThis](const juce::File& file)
        {
            if (safeThis == nullptr || !safeThis->playlist_)
                return;

            safeThis->setCenterView(CenterView::Playlist);
            safeThis->playlist_->addAudioFileFromExternalBrowserDrag(file, true);
        });

    juce::CallOutBox::launchAsynchronously(std::move(panel), anchorScreenArea, nullptr);
}

void MainComponent::syncPlaylistLoopsToChannelRack()
{
    if (!playlist_ || !channelRack_)
        return;

    channelRack_->syncMusicLoopChannels(playlist_->getUniqueSampleLoopsInOrder());
    playlist_->assignLoopMixerTracks([this](const juce::File& file)
    {
        return channelRack_->getMixerTrackForLoopFile(file);
    });

    if (channelRack_->onChannelsChanged)
        channelRack_->onChannelsChanged();
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
    const int badgeW = titleBarBadgeWidthForText(label);
    themeBtn_.setBounds(120, 7, badgeW, 14);
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
    drawTitleBarBadgeBackground(g, sysBadge);
    drawTitleBarBadgeText(g, sysBadge, "SYS.01");
    
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
    themeBtn_.setBounds(120, 7, titleBarBadgeWidthForText(themeBtn_.getButtonText()), 14);
    
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
    if (cloudUploadOverlay_)
        cloudUploadOverlay_->setBounds(getLocalBounds());
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
    {
        bottomDock_->setSelectedButton(5);
        bottomDock_->setButtonActive(5, true);
    }
    if (playlist_)
        playlist_->repaint();
    anim.animateComponent(aiPanel_.get(), target, 1.0f, 210, false, 1.0, 0.0);
}

void MainComponent::closeAiPanel()
{
    if (!aiPanel_ || !aiPanel_->isVisible())
        return;

    if (bottomDock_)
    {
        bottomDock_->setSelectedButton(-1);
        bottomDock_->setButtonActive(5, false);
    }
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

    if (bottomDock_)
    {
        bottomDock_->setButtonActive(0, v == CenterView::Mixer);
        bottomDock_->setButtonActive(1, v == CenterView::PianoRoll);
    }

    // Channel rack rides with the Playlist view.
    bool wantRack = (v == CenterView::Playlist);
    if (wantRack)
    {
        if (bottomDock_)
            bottomDock_->setButtonActive(2, true);
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
        if (bottomDock_)
            bottomDock_->setButtonActive(2, false);
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

void MainComponent::openVideoInSessionTab()
{
    if (!bottomDock_ || !videoPanel_)
        return;

    if (!videoPanel_->hasVideoLoaded())
    {
        auto& anim = juce::Desktop::getInstance().getAnimator();
        if (videoPanel_->isVisible())
            return;

        int pw = juce::jmin(900, getWidth() - 80);
        int ph = juce::jmin(620, getHeight() - 100);
        juce::Rectangle<int> defaultTarget((getWidth() - pw) / 2, (getHeight() - ph) / 2, pw, ph);
        auto target = videoPanel_->getSavedOrDefaultBounds(getLocalBounds(), defaultTarget);
        videoPanel_->setBounds(target);
        videoPanel_->setAlpha(0.0f);
        videoPanel_->setVisible(true);
        videoPanel_->toFront(true);
        anim.fadeIn(videoPanel_.get(), 180);
        return;
    }

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
}

void MainComponent::handleBassExtractionRequest(Playlist::BassExtractionRequest request,
                                                std::function<void(std::vector<Playlist::ExtractedBassNote>)> deliverNotes,
                                                bool autoApply)
{
    if (bassAnalysisBusy_)
    {
        if (! autoApply)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                "Extract Bass",
                "Another loop analysis is already running. Try again in a moment.");
        }
        return;
    }

    using DeliverFn = std::function<void(std::vector<Playlist::ExtractedBassNote>)>;
    auto deliverNotesPtr = std::make_shared<DeliverFn>(std::move(deliverNotes));

    auto notifyFailure = [autoApply](const juce::String& detail = {})
    {
        if (! autoApply)
        {
            juce::String msg = "Bass extraction failed.";
            if (detail.isNotEmpty())
                msg << "\n\n" << detail;
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                "Extract Bass", msg);
        }
    };

    auto deliverFromNotes = [deliverNotesPtr, autoApply, notifyFailure](
                                std::vector<Playlist::ExtractedBassNote> notes)
    {
        if (notes.empty() || deliverNotesPtr == nullptr || ! *deliverNotesPtr)
        {
            notifyFailure();
            return false;
        }

        (*deliverNotesPtr)(notes);
        return true;
    };

    if (ChordifyAutomationEngine::isReady())
    {
        bassAnalysisBusy_ = true;
        if (bottomDock_)
            bottomDock_->setSessionStatus("Chordify: download MIDI...");

        juce::Component::SafePointer<MainComponent> safe(this);
        chordifyAutomationEngine_.fetchBassAsync(request.audioFile, request.bpmHint, request.maxSteps,
            [safe, deliverNotesPtr, autoApply, request, notifyFailure = std::move(notifyFailure),
             deliverFromNotes = std::move(deliverFromNotes)]
            (ChordifyAutomationEngine::Result result) mutable
            {
                if (safe == nullptr)
                    return;

                safe->bassAnalysisBusy_ = false;

                std::vector<Playlist::ExtractedBassNote> notes;
                if (result.ok)
                {
                    if (! result.notes.empty())
                    {
                        notes.reserve(result.notes.size());
                        for (const auto& n : result.notes)
                            notes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
                    }
                    else if (result.midiFile.existsAsFile())
                    {
                        const auto imported = ChordifyMidiImporter::importAllTracks(
                            result.midiFile,
                            request.bpmHint > 0.0 ? request.bpmHint : result.bpm,
                            request.maxSteps);
                        notes.reserve(imported.size());
                        for (const auto& n : imported)
                            notes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
                    }
                }

                if (! deliverFromNotes(notes))
                {
                    if (safe->bottomDock_)
                        safe->bottomDock_->setSessionStatus("Chordify failed");
                    notifyFailure(result.error.isNotEmpty()
                                      ? result.error
                                      : "Chordify MIDI empty. Try menu > MIDI Time aligned manually.");
                    return;
                }

                if (safe->bottomDock_)
                    safe->bottomDock_->setSessionStatus("Chordify chords ready");

                if (! autoApply)
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                        "Chord MIDI imported",
                        "Imported Chordify chord MIDI into Extracted Chords channel.");
                }
            });
        return;
    }

    if (! ChordAnalysisEngine::isReady())
    {
        if (! playlist_ || ! deliverNotesPtr || ! *deliverNotesPtr)
            return;

        auto notes = playlist_->extractBassMidiFallback(request.audioFile,
                                                        request.sourceName,
                                                        request.bpmHint,
                                                        request.maxSteps);
        if (notes.empty())
        {
            if (! autoApply)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                    "Extract Bass",
                    "Install ML analysis: powershell -File vst-host\\analysis\\setup-analysis.ps1");
            }
            return;
        }

        (*deliverNotesPtr)(notes);
        if (! autoApply)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                "Bass MIDI extracted",
                "Used legacy extractor (ML not installed). Run setup-analysis.ps1 for Chordify-quality BTC.");
        }
        return;
    }

    bassAnalysisBusy_ = true;
    if (bottomDock_)
        bottomDock_->setSessionStatus("Analyzing chords (BTC)...");

    juce::Component::SafePointer<MainComponent> safe(this);
    chordAnalysisEngine_.analyzeAsync(request.audioFile, request.bpmHint, request.maxSteps,
        [safe, deliverNotesPtr, autoApply, notifyFailure = std::move(notifyFailure)]
        (ChordAnalysisEngine::Result result) mutable
        {
            if (safe == nullptr)
                return;

            safe->bassAnalysisBusy_ = false;
            if (safe->bottomDock_)
                safe->bottomDock_->setSessionStatus("Stopped");

            std::vector<Playlist::ExtractedBassNote> notes;
            if (result.ok)
            {
                notes.reserve(result.notes.size());
                for (const auto& n : result.notes)
                    notes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });
            }

            if (notes.empty() || deliverNotesPtr == nullptr || ! *deliverNotesPtr)
            {
                notifyFailure("BTC analysis failed. For Chordify-quality bass run:\n"
                              "powershell -File vst-host\\analysis\\setup-chordify.ps1\n"
                              "python vst-host\\analysis\\chordify_automation.py --login");
                return;
            }

            (*deliverNotesPtr)(notes);

            if (safe->bottomDock_)
                safe->bottomDock_->setSessionStatus("BTC bass ready");

            if (! autoApply)
            {
                juce::String msg = "Created an Extracted Bass slot using BTC chord analysis.";
                if (result.key.isNotEmpty())
                    msg << "\nKey: " << result.key;
                if (result.bpm > 0.0)
                    msg << "  BPM: " << juce::String(result.bpm, 1);
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                    "Bass MIDI extracted", msg);
            }
        });
}

void MainComponent::handleChordifyMidiImport(Playlist::BassExtractionRequest request)
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Import Chordify MIDI for \"" + request.sourceName + "\"",
        request.audioFile.getParentDirectory(),
        "*.mid;*.midi");

    juce::Component::SafePointer<MainComponent> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe, request = std::move(request), chooser](const juce::FileChooser& fc)
        {
            const auto midiFile = fc.getResult();
            if (safe == nullptr || ! midiFile.existsAsFile())
                return;

            const auto imported = ChordifyMidiImporter::importAllTracks(midiFile,
                                                               request.bpmHint,
                                                               request.maxSteps);
            if (imported.empty())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Chordify MIDI",
                    "Could not read bass roots from this MIDI file.");
                return;
            }

            std::vector<Playlist::ExtractedBassNote> notes;
            notes.reserve(imported.size());
            for (const auto& n : imported)
                notes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });

            if (safe->playlist_ && safe->playlist_->onExtractBassMidi)
                safe->playlist_->onExtractBassMidi(request.sourceName, notes);
            else if (safe->channelRack_)
            {
                std::vector<ChannelRack::Channel::Note> rackNotes;
                rackNotes.reserve(notes.size());
                for (const auto& n : notes)
                    rackNotes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });

                const int channelIndex = safe->channelRack_->applyExtractedChordifyMidi(request.sourceName, rackNotes, -1);
                if (channelIndex >= 0 && safe->channelRack_->onChannelClicked)
                    safe->channelRack_->onChannelClicked(channelIndex);
            }

            if (safe->bottomDock_)
                safe->bottomDock_->setSessionStatus("Chordify chords ready");
        });
}

void MainComponent::showExportAudioModal(bool defaultStems)
{
    auto root = juce::File("D:\\stratumdaw");
    root.createDirectory();

    auto baseName = currentProjectFile_.existsAsFile()
        ? currentProjectFile_.getFileNameWithoutExtension()
        : juce::String("Untitled");

    auto overlay = std::make_unique<ProjectSaveOverlay>(root, baseName, defaultStems);
    overlay->setBounds(getLocalBounds());
    overlay->onClose = [this]()
    {
        projectSaveOverlay_.reset();
        repaint();
    };
    overlay->onSave = [this, root](const juce::String& cleanName, bool exportStems)
    {
        const auto projectFile = root.getChildFile(cleanName).withFileExtension(kProjectExt);
        const bool projectSaved = saveProject(projectFile);
        if (!projectSaved)
            return;

        if (exportStems)
        {
            const auto stemsFolder = root.getChildFile(cleanName + " stems");
            if (exportStemsToFolder(stemsFolder, cleanName))
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Stems export complete",
                    "Project saved:\n" + projectFile.getFullPathName()
                    + "\n\nStems exported:\n" + stemsFolder.getFullPathName());
            }
        }
        else
        {
            const auto wavFile = root.getChildFile(cleanName).withFileExtension(".wav");
            if (exportAudioToFile(wavFile))
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Export complete",
                    "Project saved:\n" + projectFile.getFullPathName()
                    + "\n\nWAV exported:\n" + wavFile.getFullPathName());
            }
        }

        projectSaveOverlay_.reset();
    };

    addAndMakeVisible(overlay.get());
    overlay->toFront(true);
    overlay->grabKeyboardFocus();
    projectSaveOverlay_ = std::move(overlay);
}

void MainComponent::showCloudUploadModal()
{
    auto baseName = currentProjectFile_.existsAsFile()
        ? currentProjectFile_.getFileNameWithoutExtension()
        : juce::String("Untitled");

    const double defaultBpm = transportBar_ ? transportBar_->getBPM() : 120.0;
    const auto endpoint = getCloudUploadEndpoint();
    auto overlay = std::make_unique<CloudUploadOverlay>(baseName, defaultBpm, endpoint);
    overlay->setBounds(getLocalBounds());
    overlay->onClose = [this]()
    {
        cloudUploadOverlay_.reset();
        repaint();
    };
    overlay->onUpload = [this, endpoint](const CloudUploadOverlay::UploadRequest& req)
    {
        // Defer — never destroy overlay synchronously from its own mouse handler.
        juce::MessageManager::callAsync([this, endpoint, req]()
        {
            cloudUploadOverlay_.reset();
            repaint();

            auto root = juce::File("D:\\stratumdaw");
            root.createDirectory();

            const auto cleanName = req.name;
            const auto projectFile = root.getChildFile(cleanName).withFileExtension(kProjectExt);
            const auto wavFile = root.getChildFile(cleanName).withFileExtension(".wav");

            if (!saveProject(projectFile))
                return;

            if (!exportAudioToFile(wavFile))
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Export failed",
                    "Could not export WAV before BeatStars upload.");
                return;
            }

            juce::StringArray stemPaths;
            if (req.includeStems)
            {
                const auto stemsFolder = root.getChildFile(cleanName + " stems");
                if (exportStemsToFolder(stemsFolder, cleanName))
                {
                    juce::Array<juce::File> stemFiles;
                    stemsFolder.findChildFiles(stemFiles, juce::File::findFiles, false, "*.wav");
                    for (const auto& f : stemFiles)
                        stemPaths.add(f.getFullPathName());
                }
            }

            if (bottomDock_)
                bottomDock_->setSessionStatus("Publishing to BeatStars...");

            const juce::Component::SafePointer<MainComponent> safe(this);

            std::thread([safe, req, wavFile, endpoint, stemPaths]()
            {
                const auto uploadResult = uploadBeatViaBrowserControl(
                    endpoint,
                    wavFile,
                    req.name,
                    req.bpm,
                    stemPaths,
                    [safe](const juce::String& status)
                    {
                        juce::MessageManager::callAsync([safe, status]()
                        {
                            if (safe != nullptr && safe->bottomDock_)
                                safe->bottomDock_->setSessionStatus(status);
                        });
                    });

                juce::MessageManager::callAsync([safe, uploadResult, req, endpoint]()
                {
                    if (safe == nullptr)
                        return;

                    if (safe->bottomDock_)
                        safe->bottomDock_->setSessionStatus(uploadResult.ok ? "Published on BeatStars"
                                                                            : "BeatStars publish failed");

                    juce::AlertWindow::showMessageBoxAsync(
                        uploadResult.ok ? juce::AlertWindow::InfoIcon : juce::AlertWindow::WarningIcon,
                        uploadResult.ok ? "BeatStars publish complete" : "BeatStars publish failed",
                        uploadResult.ok ? ("Track: " + req.name + "\nBPM: " + juce::String(req.bpm, 1)
                                         + "\n\nBrowserControl handled upload + listing.")
                                        : (uploadResult.error.isNotEmpty()
                                               ? uploadResult.error
                                               : ("Could not publish via BrowserControl:\n" + endpoint
                                                  + "\n\n1. Ensure BrowserControl is running (port 8080)\n"
                                                  + "2. Log in to studio.beatstars.com once in that browser\n"
                                                  + "3. Try again")));
                });
            }).detach();
        });
    };

    addAndMakeVisible(overlay.get());
    overlay->toFront(true);
    overlay->grabKeyboardFocus();
    cloudUploadOverlay_ = std::move(overlay);
}

bool MainComponent::uploadBeatToCloud(const juce::String& name, const juce::File& wavFile, const juce::File&)
{
    const double bpm = transportBar_ ? transportBar_->getBPM() : 120.0;
    return uploadBeatViaBrowserControl(getCloudUploadEndpoint(), wavFile, name, bpm, {}, [](const juce::String&) {}).ok;
}

bool MainComponent::exportAudioToFile(const juce::File& wavFile, int soloChannel, bool includeRack, bool includePlaylistLoops)
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
    // Export the full playlist arrangement whenever clips exist — do not depend
    // on the current RACK/PLAYLIST toggle, which can desync from what you hear.
    const bool exportPlaylist = playlist_ != nullptr
                             && playlist_->getContentEndBar() > 0.01f;

    auto isMelodicChannel = [](const ChannelRack::Channel& ch)
    {
        if (ch.isMusicLoop)
            return false;
        return ch.pluginSlotId >= 0
            || ch.builtInInstrument == "piano"
            || ch.builtInInstrument == "guitar"
            || ch.builtInInstrument == "bass"
            || ch.type == ChannelRack::InstrumentType::Lead
            || ch.type == ChannelRack::InstrumentType::Pad
            || ch.type == ChannelRack::InstrumentType::Bass;
    };

    auto exportPatternChannelIndex = [&](int rackIndex) -> int
    {
        if (rackIndex < 0 || rackIndex >= (int)channels.size())
            return rackIndex;
        if (channels[(size_t)rackIndex].isMusicLoop)
            return -1;
        const int drumIndex = channelRack_->getDrumChannelIndexAmongDrums(rackIndex);
        return drumIndex >= 0 ? drumIndex : rackIndex;
    };

    auto exportLocalStepForChannel = [&](int absoluteStep, int rackIndex) -> int
    {
        if (!exportPlaylist || !playlist_)
            return absoluteStep % totalSteps;
        return playlist_->patternLocalStepForChannelAt(absoluteStep, totalSteps,
                                                       exportPatternChannelIndex(rackIndex));
    };

    auto exportAllowsChannelAtStep = [&](int absoluteStep, int rackIndex) -> bool
    {
        if (!exportPlaylist || !playlist_)
            return true;
        if (channels[(size_t)rackIndex].isMusicLoop)
            return false;
        return playlist_->patternAllowsChannelAtStep(absoluteStep, exportPatternChannelIndex(rackIndex));
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

    struct ScheduledPlaylistSample
    {
        juce::int64 sample = 0;
        juce::File file;
        int track = -1;
        double offsetSeconds = 0.0;
        double rate = 1.0;
        double maxSeconds = -1.0;
        float volume = 1.0f;
        bool fired = false;
    };
    std::vector<ScheduledPlaylistSample> scheduledPlaylistSamples;
    if (exportPlaylist && playlist_ && includePlaylistLoops)
    {
        for (const auto& clip : playlist_->getSampleClipRenderInfos(bpm))
        {
            const juce::int64 startSample = (juce::int64)clip.startStep * samplesPerStep;
            if (startSample > totalSamples64)
                continue;

            scheduledPlaylistSamples.push_back({
                startSample,
                clip.file,
                clip.mixerTrack,
                clip.startOffsetSeconds,
                clip.playbackRate,
                (double)clip.lengthSteps * secondsPerStep,
                clip.volume,
                false
            });
        }
    }

    auto triggerExportChannel = [&](int i, int absoluteStep, juce::int64 hitSample, int sampleOffsetInBlock)
    {
        if (!includeRack || (soloChannel >= 0 && i != soloChannel))
            return;

        if (channels[(size_t)i].isMusicLoop)
            return;

        int localStep = exportLocalStepForChannel(absoluteStep, i);
        if (exportPlaylist && localStep < 0)
            return;
        if (!exportAllowsChannelAtStep(absoluteStep, i))
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
            for (const auto& n : ch.pianoRollNotes)
                if (n.startStep == channelStep)
                    notes.push_back(n);
            if (notes.empty()
                && channelStep >= 0
                && channelStep < (int)ch.steps.size()
                && ch.steps[(size_t)channelStep])
            {
                notes.push_back({ ChannelRack::DEFAULT_DRUM_PITCH, channelStep, 1, 100 });
            }
        }

        if (notes.empty())
            return;

        const int track = (ch.mixerTrack >= 0) ? ch.mixerTrack : i;
        const double hitSeconds = (double)hitSample / sampleRate;
        if (ch.pluginSlotId >= 0)
        {
            const int minRealFeelSteps = pianoRealFeel_ ? 6 : 1;
            if (!melodic)
                pluginHost_.sendAllNotesOff(ch.pluginSlotId, 1);

            for (const auto& n : notes)
            {
                const int pitch = juce::jlimit(0, 127, n.pitch);
                const int velocity = juce::jlimit(1, 127, (int)std::round(ch.volume * (float)n.velocity));
                const int holdSteps = juce::jmax(minRealFeelSteps, n.lengthSteps);
                pluginHost_.setSlotTrack(ch.pluginSlotId, track);
                pluginHost_.sendMidiNote(ch.pluginSlotId, 1, pitch, velocity, true, sampleOffsetInBlock);
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
                pluginHost_.playSynthPiano(n.pitch, hitSeconds,
                                           secondsPerStep * juce::jmax(minRealFeelSteps, n.lengthSteps),
                                           velocity, track);
            }
            return;
        }

        if (ch.builtInInstrument == "bass")
        {
            for (const auto& n : notes)
            {
                const float velocity = juce::jlimit(0.0f, 1.0f, ch.volume * ((float)n.velocity / 127.0f));
                pluginHost_.playSynthBass(n.pitch, hitSeconds,
                                          secondsPerStep * juce::jmax(1, n.lengthSteps), velocity, track);
            }
            return;
        }

        if (ch.sampleFile.existsAsFile())
        {
            pluginHost_.stopSampleVoicesOnTrack(ch.sampleFile, track, true);
            pluginHost_.playSampleFile(ch.sampleFile, track, 0.0, ch.volume, 1.0, -1.0, sampleOffsetInBlock);
        }
    };

    auto queueStepHits = [&](int absoluteStep, juce::int64 stepSample)
    {
        for (int i = 0; i < (int)channels.size(); ++i)
        {
            if (!includeRack || (soloChannel >= 0 && i != soloChannel))
                continue;

            if (channels[(size_t)i].isMusicLoop)
                continue;

            const int localStep = exportLocalStepForChannel(absoluteStep, i);
            if (exportPlaylist && localStep < 0)
                continue;
            if (!exportAllowsChannelAtStep(absoluteStep, i))
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
            else
            {
                if (channelStep >= 0 && channelStep < (int)ch.steps.size())
                    hasHit = ch.steps[(size_t)channelStep];
                if (!hasHit)
                {
                    for (const auto& n : ch.pianoRollNotes)
                        if (n.startStep == channelStep) { hasHit = true; break; }
                }
            }

            if (!hasHit)
                continue;

            const double swingDelay = channelRack_
                ? channelRack_->getSwingDelaySeconds(channelStep, ch) : 0.0;
            const juce::int64 hitSample = stepSample
                + (juce::int64)std::llround(swingDelay * sampleRate);
            scheduledHits.push_back({ hitSample, absoluteStep, i });
        }
    };

    const bool restorePlaylistPlayback = playbackMode_ == TransportBar::PlaybackMode::Playlist;
    const juce::ScopedLock renderGuard(pluginHost_.getRenderLock());
    pluginHost_.clearTransientPlayback();
    if (playlist_)
    {
        playlist_->setPlaybackEnabled(false);
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

        while ((juce::int64)nextStep * samplesPerStep < samplePos + n
               && (double)nextStep * secondsPerStep <= contentSeconds + 0.0001)
        {
            const juce::int64 stepSample = (juce::int64)nextStep * samplesPerStep;
            queueStepHits(nextStep, stepSample);
            ++nextStep;
        }

        for (auto& sample : scheduledPlaylistSamples)
        {
            if (sample.fired || sample.sample < samplePos || sample.sample >= samplePos + n)
                continue;

            const int offsetInBlock = (int)(sample.sample - samplePos);
            pluginHost_.playSampleFile(sample.file,
                                       sample.track,
                                       sample.offsetSeconds,
                                       sample.volume,
                                       sample.rate,
                                       sample.maxSeconds,
                                       offsetInBlock);
            sample.fired = true;
        }

        for (const auto& hit : scheduledHits)
        {
            if (hit.sample >= samplePos && hit.sample < samplePos + n)
            {
                const int offsetInBlock = (int)(hit.sample - samplePos);
                triggerExportChannel(hit.channelIdx, hit.absoluteStep, hit.sample, offsetInBlock);
            }
        }
        scheduledHits.erase(
            std::remove_if(scheduledHits.begin(), scheduledHits.end(),
                [&](const ScheduledExportHit& h) { return h.sample < samplePos + n; }),
            scheduledHits.end());

        float* outs[2] = { block.getWritePointer(0), block.getWritePointer(1) };
        pluginHost_.renderAudioBlock(outs, 2, n);
        writer->writeFromAudioSampleBuffer(block, 0, n);
        samplePos += n;
        sendDueMidiOffs(samplePos);
    }

    for (auto& off : pendingOffs)
        if (off.slot >= 0)
            pluginHost_.sendMidiNote(off.slot, 1, off.pitch, 0, false);
    pluginHost_.clearTransientPlayback();

    if (playlist_)
    {
        playlist_->setPlayhead(0, false, bpm);
        playlist_->setPlaybackEnabled(restorePlaylistPlayback);
    }
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

bool MainComponent::exportStemsToFolder(const juce::File& folder, const juce::String& beatName)
{
    if (!channelRack_)
        return false;

    if (!folder.createDirectory())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Export failed", "Couldn't create stems folder:\n" + folder.getFullPathName());
        return false;
    }

    auto cleanFilePart = [](juce::String name)
    {
        name = name.trim();
        for (auto c : juce::String(R"(\/:*?"<>|)"))
            name = name.replaceCharacter(c, '-');
        name = name.replace("  ", " ");
        return name.isNotEmpty() ? name : juce::String("Stem");
    };

    auto hasRackContent = [](const ChannelRack::Channel& ch)
    {
        if (ch.sampleFile.existsAsFile() || ch.pluginSlotId >= 0 || ch.builtInInstrument.isNotEmpty())
            return true;
        if (!ch.pianoRollNotes.empty())
            return true;
        for (bool step : ch.steps)
            if (step)
                return true;
        return false;
    };

    auto& channels = channelRack_->getChannels();
    bool wroteAny = false;

    for (int i = 0; i < (int)channels.size(); ++i)
    {
        const auto& ch = channels[(size_t)i];
        if (!hasRackContent(ch))
            continue;

        const auto fileName = juce::String::formatted("%02d - %s.wav",
                                                      i + 1,
                                                      cleanFilePart(ch.name).toRawUTF8());
        if (!exportAudioToFile(folder.getChildFile(fileName), i, true, false))
            return false;

        wroteAny = true;
    }

    if (playlist_ && playlist_->hasSampleClips())
    {
        const auto previousMode = playbackMode_;
        playbackMode_ = TransportBar::PlaybackMode::Playlist;
        const bool ok = exportAudioToFile(folder.getChildFile(cleanFilePart(beatName) + " - Loops.wav"),
                                          -1, false, true);
        playbackMode_ = previousMode;
        if (!ok)
            return false;

        wroteAny = true;
    }

    if (!wroteAny)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "No stems exported", "There are no rack channels or playlist loops to export yet.");
        return false;
    }

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
