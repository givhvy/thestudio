#include "MainComponent.h"
#include "Theme.h"
#include "PianoRoll.h"
#include "Midi808ImportSettings.h"
#include "AgentRegistry.h"
#include "MarketplacePanel.h"
#include <algorithm>
#include <array>
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
    std::thread([]()
    {
        std::vector<HWND> windows;
        EnumWindows(collectChordifyWindowsForExternalDrop, reinterpret_cast<LPARAM>(&windows));

        for (auto hwnd : windows)
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

        if (windows.empty())
            return;

        Sleep(8000);
        for (auto hwnd : windows)
            if (IsWindow(hwnd))
                SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }).detach();
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
    if (Theme::aeroMode)
    {
        // Glossy white-aqua glass pill.
        juce::ColourGradient grad(juce::Colours::white.withAlpha(highlighted ? 0.95f : 0.78f),
                                  badge.getX(), badge.getY(),
                                  juce::Colour(0xff9fd8ec).withAlpha(0.9f),
                                  badge.getX(), badge.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(badge, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.fillRoundedRectangle(badge.withHeight(badge.getHeight() * 0.5f).reduced(1.0f, 0.8f), 2.0f);
        g.setColour(juce::Colour(0xff0e7490).withAlpha(0.8f));
        g.drawRoundedRectangle(badge.reduced(0.5f), 3.0f, 0.8f);
        return;
    }

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
    g.setColour(Theme::aeroMode ? juce::Colour(0xff083344) : juce::Colour(0xff71717a));
    g.setFont(titleBarBadgeFont());
    g.drawText(text, badge.toNearestInt(), juce::Justification::centred);
}

juce::File cloudUploadConfigFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stratum DAW")
        .getChildFile("cloud-upload.json");
}

juce::File stratumDocumentsRoot()
{
   #if JUCE_WINDOWS
    return juce::File("D:\\stratumdaw");
   #else
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("stratumdaw");
   #endif
}

// All albums live under <docs>/Albums/<name>/.
juce::File stratumAlbumsRoot()
{
    auto a = stratumDocumentsRoot().getChildFile("Albums");
    a.createDirectory();
    return a;
}

// Tracks the currently active album (empty string = no album).
juce::File stratumCurrentAlbumFile()
{
    return stratumDocumentsRoot().getChildFile("current_album.txt");
}

juce::String loadCurrentAlbum()
{
    auto f = stratumCurrentAlbumFile();
    if (!f.existsAsFile()) return {};
    return f.loadFileAsString().trim();
}

void saveCurrentAlbum(const juce::String& name)
{
    stratumDocumentsRoot().createDirectory();
    stratumCurrentAlbumFile().replaceWithText(name);
}

// Returns the folder where Save should land. Album folder if one is active,
// otherwise the top-level docs root.
juce::File stratumSaveRoot()
{
    auto album = loadCurrentAlbum();
    if (album.isEmpty()) return stratumDocumentsRoot();
    auto f = stratumAlbumsRoot().getChildFile(album);
    f.createDirectory();
    return f;
}

juce::File stratumPinterestOutputDir()
{
   #if JUCE_WINDOWS
    return juce::File("D:\\folderforpinterest");
   #else
    return juce::File::getSpecialLocation(juce::File::userPicturesDirectory)
        .getChildFile("Stratum Pinterest");
   #endif
}

constexpr const char* kStratumCurrentVersion = "1.0.0";
constexpr const char* kStratumLatestReleaseApi = "https://api.github.com/repos/givhvy/thestudio/releases/latest";
constexpr const char* kStratumReleasesPage = "https://github.com/givhvy/thestudio/releases/latest";

juce::String normaliseVersionTag(juce::String tag)
{
    tag = tag.trim().toLowerCase();
    if (tag.startsWithChar('v'))
        tag = tag.substring(1);
    if (tag.startsWith("mac-"))
        tag = tag.fromFirstOccurrenceOf("mac-", false, false);
    return tag;
}

std::array<int, 3> parseVersionParts(const juce::String& tag)
{
    std::array<int, 3> parts { 0, 0, 0 };
    auto tokens = juce::StringArray::fromTokens(normaliseVersionTag(tag), ".", "");
    for (int i = 0; i < juce::jmin(3, tokens.size()); ++i)
        parts[(size_t)i] = tokens[i].getIntValue();
    return parts;
}

bool isVersionNewer(const juce::String& latest, const juce::String& current)
{
    auto a = parseVersionParts(latest);
    auto b = parseVersionParts(current);
    for (int i = 0; i < 3; ++i)
    {
        if (a[(size_t)i] > b[(size_t)i]) return true;
        if (a[(size_t)i] < b[(size_t)i]) return false;
    }
    return normaliseVersionTag(latest).isNotEmpty()
        && normaliseVersionTag(latest) != normaliseVersionTag(current);
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

class Midi808SettingsOverlay : public juce::Component
{
public:
    Midi808SettingsOverlay()
    {
        setWantsKeyboardFocus(true);
        lowestNotesOnly_ = Midi808ImportSettings::get().lowestNotesOnly;
        foldToC4C6_ = Midi808ImportSettings::get().foldToC4C6;
    }

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
        g.drawText("808 MIDI Import", titleRect_, juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f));
        g.drawText("Applies to Chordify import, wait for 808 slot, and piano roll MIDI drops.",
                   subtitleRect_, juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText("LOWEST NOTES ONLY", option1LabelRect_, juce::Justification::centredLeft, true);
        g.drawText("FOLD TO C4-C6 RANGE", option2LabelRect_, juce::Justification::centredLeft, true);

        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
        g.drawText("Keep only the lowest note at each step when importing MIDI.",
                   option1HintRect_, juce::Justification::centredLeft, true);
        g.drawText("Map imported pitches into the C4-C6 octave window.",
                   option2HintRect_, juce::Justification::centredLeft, true);

        drawPill(g, closeRect_, "X", false);
        drawPill(g, lowestOffRect_, "OFF", !lowestNotesOnly_);
        drawPill(g, lowestOnRect_, "ON", lowestNotesOnly_);
        drawPill(g, foldOffRect_, "OFF", !foldToC4C6_);
        drawPill(g, foldOnRect_, "ON", foldToC4C6_);
        drawPill(g, doneRect_, "DONE", true);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        updateLayout();
        if (closeRect_.contains(e.x, e.y))
        {
            if (onClose) onClose();
            return;
        }
        if (lowestOffRect_.contains(e.x, e.y))
        {
            lowestNotesOnly_ = false;
            persist();
            repaint();
            return;
        }
        if (lowestOnRect_.contains(e.x, e.y))
        {
            lowestNotesOnly_ = true;
            persist();
            repaint();
            return;
        }
        if (foldOffRect_.contains(e.x, e.y))
        {
            foldToC4C6_ = false;
            persist();
            repaint();
            return;
        }
        if (foldOnRect_.contains(e.x, e.y))
        {
            foldToC4C6_ = true;
            persist();
            repaint();
            return;
        }
        if (doneRect_.contains(e.x, e.y))
        {
            if (onClose) onClose();
        }
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
            if (onClose) onClose();
            return true;
        }
        return false;
    }

private:
    bool lowestNotesOnly_ = true;
    bool foldToC4C6_ = true;
    juce::Rectangle<int> panel_, titleRect_, subtitleRect_;
    juce::Rectangle<int> option1LabelRect_, option1HintRect_, lowestOffRect_, lowestOnRect_;
    juce::Rectangle<int> option2LabelRect_, option2HintRect_, foldOffRect_, foldOnRect_;
    juce::Rectangle<int> closeRect_, doneRect_;

    void persist()
    {
        auto& settings = Midi808ImportSettings::get();
        settings.lowestNotesOnly = lowestNotesOnly_;
        settings.foldToC4C6 = foldToC4C6_;
        settings.save();
        if (settings.onChanged)
            settings.onChanged();
    }

    void updateLayout()
    {
        const int width = juce::jlimit(420, 560, getWidth() - 80);
        const int height = 340;
        panel_ = juce::Rectangle<int>((getWidth() - width) / 2, (getHeight() - height) / 2, width, height);

        auto content = panel_.reduced(28, 24);
        closeRect_ = juce::Rectangle<int>(panel_.getRight() - 46, panel_.getY() + 18, 28, 26);
        titleRect_ = content.removeFromTop(30);
        subtitleRect_ = content.removeFromTop(42);
        content.removeFromTop(10);

        option1LabelRect_ = content.removeFromTop(18);
        option1HintRect_ = content.removeFromTop(22);
        auto row1 = content.removeFromTop(30);
        lowestOffRect_ = row1.removeFromLeft(72);
        row1.removeFromLeft(8);
        lowestOnRect_ = row1.removeFromLeft(72);
        content.removeFromTop(16);

        option2LabelRect_ = content.removeFromTop(18);
        option2HintRect_ = content.removeFromTop(22);
        auto row2 = content.removeFromTop(30);
        foldOffRect_ = row2.removeFromLeft(72);
        row2.removeFromLeft(8);
        foldOnRect_ = row2.removeFromLeft(72);

        doneRect_ = panel_.reduced(28, 24).removeFromBottom(42).removeFromRight(120);
    }

    static void drawPill(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool active)
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
};

// ── Export success toast (bottom-right, auto-dismisses after 5s) ─────────────
class ExportSuccessToast : public juce::Component, private juce::Timer
{
public:
    ExportSuccessToast(const juce::String& title,
                       const juce::String& projectPath,
                       const juce::String& extraPath)
        : title_(title), projectPath_(projectPath), extraPath_(extraPath)
    {
        setSize(390, extraPath.isNotEmpty() ? 140 : 112);
        startTimerHz(60);
    }

    std::function<void()> onDismiss;

    void paint(juce::Graphics& g) override
    {
        const float a = juce::jmin(1.0f, animAlpha_);
        const auto b = getLocalBounds().toFloat();

        // Green glow halo
        for (int i = 6; i > 0; --i)
        {
            g.setColour(juce::Colour(0xff10b981).withAlpha(0.028f * a / (float)i));
            g.fillRoundedRectangle(b.expanded((float)i * 5.0f), 20.0f);
        }

        // Glass panel body
        juce::ColourGradient body(juce::Colour(0xff161624).withAlpha(0.97f * a), b.getX(), b.getY(),
                                  juce::Colour(0xff0c0c12).withAlpha(0.98f * a), b.getRight(), b.getBottom(), false);
        g.setGradientFill(body);
        g.fillRoundedRectangle(b, 14.0f);

        // Outer white shimmer border
        g.setColour(juce::Colours::white.withAlpha(0.10f * a));
        g.drawRoundedRectangle(b.reduced(0.5f), 14.0f, 1.0f);
        // Inner green accent border
        g.setColour(juce::Colour(0xff10b981).withAlpha(0.50f * a));
        g.drawRoundedRectangle(b.reduced(1.5f), 13.0f, 1.0f);

        // Top-left colour stripe
        juce::ColourGradient stripe(juce::Colour(0xff10b981).withAlpha(0.55f * a), b.getX(), b.getY(),
                                    juce::Colour(0xff059669).withAlpha(0.0f), b.getX() + 120.0f, b.getY(), false);
        g.setGradientFill(stripe);
        g.fillRoundedRectangle(b.withWidth(160.0f).withHeight(3.0f), 1.5f);

        // Checkmark circle
        const float cx = 32.0f, cy = 32.0f, cr = 13.5f;
        // Glow behind circle
        g.setColour(juce::Colour(0xff10b981).withAlpha(0.20f * a));
        g.fillEllipse(cx - cr - 4, cy - cr - 4, (cr + 4) * 2, (cr + 4) * 2);
        // Circle fill
        juce::ColourGradient circ(juce::Colour(0xff34d399).withAlpha(a), cx - cr, cy - cr,
                                  juce::Colour(0xff059669).withAlpha(a), cx + cr, cy + cr, false);
        g.setGradientFill(circ);
        g.fillEllipse(cx - cr, cy - cr, cr * 2.0f, cr * 2.0f);
        // Tick
        {
            juce::Path tick;
            tick.startNewSubPath(cx - 6.0f, cy + 0.5f);
            tick.lineTo(cx - 1.5f, cy + 5.5f);
            tick.lineTo(cx + 7.0f, cy - 5.0f);
            g.setColour(juce::Colours::white.withAlpha(a));
            g.strokePath(tick, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Title
        g.setColour(juce::Colours::white.withAlpha(a));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(15.0f).withStyle("Bold"));
        g.drawText(title_, 56, 16, getWidth() - 78, 22, juce::Justification::centredLeft);

        // Sub-label: "Done" badge
        g.setColour(juce::Colour(0xff10b981).withAlpha(0.85f * a));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
        g.drawText("DONE", 56, 38, 40, 14, juce::Justification::centredLeft);

        // File paths
        auto drawPath = [&](const juce::String& label, const juce::String& path, int y)
        {
            const juce::String truncated = path.length() > 46 ? ("..." + path.substring(path.length() - 43)) : path;
            g.setColour(juce::Colour(0xff6ee7b7).withAlpha(0.60f * a));
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
            g.drawText(label, 18, y, 46, 14, juce::Justification::centredLeft);
            g.setColour(juce::Colour(0xff9ca3af).withAlpha(0.80f * a));
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
            g.drawText(truncated, 64, y, getWidth() - 76, 14, juce::Justification::centredLeft);
        };
        if (projectPath_.isNotEmpty()) drawPath("Project", projectPath_, extraPath_.isNotEmpty() ? 70 : 60);
        if (extraPath_.isNotEmpty())   drawPath("File",    extraPath_,    92);

        // Countdown bar
        const float barFrac = juce::jmax(0.0f, 1.0f - progressFrac_);
        g.setColour(juce::Colour(0xff10b981).withAlpha(0.35f * a));
        g.fillRoundedRectangle(0.0f, (float)getHeight() - 3.0f, (float)getWidth(), 3.0f, 1.5f);
        g.setColour(juce::Colour(0xff34d399).withAlpha(0.70f * a));
        g.fillRoundedRectangle(0.0f, (float)getHeight() - 3.0f, (float)getWidth() * barFrac, 3.0f, 1.5f);

        // Close ×
        const bool hoverClose = closeRect_.contains(mousePos_);
        g.setColour((hoverClose ? juce::Colours::white : juce::Colours::white.withAlpha(0.40f)).withMultipliedAlpha(a));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(14.0f));
        g.drawText(juce::String::charToString(0xD7), closeRect_, juce::Justification::centred);
    }

    void mouseMove(const juce::MouseEvent& e) override  { mousePos_ = e.getPosition(); repaint(); }
    void mouseExit(const juce::MouseEvent&) override    { mousePos_ = { -1, -1 }; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (closeRect_.contains(e.getPosition()))
            startDismiss();
    }

private:
    juce::String title_, projectPath_, extraPath_;
    float animAlpha_ = 0.0f;
    float progressFrac_ = 0.0f;
    bool dismissing_ = false;
    int tickCount_ = 0;
    juce::Point<int> mousePos_ { -1, -1 };
    juce::Rectangle<int> closeRect_ { 0, 6, 26, 26 }; // set in resized

    static constexpr int kHoldTicks = 60 * 5; // 5s

    void resized() override
    {
        closeRect_ = { getWidth() - 28, 8, 22, 22 };
    }

    void timerCallback() override
    {
        if (dismissing_)
        {
            animAlpha_ -= 0.07f;
            if (animAlpha_ <= 0.0f) { stopTimer(); if (onDismiss) onDismiss(); }
        }
        else
        {
            animAlpha_ = juce::jmin(1.0f, animAlpha_ + 0.08f);
            progressFrac_ = juce::jmin(1.0f, (float)(++tickCount_) / (float)kHoldTicks);
            if (progressFrac_ >= 1.0f) startDismiss();
        }
        repaint();
    }

    void startDismiss() { dismissing_ = true; }
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

// Visual album picker — shows albums as cover-art cards (Spotify-style grid).
class AlbumPickerOverlay : public juce::Component
{
public:
    struct AlbumEntry
    {
        juce::String name;       // empty = "no album" sentinel
        juce::File   folder;
        juce::File   coverFile;  // may not exist; procedural fallback drawn
        juce::Image  cover;      // empty if no cover image
        juce::Rectangle<int> rect;  // hit-test in painted layout
    };

    AlbumPickerOverlay(juce::File albumsRoot, juce::String currentAlbum)
        : albumsRoot_(std::move(albumsRoot)),
          current_(std::move(currentAlbum))
    {
        setWantsKeyboardFocus(true);
        rebuildEntries();
    }

    // name = "" → user picked "No album". name = "__create__" → handled internally.
    std::function<void(const juce::String& name)> onPick;
    std::function<void()> onClose;
    std::function<void(const juce::String& name)> onRequestCoverChange;

    void rebuildEntries()
    {
        entries_.clear();
        juce::Array<juce::File> folders;
        albumsRoot_.findChildFiles(folders, juce::File::findDirectories, false);
        folders.sort();
        for (auto& f : folders)
        {
            AlbumEntry e;
            e.name = f.getFileName();
            e.folder = f;
            for (auto* ext : { "cover.png", "cover.jpg", "cover.jpeg" })
            {
                auto c = f.getChildFile(ext);
                if (c.existsAsFile()) { e.coverFile = c; break; }
            }
            if (e.coverFile.existsAsFile())
                e.cover = juce::ImageFileFormat::loadFrom(e.coverFile);
            entries_.push_back(std::move(e));
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        // Dim backdrop.
        g.fillAll(juce::Colours::black.withAlpha(0.72f));

        const int w = getWidth();
        const int h = getHeight();
        const int padX = juce::jmax(40, (w - 1200) / 2);
        const int topY = 70;

        // Title.
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(28.0f).withStyle("Bold"));
        g.drawText("ALBUMS", padX, 20, w - padX * 2, 36, juce::Justification::centredLeft);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.5f));
        g.drawText("CHOOSE AN ALBUM TO ORGANIZE YOUR SAVES",
                   padX, 50, w - padX * 2, 16, juce::Justification::centredLeft);

        // Close X (top-right).
        closeRect_ = juce::Rectangle<int>(w - padX - 32, 28, 24, 24);
        g.setColour(juce::Colours::white.withAlpha(hoverIdx_ == -2 ? 0.95f : 0.55f));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(22.0f).withStyle("Bold"));
        g.drawText("X", closeRect_, juce::Justification::centred);

        // Grid layout.
        const int cardW = 180;
        const int cardH = 230;
        const int gap   = 24;
        const int gridW = w - padX * 2;
        int cols = juce::jmax(1, (gridW + gap) / (cardW + gap));
        int x = padX;
        int y = topY;
        int col = 0;
        const int totalCards = 2 + (int)entries_.size();   // +Create, +None

        auto place = [&](int idx, std::function<void(juce::Rectangle<int>)> draw)
        {
            juce::Rectangle<int> r(x, y, cardW, cardH);
            draw(r);
            cardRects_[idx] = r;
            ++col;
            if (col >= cols) { col = 0; x = padX; y += cardH + gap; }
            else { x += cardW + gap; }
        };

        cardRects_.clear();
        cardRects_.resize((size_t)totalCards);

        // Card 0 — Create new album
        place(0, [&](juce::Rectangle<int> r)
        {
            paintCard(g, r, 0, juce::Image(), "NEW ALBUM", "+ Create",
                      juce::Colour(0xff1d4ed8), juce::Colour(0xff7c3aed), false);
        });
        // Card 1 — None
        place(1, [&](juce::Rectangle<int> r)
        {
            paintCard(g, r, 1, juce::Image(), "NO ALBUM", "Work outside",
                      juce::Colour(0xff52525b), juce::Colour(0xff18181b), current_.isEmpty());
        });
        // Albums.
        for (size_t i = 0; i < entries_.size(); ++i)
        {
            int idx = 2 + (int)i;
            auto& e = entries_[i];
            place(idx, [&](juce::Rectangle<int> r)
            {
                paintCard(g, r, idx, e.cover, e.name.toUpperCase(), {},
                          juce::Colour(0xff334155), juce::Colour(0xff0f172a),
                          e.name == current_);
            });
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int prev = hoverIdx_;
        hoverIdx_ = hitTest(e.getPosition());
        if (hoverIdx_ != prev) repaint();
    }
    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoverIdx_ != -1) { hoverIdx_ = -1; repaint(); }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int idx = hitTest(e.getPosition());
        if (idx == -2) { if (onClose) onClose(); return; }
        if (idx < 0) return;

        if (e.mods.isRightButtonDown())
        {
            if (idx >= 2)
            {
                const auto& entry = entries_[(size_t)(idx - 2)];
                juce::PopupMenu m;
                m.addItem(1, "Set cover image...");
                m.addItem(2, "Open folder");
                m.addSeparator();
                m.addItem(3, "Delete album");
                m.showMenuAsync({}, [this, name = entry.name, folder = entry.folder](int r)
                {
                    if (r == 1 && onRequestCoverChange) onRequestCoverChange(name);
                    else if (r == 2) folder.revealToUser();
                    else if (r == 3) confirmDelete(name);
                });
            }
            return;
        }

        if (idx == 0)
        {
            if (onPick) onPick("__create__");
        }
        else if (idx == 1)
        {
            if (onPick) onPick({});
        }
        else
        {
            if (onPick) onPick(entries_[(size_t)(idx - 2)].name);
        }
    }

    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
        return false;
    }

private:
    int hitTest(juce::Point<int> p) const
    {
        if (closeRect_.contains(p)) return -2;
        for (size_t i = 0; i < cardRects_.size(); ++i)
            if (cardRects_[i].contains(p)) return (int)i;
        return -1;
    }

    void confirmDelete(const juce::String& name)
    {
        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon, "Delete album",
            "Delete album '" + name + "' and ALL its files on disk?",
            "Delete", "Cancel", nullptr,
            juce::ModalCallbackFunction::create([this, name](int ok)
            {
                if (!ok) return;
                albumsRoot_.getChildFile(name).deleteRecursively();
                if (current_ == name) current_ = {};
                rebuildEntries();
            }));
    }

    void paintCard(juce::Graphics& g, juce::Rectangle<int> r, int idx,
                   const juce::Image& cover, const juce::String& title,
                   const juce::String& sub, juce::Colour topC, juce::Colour botC, bool selected)
    {
        const bool hovered = (hoverIdx_ == idx);
        auto fr = r.toFloat();

        // Drop shadow
        for (int i = 4; i > 0; --i)
        {
            g.setColour(juce::Colours::black.withAlpha((hovered ? 0.10f : 0.06f) / (float)i));
            g.fillRoundedRectangle(fr.translated(0.0f, (float)i * 1.5f).expanded((float)i * 0.4f), 10.0f);
        }

        // Cover square at top of card (square area).
        auto coverR = juce::Rectangle<float>(fr.getX(), fr.getY(), fr.getWidth(), fr.getWidth());
        g.saveState();
        juce::Path coverClip;
        coverClip.addRoundedRectangle(coverR.getX(), coverR.getY(), coverR.getWidth(), coverR.getHeight(),
                                      10.0f, 10.0f, true, true, false, false);
        g.reduceClipRegion(coverClip);
        if (cover.isValid())
        {
            g.drawImage(cover, coverR, juce::RectanglePlacement::fillDestination);
        }
        else
        {
            juce::ColourGradient grad(topC, coverR.getX(), coverR.getY(),
                                      botC, coverR.getRight(), coverR.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRect(coverR);
            // Procedural big initial (first letter).
            const juce::String initial = title.isNotEmpty() ? title.substring(0, 1) : juce::String("?");
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(72.0f).withStyle("Bold"));
            g.drawText(initial, coverR.toNearestInt(), juce::Justification::centred);
        }
        g.restoreState();

        // Strip below cover for name.
        auto stripR = juce::Rectangle<float>(fr.getX(), coverR.getBottom(), fr.getWidth(),
                                              fr.getHeight() - coverR.getHeight());
        juce::ColourGradient strip(juce::Colour(0xff1a1a1d), stripR.getX(), stripR.getY(),
                                    juce::Colour(0xff09090b), stripR.getX(), stripR.getBottom(), false);
        g.setGradientFill(strip);
        juce::Path stripClip;
        stripClip.addRoundedRectangle(stripR.getX(), stripR.getY(), stripR.getWidth(), stripR.getHeight(),
                                      10.0f, 10.0f, false, false, true, true);
        g.fillPath(stripClip);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
        g.drawText(title, stripR.reduced(10, 4).withHeight(20).toNearestInt(),
                   juce::Justification::centredLeft, true);
        if (sub.isNotEmpty())
        {
            g.setColour(juce::Colours::white.withAlpha(0.55f));
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f));
            g.drawText(sub, stripR.reduced(10, 4).translated(0, 18).withHeight(16).toNearestInt(),
                       juce::Justification::centredLeft, true);
        }

        // Selection / hover rim.
        if (selected)
        {
            g.setColour(Theme::orange1);
            g.drawRoundedRectangle(fr, 10.0f, 2.5f);
        }
        else if (hovered)
        {
            g.setColour(juce::Colours::white.withAlpha(0.50f));
            g.drawRoundedRectangle(fr, 10.0f, 1.5f);
        }
    }

    juce::File albumsRoot_;
    juce::String current_;
    std::vector<AlbumEntry> entries_;
    std::vector<juce::Rectangle<int>> cardRects_;
    juce::Rectangle<int> closeRect_;
    int hoverIdx_ = -1;
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

class ChangelogOverlay final : public juce::Component
{
public:
    ChangelogOverlay()
    {
        setWantsKeyboardFocus(true);
    }

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

        g.setColour(juce::Colour(0xff151519));
        g.fillRoundedRectangle(panel_.toFloat(), 10.0f);
        g.setColour(Theme::accent.withAlpha(0.70f));
        g.drawRoundedRectangle(panel_.toFloat().reduced(0.5f), 10.0f, 1.0f);

        auto header = panel_.withHeight(46);
        juce::ColourGradient headerGrad(Theme::accentBright, (float)header.getX(), (float)header.getY(),
                                        Theme::accentDim, (float)header.getRight(), (float)header.getBottom(), false);
        g.setGradientFill(headerGrad);
        g.fillRoundedRectangle(header.toFloat(), 10.0f);
        g.setColour(juce::Colour(0xff151519));
        g.fillRect(header.withTrimmedTop(36));

        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(15.0f).withStyle("Bold"));
        g.drawText("CHANGELOG", header.reduced(22, 0), juce::Justification::centredLeft, true);

        drawPill(g, closeRect_, "X", false);

        {
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(contentRect_);
            g.addTransform(juce::AffineTransform::translation(0.0f, (float)-scrollY_));
            drawEntries(g, contentRect_.translated(0, scrollY_));
        }

        drawScrollbar(g);
    }

    void resized() override
    {
        updateLayout();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        updateLayout();
        if (closeRect_.contains(e.getPosition()) || !panel_.contains(e.getPosition()))
        {
            if (onClose) onClose();
            return;
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        updateLayout();
        const int maxScroll = juce::jmax(0, contentHeight_ - contentRect_.getHeight());
        scrollY_ = juce::jlimit(0, maxScroll, scrollY_ - (int)std::round(wheel.deltaY * 86.0f));
        repaint();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose) onClose();
            return true;
        }
        return false;
    }

private:
    struct Entry
    {
        juce::String date;
        juce::String title;
        juce::StringArray items;
    };

    juce::Rectangle<int> panel_, contentRect_, closeRect_;
    int scrollY_ = 0;
    int contentHeight_ = 0;

    static const std::vector<Entry>& entries()
    {
        static const std::vector<Entry> data = {
            { "2026-06-06", "All-drums swing piano roll",
                { "Opening a drum channel in Piano Roll now shows Kick, Snare, Hat, Ride, Rim, and other drum rows together.",
                  "The swing view maps each lane back to its Channel Rack sound slot so timing edits and auditions stay tied to the correct drum." } },
            { "2026-06-06", "Visible swing timing",
                { "Channel Rack now draws purple timing markers on swung drum hits.",
                  "Piano Roll now shows ghost note positions for the active swing feel so users can see how Dilla, MF DOOM, or Joey timing changes playback." } },
            { "2026-06-06", "Playlist AutoCut for Chordify",
                { "Added an AUTOCUT toggle to the Playlist header.",
                  "When enabled, new loop drops longer than 16 bars are clipped to 16 bars and Chordify receives a temporary 16-bar WAV for analysis." } },
            { "2026-06-06", "Mouse-anchored playlist zoom",
                { "Ctrl+mouse-wheel zoom in the Playlist now follows the mouse position.",
                  "The bar under the cursor stays anchored while zooming, closer to FL Studio's timeline feel." } },
            { "2026-06-06", "Playlist right-drag erase",
                { "Plain right-click still deletes a playlist clip immediately.",
                  "Holding right-click and dragging now erases every pattern or loop clip the mouse passes over." } },
            { "2026-06-06", "Loop playback BPM resync",
                { "Changing BPM while playlist loops are playing now clears stale loop voices immediately.",
                  "Tempo-synced loops restart from the current playhead at the new BPM without needing Space stop/play." } },
            { "2026-06-06", "AI genre middle-click patterns",
                { "Middle-clicking a genre preset in the AI Assistant now cycles to the next drum pattern for that genre.",
                  "This changes only the drum MIDI/steps and keeps the current drum sounds loaded." } },
            { "2026-06-04", "Clean zoomed-out timeline",
                { "Playlist ruler labels now space themselves automatically as you zoom out.",
                  "Long arrangements use cleaner major ticks and subtle grid lines instead of overlapping bar numbers." } },
            { "2026-06-04", "Playlist zoom range",
                { "Added - / FIT / + zoom controls to the Playlist header.",
                  "Playlist zoom can now pull back far enough to view long 3-4 minute arrangements at once." } },
            { "2026-06-04", "Create Video auto-save + player fix",
                { "Create Video now saves the project and a WAV copy automatically while starting the render.",
                  "The clean in-app video preview now loads rendered videos through an encoded same-folder wrapper to avoid WebView2 error code 9." } },
            { "2026-06-03", "Changelog tab",
                { "Added a CHANGELOG button in the top title strip.",
                  "New updates can now be documented inside the DAW so users can quickly see what changed." } },
            { "2026-06-03", "One-click backups",
                { "Added a BACKUP button in the top title strip.",
                  "Each click saves the current project as backup 1, backup 2, and so on without opening a modal." } },
            { "2026-06-03", "Clean rendered video preview",
                { "Finished Beats Studio renders now open directly inside the DAW.",
                  "The rendered video window shows only the video and an X button that stops playback and closes it." } },
            { "2026-06-03", "Mixer Auto Frequency Mix",
                { "Added AUTO FREQ in the mixer for Boom Bap, Trap, R&B, Drake/Gunna, and Lo-Fi frequency targets.",
                  "The mixer now shows a frequency-balance panel and can insert/update native Auto Frequency EQs for each track role." } },
            { "2026-06-03", "Channel Rack copy / paste between sessions",
                { "Ctrl+C copies the current Channel Rack session pattern.",
                  "Ctrl+V pastes that pattern into a new project while keeping steps, MIDI notes, samples, genre, swing, and pattern length." } },
            { "2026-06-03", "Mixer dB metering and AutoMix",
                { "Mixer strips now show fader dB and live peak dB.",
                  "AutoMix applies genre-aware target levels for drums, 808s, loops, instruments, and master." } },
            { "2026-06-03", "FX bypass and automation",
                { "FX chain dots can turn effects on or off without removing the plugin.",
                  "Right-click an FX slot to create playlist automation for effect enable/disable changes." } },
            { "2026-06-03", "Playlist and MIDI workflow",
                { "Pattern clips now expand to match 32-step racks and long piano roll MIDI notes.",
                  "Chordify MIDI drops can target 808 channels automatically, with fallback Waiting for 808 slots." } },
            { "2026-06-03", "Export and stems workflow",
                { "Save now opens the export audio workflow and saves the project together with WAV output.",
                  "Export stems creates a named stems folder with separated Channel Rack slots and loops." } }
        };
        return data;
    }

    void updateLayout()
    {
        const int width = juce::jlimit(560, 860, getWidth() - 100);
        const int height = juce::jlimit(420, 660, getHeight() - 90);
        panel_ = { (getWidth() - width) / 2, (getHeight() - height) / 2, width, height };
        closeRect_ = { panel_.getRight() - 44, panel_.getY() + 10, 28, 26 };
        contentRect_ = panel_.reduced(26, 18).withTrimmedTop(48);
        contentHeight_ = calculateContentHeight(contentRect_.getWidth());
        scrollY_ = juce::jlimit(0, juce::jmax(0, contentHeight_ - contentRect_.getHeight()), scrollY_);
    }

    int calculateContentHeight(int width) const
    {
        int y = 0;
        for (const auto& entry : entries())
        {
            y += 24;
            for (const auto& item : entry.items)
                y += textHeightFor(item, width - 28, 13.0f) + 8;
            y += 22;
        }
        return y + 10;
    }

    static int textHeightFor(const juce::String& text, int width, float fontHeight)
    {
        juce::Font font(juce::FontOptions().withName("Segoe UI").withHeight(fontHeight));
        juce::GlyphArrangement glyphs;
        glyphs.addJustifiedText(font, text, 0.0f, 0.0f, (float)juce::jmax(80, width), juce::Justification::topLeft);
        return juce::jmax(18, (int)std::ceil(glyphs.getBoundingBox(0, glyphs.getNumGlyphs(), true).getHeight()) + 2);
    }

    void drawEntries(juce::Graphics& g, juce::Rectangle<int> area)
    {
        int y = area.getY();
        for (const auto& entry : entries())
        {
            auto titleRow = juce::Rectangle<int>(area.getX(), y, area.getWidth(), 22);
            g.setColour(Theme::accentBright);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.5f).withStyle("Bold"));
            g.drawText(entry.date + "  -  " + entry.title, titleRow, juce::Justification::centredLeft, true);
            y += 26;

            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f));
            for (const auto& item : entry.items)
            {
                const int h = textHeightFor(item, area.getWidth() - 32, 13.0f);
                auto bullet = juce::Rectangle<int>(area.getX() + 8, y + 7, 5, 5);
                g.setColour(Theme::accent.withAlpha(0.85f));
                g.fillEllipse(bullet.toFloat());
                g.setColour(Theme::zinc200);
                g.drawFittedText(item, area.getX() + 24, y, area.getWidth() - 30, h,
                                 juce::Justification::topLeft, 3);
                y += h + 8;
            }

            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.drawHorizontalLine(y + 3, (float)area.getX(), (float)area.getRight());
            y += 22;
        }
    }

    void drawScrollbar(juce::Graphics& g)
    {
        const int maxScroll = juce::jmax(0, contentHeight_ - contentRect_.getHeight());
        if (maxScroll <= 0)
            return;

        auto track = juce::Rectangle<int>(contentRect_.getRight() + 8, contentRect_.getY(), 4, contentRect_.getHeight());
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRoundedRectangle(track.toFloat(), 2.0f);
        const float thumbH = juce::jmax(28.0f, track.getHeight() * (contentRect_.getHeight() / (float)contentHeight_));
        const float thumbY = track.getY() + (track.getHeight() - thumbH) * (scrollY_ / (float)maxScroll);
        g.setColour(Theme::accentBright.withAlpha(0.90f));
        g.fillRoundedRectangle((float)track.getX(), thumbY, (float)track.getWidth(), thumbH, 2.0f);
    }

    void drawPill(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool active)
    {
        juce::ColourGradient bg(active ? Theme::accentBright : juce::Colours::white.withAlpha(0.12f),
                                (float)r.getX(), (float)r.getY(),
                                active ? Theme::accentDim : juce::Colour(0xff222226),
                                (float)r.getRight(), (float)r.getBottom(), false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(r.toFloat(), 8.0f);
        g.setColour(active ? Theme::accentBright : juce::Colours::white.withAlpha(0.18f));
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 8.0f, 1.0f);
        g.setColour(active ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(text, r, juce::Justification::centred, true);
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

    const auto mcT0 = juce::Time::getMillisecondCounterHiRes();
    auto mcLog = [&mcT0](const char* what)
    {
        juce::Logger::writeToLog("[startup/mc] " + juce::String(what) + " +"
            + juce::String(juce::Time::getMillisecondCounterHiRes() - mcT0, 1) + "ms");
    };

    ConsistencyPanel::recordSessionStart();
    mcLog("recordSessionStart");

    transportBar_ = std::make_unique<TransportBar>(pluginHost_);
    mcLog("TransportBar");
    channelRack_ = std::make_unique<ChannelRack>(pluginHost_);
    mcLog("ChannelRack");
    mixer_ = std::make_unique<Mixer>(pluginHost_);
    mcLog("Mixer");
    browser_ = std::make_unique<Browser>(pluginHost_);
    mcLog("Browser");
    playlist_ = std::make_unique<Playlist>(pluginHost_);
    mcLog("Playlist");
    bottomDock_ = std::make_unique<BottomDock>();
    mcLog("BottomDock");
    pianoRoll_ = std::make_unique<PianoRoll>(pluginHost_);
    mcLog("PianoRoll");
    aiPanel_ = std::make_unique<AIPanel>();
    mcLog("AIPanel");
    patternsPanel_ = std::make_unique<PatternsPanel>();
    mcLog("PatternsPanel");
    videoPanel_ = std::make_unique<VideoPanel>();
    mcLog("VideoPanel");
    consistencyPanel_ = std::make_unique<ConsistencyPanel>();
    mcLog("ConsistencyPanel");
    orgChartPanel_ = std::make_unique<OrgChartPanel>();
    mcLog("OrgChartPanel");
    orgChartPanel_->onRunAgent = [this](const juce::String& agentId) { runOrgChartAgent(agentId); };
    orgChartPanel_->onRunAllEnabled = [this]() { runAllEnabledOrgChartAgents(); };
    youTubePanel_ = std::make_unique<YouTubePanel>(stratumDocumentsRoot());
    mcLog("YouTubePanel");

    addAndMakeVisible(*transportBar_);
    addAndMakeVisible(*playlist_);
    addAndMakeVisible(*pianoRoll_);
    addAndMakeVisible(*mixer_);
    addAndMakeVisible(*channelRack_);
    addAndMakeVisible(*bottomDock_);
    addAndMakeVisible(*browser_);
    addChildComponent(*aiPanel_);          // hidden until user clicks AI button
    addChildComponent(*patternsPanel_);    // hidden until user clicks PATTERNS
    addChildComponent(*videoPanel_);       // hidden until user clicks VIDEO button
    addChildComponent(*consistencyPanel_); // hidden until CONSISTENCY tab is clicked
    addChildComponent(*orgChartPanel_);    // hidden until AGENTS tab is clicked
    addChildComponent(*youTubePanel_);     // hidden until YOUTUBE tab is clicked
    videoPanel_->onClose = [this](){
        if (videoPanel_->isEmbeddedInSession())
        {
            videoPanel_->unembedPlayerFromSession();
            if (bottomDock_)
                bottomDock_->setSessionVideoMode(false);
        }
        if (bottomDock_)
            bottomDock_->setButtonActive(4, false);
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
    bottomDock_->getChordifyReady = []() { return ChordifyAutomationEngine::isFullyReady(); };
    bottomDock_->getChordifyRunning = []() { return ChordifyAutomationEngine::isRunning(); };
    bottomDock_->onChordifyRestart = [this]() { handleChordifyRestart(); };

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

    auto isDrumOverviewChannel = [isAkaiMpcChannel](const ChannelRack::Channel& ch)
    {
        if (isAkaiMpcChannel(ch) || ch.isMusicLoop)
            return false;

        if (ch.type == ChannelRack::InstrumentType::Bass
            || ch.type == ChannelRack::InstrumentType::Lead
            || ch.type == ChannelRack::InstrumentType::Pad)
            return false;

        return true;
    };

    auto refreshPianoRollChannel = [this, isKickChannelForPianoRoll, is808ChannelForPianoRoll, isAkaiMpcChannel, isDrumOverviewChannel](int channelIndex)
    {
        auto& channels = channelRack_->getChannels();
        if (channelIndex < 0 || channelIndex >= (int)channels.size())
            return;

        const auto& ch = channels[(size_t)channelIndex];
        pianoRollDrumOverview_ = false;
        pianoRollDrumOverviewChannels_.clear();

        pianoRoll_->setChannelName(ch.name);
        pianoRoll_->setChannelContext(isKickChannelForPianoRoll(ch), is808ChannelForPianoRoll(ch));
        pianoRoll_->setSwingVisualLabel(channelRack_->getSwingPreset() == ChannelRack::SwingPreset::None
            ? juce::String()
            : channelRack_->getSwingPresetLabel());

        std::vector<PianoRollNote> pianoNotes;
        if (isDrumOverviewChannel(ch))
        {
            juce::StringArray laneNames;
            constexpr int topPitch = 84;
            int lane = 0;
            for (int i = 0; i < (int)channels.size(); ++i)
            {
                const auto& src = channels[(size_t)i];
                if (!isDrumOverviewChannel(src))
                    continue;

                pianoRollDrumOverviewChannels_.push_back(i);
                laneNames.add(src.name);
                const int lanePitch = topPitch - lane;
                if (!src.pianoRollNotes.empty())
                {
                    for (const auto& n : src.pianoRollNotes)
                    {
                        pianoNotes.push_back({ lanePitch, n.startStep, n.lengthSteps, n.velocity,
                                               channelRack_->getSwingDelaySteps(n.startStep, src) });
                    }
                }
                else
                {
                    for (int step = 0; step < (int)src.steps.size(); ++step)
                        if (src.steps[(size_t)step])
                            pianoNotes.push_back({ lanePitch, step, 1, 100,
                                                   channelRack_->getSwingDelaySteps(step, src) });
                }
                ++lane;
            }

            pianoRollDrumOverview_ = !pianoRollDrumOverviewChannels_.empty();
            pianoRoll_->setChannelName("Drums");
            pianoRoll_->setNotes(pianoNotes);
            pianoRoll_->setDrumLaneNames(laneNames, topPitch);
            return;
        }

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
                    {
                        pianoNotes.push_back({ lanePitch, n.startStep, n.lengthSteps, n.velocity,
                                               channelRack_->getSwingDelaySteps(n.startStep, src) });
                    }
                }
                else
                {
                    for (int step = 0; step < (int)src.steps.size(); ++step)
                        if (src.steps[(size_t)step])
                            pianoNotes.push_back({ lanePitch, step, 1, 100,
                                                   channelRack_->getSwingDelaySteps(step, src) });
                }
                ++lane;
            }
            pianoRoll_->setNotes(pianoNotes);
            pianoRoll_->setDrumLaneNames(laneNames, topPitch);
            return;
        }

        for (const auto& n : ch.pianoRollNotes)
            pianoNotes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity,
                                   channelRack_->getSwingDelaySteps(n.startStep, ch) });
        pianoRoll_->setNotes(pianoNotes);
    };

    channelRack_->onChannelsChanged = syncMixerToChannelRack;

    // After undo/redo restores channel-rack state, reload the visible piano roll
    // so its notes reflect the restored snapshot (the piano roll holds its own copy).
    reloadPianoRollFromState_ = [this, refreshPianoRollChannel]()
    {
        if (pianoRollDrumOverview_) return; // overview is rebuilt on demand
        if (pianoRollChannelIndex_ >= 0)
            refreshPianoRollChannel(pianoRollChannelIndex_);
    };

    channelRack_->onChannelDeleted = [this, refreshPianoRollChannel](int deletedIndex)
    {
        if (pianoRollChannelIndex_ == deletedIndex)
        {
            pianoRollChannelIndex_ = channelRack_->getSelectedChannel();
            if (pianoRollChannelIndex_ >= 0)
                refreshPianoRollChannel(pianoRollChannelIndex_);
            else if (pianoRoll_)
                pianoRoll_->setNotes({});
        }
        else if (pianoRollChannelIndex_ > deletedIndex)
        {
            --pianoRollChannelIndex_;
        }
    };
    channelRack_->onSwingChanged = [this, refreshPianoRollChannel]()
    {
        if (pianoRollChannelIndex_ >= 0 && pianoRoll_)
            refreshPianoRollChannel(pianoRollChannelIndex_);
    };
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
        setMixerOverlayVisible(true);
    };

    // Connect channel click to open Piano Roll
    channelRack_->onChannelClicked = [this, refreshPianoRollChannel](int channelIndex) {
        if (pianoRollChannelIndex_ >= 0
            && pianoRollChannelIndex_ != channelIndex
            && pianoRoll_
            && pianoRoll_->onNotesChanged)
        {
            channelRack_->setSelectedChannel(pianoRollChannelIndex_);
            pianoRoll_->onNotesChanged();
        }

        channelRack_->setSelectedChannel(channelIndex);
        setCenterView(CenterView::PianoRoll);
        pianoRollChannelIndex_ = channelIndex;
        refreshPianoRollChannel(channelIndex);
    };

    // Double-click a sample channel name → open the FL-style sample settings.
    channelRack_->onOpenSampleProps = [this](int channelIndex)
    {
        auto& chans = channelRack_->getChannels();
        if (channelIndex < 0 || channelIndex >= (int)chans.size())
            return;
        auto& ch = chans[(size_t)channelIndex];
        if (!ch.sampleFile.existsAsFile())
            return;

        auto editor = std::make_unique<SamplePropsPanel>(ch.sampleFile, ch.name, &ch.sampleProps);
        editor->onChange = [this, channelIndex] {
            channelRack_->auditionChannel(channelIndex);
            channelRack_->repaint();
        };
        editor->onAudition = [this, channelIndex] {
            channelRack_->auditionChannel(channelIndex);
        };

        auto win = std::make_unique<NativeEffectWindow>("Channel Settings - " + ch.name, std::move(editor));
        win->onClose = [this] { samplePropsWindow_.reset(); };
        const int W = win->getWidth();
        const int H = win->getHeight();
        int x = juce::jlimit(0, juce::jmax(0, getWidth()  - W), (getWidth()  - W) / 2);
        int y = juce::jlimit(28, juce::jmax(28, getHeight() - H), (getHeight() - H) / 2);
        win->setBounds(x, y, W, H);
        addAndMakeVisible(win.get());
        win->toFront(true);
        samplePropsWindow_ = std::move(win);
    };

    // Scroll wheel on the PIANO tab: cycle through sound-only channels
    // (isMusicLoop == false), keeping loops out of the rotation.
    transportBar_->onPianoTabScroll = [this, refreshPianoRollChannel](int delta)
    {
        auto& channels = channelRack_->getChannels();

        // Build ordered list of sound channel indices (no loops).
        std::vector<int> soundIdx;
        soundIdx.reserve(channels.size());
        for (int i = 0; i < (int)channels.size(); ++i)
            if (!channels[(size_t)i].isMusicLoop)
                soundIdx.push_back(i);

        if (soundIdx.empty()) return;

        // Find where the current channel sits in the sound-only list.
        int pos = -1;
        for (int i = 0; i < (int)soundIdx.size(); ++i)
            if (soundIdx[(size_t)i] == pianoRollChannelIndex_)
                { pos = i; break; }

        // If nothing is loaded yet, start at index 0; otherwise step by delta.
        if (pos < 0)
            pos = (delta > 0) ? 0 : (int)soundIdx.size() - 1;
        else
            pos = (pos + delta + (int)soundIdx.size()) % (int)soundIdx.size();

        const int newChannel = soundIdx[(size_t)pos];
        if (pianoRollChannelIndex_ >= 0
            && pianoRollChannelIndex_ != newChannel
            && pianoRoll_
            && pianoRoll_->onNotesChanged)
        {
            channelRack_->setSelectedChannel(pianoRollChannelIndex_);
            pianoRoll_->onNotesChanged();
        }

        pianoRollChannelIndex_ = newChannel;
        channelRack_->setSelectedChannel(newChannel);
        refreshPianoRollChannel(newChannel);

        // Switch to Piano Roll view if not already there.
        if (centerView_ != CenterView::PianoRoll)
            setCenterView(CenterView::PianoRoll);
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

        const int channelIndex = channelRack_->applyWaitFor808Midi(notes);
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
            if (pianoRollDrumOverview_ && !pianoRollDrumOverviewChannels_.empty())
            {
                constexpr int topPitch = 84;
                for (int target : pianoRollDrumOverviewChannels_)
                {
                    if (target < 0 || target >= (int)channels.size())
                        continue;

                    auto& laneCh = channels[(size_t)target];
                    laneCh.pianoRollNotes.clear();
                    laneCh.steps.assign((size_t)juce::jmax(channelRack_->getTotalSteps(), (int)laneCh.steps.size()), false);
                }

                for (const auto& n : pianoNotes)
                {
                    const int lane = topPitch - n.pitch;
                    if (lane < 0 || lane >= (int)pianoRollDrumOverviewChannels_.size())
                        continue;

                    const int target = pianoRollDrumOverviewChannels_[(size_t)lane];
                    if (target < 0 || target >= (int)channels.size())
                        continue;

                    auto& laneCh = channels[(size_t)target];
                    const int start = juce::jmax(0, n.startStep);
                    const int len = juce::jmax(1, n.lengthSteps);
                    if (start + len > (int)laneCh.steps.size())
                        laneCh.steps.resize((size_t)(start + len), false);

                    laneCh.pianoRollNotes.push_back({ ChannelRack::DEFAULT_DRUM_PITCH, start, len,
                                                      juce::jlimit(1, 127, n.velocity) });
                    laneCh.steps[(size_t)start] = true;
                }

                channelRack_->repaint();
                syncPlaylistPatternLength();
                return;
            }

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

            if (pianoRollDrumOverview_ && !pianoRollDrumOverviewChannels_.empty())
            {
                constexpr int topPitch = 84;
                const int lane = topPitch - pitch;
                if (lane >= 0 && lane < (int)pianoRollDrumOverviewChannels_.size())
                {
                    const int target = pianoRollDrumOverviewChannels_[(size_t)lane];
                    if (target >= 0 && target < (int)channels.size())
                        channelRack_->auditionChannel(target);
                }
                return;
            }

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
        // Keep the transport LCD clock locked to the same step the scrubber uses.
        transportBar_->setPlaybackStep(absoluteStep, playing);
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
        // Dragging the playlist playhead means the user wants to audition the
        // song arrangement — auto-switch playback source to Playlist (not Rack).
        if (transportBar_
            && transportBar_->getPlaybackMode() != TransportBar::PlaybackMode::Playlist)
            transportBar_->setPlaybackMode(TransportBar::PlaybackMode::Playlist);
        pluginHost_.stopSamplePlaybackImmediate();
        if (channelRack_)
            channelRack_->setAbsoluteStep(absoluteStep);
        if (pianoRoll_)
            pianoRoll_->setPlayhead(absoluteStep, false, transportBar_->getBPM());
        if (transportBar_)
            transportBar_->setPlaybackStep(absoluteStep, transportBar_->isPlaying());
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
        if (transportBar_)
            transportBar_->setPlaybackStep(absoluteStep, transportBar_->isPlaying());
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
        if (pianoRollDrumOverview_
            && std::find(pianoRollDrumOverviewChannels_.begin(), pianoRollDrumOverviewChannels_.end(), channelIdx)
                != pianoRollDrumOverviewChannels_.end()
            && pianoRollChannelIndex_ >= 0)
        {
            refreshPianoRollChannel(pianoRollChannelIndex_);
            return;
        }

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
    transportBar_->onPianoToggle       = [this](){ setCenterView(CenterView::PianoRoll); };
    transportBar_->onMixerToggle       = [this](){ toggleMixerOverlay(); };
    transportBar_->onPlaylistToggle    = [this](){ setCenterView(CenterView::Playlist); };
    transportBar_->onOrgChartToggle    = [this](){ setCenterView(CenterView::OrgChart); };
    transportBar_->onYouTubeToggle     = [this](){ setCenterView(CenterView::YouTube); };
    transportBar_->onConsistencyToggle = [this](){ setCenterView(CenterView::Consistency); };

    // Mixer X (close) → hide the floating overlay
    mixer_->onClose = [this](){ setMixerOverlayVisible(false); };

    // Mixer maximize/restore toggle (FL-style window)
    mixer_->onToggleMaximize = [this]()
    {
        mixerMaximized_ = !mixerMaximized_;
        mixer_->setMaximized(mixerMaximized_);
        resized();
    };
    // Remember the window's user-set bounds while floating.
    mixer_->onWindowBoundsChanged = [this](juce::Rectangle<int> b)
    {
        if (!mixerMaximized_)
            mixerFloatBounds_ = b;
    };

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
    transportBar_->onRenderVideoForBeat = [this](){
        renderVideoInBeatsStudio();
    };
    transportBar_->onSettingsClicked = [this]() {
        showMidi808SettingsModal();
    };
    
    // Wire up BottomDock Quick Tools buttons
    bottomDock_->onMixer = [this](){
        toggleMixerOverlay();
    };
    bottomDock_->onPianoRoll = [this](){
        setCenterView(centerView_ == CenterView::PianoRoll ? CenterView::Playlist : CenterView::PianoRoll);
    };
    bottomDock_->onChannelRack = [this](){
        auto& anim = juce::Desktop::getInstance().getAnimator();
        // "Shown" means visible AND actually opaque. A stuck animation can leave
        // the rack visible-but-transparent; treat that as hidden so a click
        // always brings it back.
        const bool shown = channelRack_->isVisible() && channelRack_->getAlpha() > 0.5f;
        if (shown)
        {
            bottomDock_->setButtonActive(2, false);
            anim.fadeOut (channelRack_.get(), 130);
        }
        else
        {
            anim.cancelAnimation (channelRack_.get(), false);
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
        const auto exactMidi = PatternsPanel::resolveExactMidiFile(pattern);
        if (exactMidi.existsAsFile())
        {
            juce::StringArray missing;
            if (pattern.useFullPresetRows)
                channelRack_->applyDrumPreset(pattern.presetId, &missing);
            if (channelRack_->applyExactDrumMidiFile(exactMidi, pattern.title, pattern.presetId, &missing))
            {
                if (pattern.bpm > 0)
                    transportBar_->setBPM((double)pattern.bpm);
                if (aiPanel_)
                    aiPanel_->addAssistantMessage("Loaded exact drum MIDI: " + pattern.title + ".");
                if (!channelRack_->isVisible())
                    channelRack_->setVisible(true);
                channelRack_->toFront(false);
                return;
            }
        }

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
    auto applyPatternDefinitionStepsOnly = [this](const PatternsPanel::PatternDefinition& pattern) {
        ChannelRack::PatternGrid rackGrid {};
        for (size_t r = 0; r < rackGrid.size(); ++r)
            for (size_t s = 0; s < rackGrid[r].size(); ++s)
                rackGrid[r][s] = pattern.rows[r][s];

        channelRack_->applyStepPatternToExistingRows(rackGrid);
        if (pattern.bpm > 0)
            transportBar_->setBPM((double)pattern.bpm);

        if (!channelRack_->isVisible())
            channelRack_->setVisible(true);
        channelRack_->toFront(false);
    };
    aiPanel_->onPatternVariant = [this, applyPatternDefinition](const PatternsPanel::PatternDefinition& pattern) {
        applyPatternDefinition(pattern);
    };
    aiPanel_->onCyclePatternOnly = [this, applyPatternDefinitionStepsOnly](const PatternsPanel::PatternDefinition& pattern) {
        applyPatternDefinitionStepsOnly(pattern);
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
    aiPanel_->onAudioGenerated = [this](juce::File f) {
        if (channelRack_)
            channelRack_->addSampleChannel(f, "Voice - " + f.getFileNameWithoutExtension());
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
    browser_->onOpenMarketplace = [this]() { showMarketplacePanel(); };
    
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
        if (transportBar_->isPlaying()
            && transportBar_->getPlaybackMode() == TransportBar::PlaybackMode::Playlist)
            playlist_->requestSamplePlaybackResync(true);
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

    pinterestBtn_.setLookAndFeel(&titleBarBadgeLaf);
    pinterestBtn_.onClick = [this]() { showPinterestMenu(); };
    addAndMakeVisible(pinterestBtn_);

    midi808Btn_.setLookAndFeel(&titleBarBadgeLaf);
    midi808Btn_.onClick = [this]() { showMidi808SettingsModal(); };
    addAndMakeVisible(midi808Btn_);

    consistencyTitleBtn_.setLookAndFeel(&titleBarBadgeLaf);
    consistencyTitleBtn_.onClick = [this]() {
        setCenterView(CenterView::Consistency);
    };
    addAndMakeVisible(consistencyTitleBtn_);

    distrokidBtn_.setLookAndFeel(&titleBarBadgeLaf);
    distrokidBtn_.onClick = []() {
        juce::URL("https://distrokid.com/new/").launchInDefaultBrowser();
    };
    addAndMakeVisible(distrokidBtn_);

    changelogBtn_.setLookAndFeel(&titleBarBadgeLaf);
    changelogBtn_.onClick = [this]() { showChangelogModal(); };
    addAndMakeVisible(changelogBtn_);

    backupBtn_.setLookAndFeel(&titleBarBadgeLaf);
    backupBtn_.onClick = [this]() { backupCurrentProject(); };
    addAndMakeVisible(backupBtn_);

    updateBtn_.setLookAndFeel(&titleBarBadgeLaf);
    updateBtn_.onClick = [this]()
    {
        if (updateAvailable_)
            openUpdateDownload();
        else
            checkForUpdates(true);
    };
    addAndMakeVisible(updateBtn_);


    albumBtn_.setLookAndFeel(&titleBarBadgeLaf);
    albumBtn_.onClick = [this]() { showAlbumPicker(); };
    addAndMakeVisible(albumBtn_);
    refreshAlbumButton();

    // Restore persisted Chordify enable state.
    {
        auto f = stratumDocumentsRoot().getChildFile("chordify_enabled.txt");
        if (f.existsAsFile())
            chordifyEnabled_ = !f.loadFileAsString().trim().equalsIgnoreCase("off");
    }
    chordifyBtn_.setLookAndFeel(&titleBarBadgeLaf);
    chordifyBtn_.onClick = [this]() { toggleChordifyEnabled(); };
    addAndMakeVisible(chordifyBtn_);
    refreshChordifyButton();

    dockBtn_.setLookAndFeel(&titleBarBadgeLaf);
    dockBtn_.onClick = [this]() {
        bottomDockHidden_ = !bottomDockHidden_;
        if (bottomDock_) bottomDock_->setVisible(!bottomDockHidden_);
        if (browser_) browser_->setPluginPanelHidden(bottomDockHidden_);
        resized();
        repaint();
    };
    addAndMakeVisible(dockBtn_);

    auto themeText = themeStateFile().loadFileAsString().trim().toLowerCase();
    if (themeText == "blue") applyThemePreset(Theme::Preset::Blue, false);
    else if (themeText == "purple") applyThemePreset(Theme::Preset::Purple, false);
    else if (themeText == "emerald") applyThemePreset(Theme::Preset::Emerald, false);
    else if (themeText == "crimson") applyThemePreset(Theme::Preset::Crimson, false);
    else if (themeText == "gold") applyThemePreset(Theme::Preset::Gold, false);
    else if (themeText == "aero") applyThemePreset(Theme::Preset::FrutigerAero, false);
    else if (themeText == "glass") applyThemePreset(Theme::Preset::LiquidGlass, false);
    else applyThemePreset(Theme::Preset::Default, false);

    juce::Component::SafePointer<MainComponent> updateSafe(this);
    juce::Timer::callAfterDelay(2500, [updateSafe]()
    {
        if (updateSafe != nullptr)
            updateSafe->checkForUpdates(false);
    });
    
    setSize(1280, 800);

    // Keyboard focus for spacebar shortcut
    setWantsKeyboardFocus(true);

    // Click-outside-to-dismiss for the floating ChannelRack overlay.
    // Tab still toggles it back open via the existing keyPressed handler.
    juce::Desktop::getInstance().addGlobalMouseListener(this);

    // Capture the initial state and start the undo-polling timer.
    lastSnapshotJson_ = captureSnapshotJson();
    startTimer(400);
    mcLog("constructor done");
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::tabKey)
    {
        if (bottomDock_ && bottomDock_->onChannelRack)
            bottomDock_->onChannelRack();
        return true;
    }

    // ` (backtick) → close every floating overlay panel at once, returning
    // focus to whatever main view (Mixer / Piano Roll / Playlist) is below.
    if (key.getTextCharacter() == '`' || key.getKeyCode() == 0xC0)
    {
        bool any = false;
        auto hide = [&any](juce::Component* c) {
            if (c && c->isVisible()) { c->setVisible(false); any = true; }
        };
        hide(aiPanel_.get());
        hide(patternsPanel_.get());
        // Don't collapse the video panel if the user has embedded it into the
        // bottom session dock — that's its "docked" home, not an overlay.
        if (videoPanel_ && !videoPanel_->isEmbeddedInSession())
            hide(videoPanel_.get());
        hide(channelRack_.get());
        if (changelogOverlay_)
        {
            changelogOverlay_.reset();
            any = true;
        }
        if (any) repaint();
        return true;
    }

    if (key == juce::KeyPress::spaceKey)
    {
        if (transportBar_) transportBar_->togglePlay();
        return true;
    }

    // '1' → toggle the RACK button (switch playback source: Rack ↔ Playlist).
    if (key.getTextCharacter() == '1' && !key.getModifiers().isAnyModifierKeyDown())
    {
        if (transportBar_)
        {
            const auto newMode = (transportBar_->getPlaybackMode() == TransportBar::PlaybackMode::Rack)
                                     ? TransportBar::PlaybackMode::Playlist
                                     : TransportBar::PlaybackMode::Rack;
            transportBar_->setPlaybackMode(newMode);
        }
        return true;
    }

    using KP = juce::KeyPress;
    using MK = juce::ModifierKeys;

    // Alt+Enter → toggle true full-screen (hides the Windows taskbar), FL-style.
    if (key == KP(juce::KeyPress::returnKey, MK::altModifier, 0))
    {
        toggleFullScreen();
        return true;
    }

    // Ctrl+Alt+Z → redo  (check BEFORE plain Ctrl+Z)
    if (key == KP('c', MK::ctrlModifier, 0)
        && channelRack_ != nullptr
        && channelRack_->isVisible())
    {
        if (channelRack_->copySessionPatternToClipboard())
        {
            if (bottomDock_)
                bottomDock_->setSessionStatus("Copied Channel Rack session");
            return true;
        }
    }

    if (key == KP('v', MK::ctrlModifier, 0)
        && channelRack_ != nullptr)
    {
        if (channelRack_->pasteSessionPatternFromClipboard())
        {
            channelRack_->setVisible(true);
            channelRack_->toFront(false);
            if (bottomDock_)
                bottomDock_->setSessionStatus("Pasted Channel Rack session");
            return true;
        }
    }

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
    juce::Desktop::getInstance().removeGlobalMouseListener(this);
    ConsistencyPanel::recordSessionEnd();
    themeBtn_.setLookAndFeel(nullptr);
    midi808Btn_.setLookAndFeel(nullptr);
    consistencyTitleBtn_.setLookAndFeel(nullptr);
    distrokidBtn_.setLookAndFeel(nullptr);
    changelogBtn_.setLookAndFeel(nullptr);
    backupBtn_.setLookAndFeel(nullptr);
    updateBtn_.setLookAndFeel(nullptr);
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
        case Theme::Preset::FrutigerAero: label = "AERO"; break;
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

void MainComponent::refreshChordifyButton()
{
    chordifyBtn_.setButtonText(chordifyEnabled_ ? juce::String("CHORDIFY: ON")
                                                : juce::String("CHORDIFY: OFF"));
    resized();
    repaint();
}

void MainComponent::toggleChordifyEnabled()
{
    chordifyEnabled_ = !chordifyEnabled_;
    stratumDocumentsRoot().createDirectory();
    stratumDocumentsRoot().getChildFile("chordify_enabled.txt")
        .replaceWithText(chordifyEnabled_ ? "on" : "off");
    refreshChordifyButton();
    if (bottomDock_)
        bottomDock_->setSessionStatus(chordifyEnabled_
            ? juce::String("Chordify auto-analyze ON")
            : juce::String("Chordify auto-analyze OFF - loops drop without analysis"));
}

void MainComponent::refreshAlbumButton()
{
    const auto album = loadCurrentAlbum();
    const juce::String label = album.isEmpty() ? juce::String("ALBUM")
                                               : juce::String("ALBUM: " + album.toUpperCase());
    albumBtn_.setButtonText(label);
    resized();   // re-layout so badge width fits
    repaint();
}

void MainComponent::showAlbumPicker()
{
    auto overlay = std::make_unique<AlbumPickerOverlay>(stratumAlbumsRoot(), loadCurrentAlbum());
    overlay->setBounds(getLocalBounds());

    overlay->onClose = [this]()
    {
        albumPickerOverlay_.reset();
        repaint();
    };
    overlay->onPick = [this](const juce::String& name)
    {
        if (name == "__create__")
        {
            auto* aw = new juce::AlertWindow("New album",
                                             "Album name:", juce::AlertWindow::NoIcon);
            aw->addTextEditor("name", "", "");
            aw->addButton("Create", 1, juce::KeyPress(juce::KeyPress::returnKey));
            aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            aw->enterModalState(true,
                juce::ModalCallbackFunction::create([this, aw](int result)
                {
                    if (result == 1)
                    {
                        auto raw = aw->getTextEditorContents("name").trim();
                        auto safe = raw.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_");
                        if (safe.isNotEmpty())
                        {
                            stratumAlbumsRoot().getChildFile(safe).createDirectory();
                            saveCurrentAlbum(safe);
                            refreshAlbumButton();
                            if (albumPickerOverlay_) albumPickerOverlay_->rebuildEntries();
                        }
                    }
                    delete aw;
                }), true);
            return;
        }
        saveCurrentAlbum(name);
        refreshAlbumButton();
        albumPickerOverlay_.reset();
        repaint();
    };
    overlay->onRequestCoverChange = [this](const juce::String& albumName)
    {
        auto* fc = new juce::FileChooser("Pick cover image",
                                         juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
                                         "*.png;*.jpg;*.jpeg");
        fc->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, fc, albumName](const juce::FileChooser& chooser)
            {
                auto src = chooser.getResult();
                if (src.existsAsFile())
                {
                    auto dest = stratumAlbumsRoot().getChildFile(albumName)
                                                   .getChildFile("cover" + src.getFileExtension().toLowerCase());
                    // Remove other cover.* variants so we don't have stale ones.
                    for (auto* ext : { "cover.png", "cover.jpg", "cover.jpeg" })
                        stratumAlbumsRoot().getChildFile(albumName).getChildFile(ext).deleteFile();
                    src.copyFileTo(dest);
                    if (albumPickerOverlay_) albumPickerOverlay_->rebuildEntries();
                }
                delete fc;
            });
    };

    addAndMakeVisible(*overlay);
    overlay->toFront(true);
    overlay->grabKeyboardFocus();
    albumPickerOverlay_ = std::move(overlay);
    return;
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
        else if (preset == Theme::Preset::FrutigerAero) id = "aero";
        else if (preset == Theme::Preset::LiquidGlass) id = "glass";
        themeStateFile().replaceWithText(id);
    }

    repaint();
    for (auto* child : getChildren())
        if (child) child->repaint();
}

static void launchPinterestDownload(juce::TextButton& btn,
                                     std::unique_ptr<std::thread>& threadSlot,
                                     const juce::String& pyScript,
                                     const juce::String& query,
                                     const juce::String& outFolder,
                                     int count = 10)
{
    btn.setButtonText("DOWNLOADING...");
    btn.setEnabled(false);
    AgentRegistry::get().setJobRunning(AgentIds::pinterest, "Downloading images from Pinterest...");

    if (threadSlot && threadSlot->joinable())
        threadSlot->detach();

    threadSlot = std::make_unique<std::thread>([&btn, pyScript, query, outFolder, count]()
    {
        // Build command: python "script.py" "query" --count N --out "folder"
        juce::String args = "\"" + pyScript + "\" \"" + query + "\""
                          + " --count " + juce::String(count)
                          + " --out \"" + outFolder + "\"";

        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(sei);
        sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = L"python";
        auto wargs = args.toWideCharPointer();
        sei.lpParameters = wargs;
        sei.nShow  = SW_SHOWNORMAL;
        ShellExecuteExW(&sei);

        if (sei.hProcess)
        {
            WaitForSingleObject(sei.hProcess, INFINITE);
            CloseHandle(sei.hProcess);
        }

        juce::MessageManager::callAsync([&btn, outFolder]()
        {
            btn.setButtonText("PINTEREST");
            btn.setEnabled(true);
            AgentRegistry::get().setJobDone(AgentIds::pinterest, "Images saved to " + outFolder);
            juce::File folder(outFolder);
            folder.createDirectory();
            auto opts = juce::MessageBoxOptions()
                .withTitle("Pinterest Download Complete")
                .withMessage("Images saved to:\n" + outFolder)
                .withButton("Open Folder")
                .withButton("OK");
            juce::AlertWindow::showAsync(opts, [folder](int r) {
                if (r == 1) folder.revealToUser();
            });
        });
    });
}

void MainComponent::showPinterestMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Download images (home feed)");
    menu.addItem(2, "Search & download...");
    menu.addSeparator();
    menu.addItem(3, "Login / setup session");
    menu.addItem(4, "Open output folder");

    auto scriptDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                         .getParentDirectory();
    auto scriptFile = scriptDir;
    for (int i = 0; i < 6; ++i)
    {
        if (scriptFile.getChildFile("pinterest_downloader.py").existsAsFile())
            break;
        scriptFile = scriptFile.getParentDirectory();
    }
    juce::String pyScript = scriptFile.getChildFile("pinterest_downloader.py").getFullPathName();
    juce::String outFolder = "D:\\folderforpinterest";

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&pinterestBtn_),
        [this, pyScript, outFolder](int result)
        {
            if (result == 1)
            {
                launchPinterestDownload(pinterestBtn_, pinterestThread_,
                                        pyScript, "aesthetic", outFolder);
            }
            else if (result == 2)
            {
                auto* editor = new juce::AlertWindow("Pinterest Search",
                    "Enter a search query (e.g. 'boom bap', 'dark city'):",
                    juce::MessageBoxIconType::NoIcon);
                editor->addTextEditor("query", "", "Search query:");
                editor->addButton("Download", 1);
                editor->addButton("Cancel", 0);
                editor->enterModalState(true, juce::ModalCallbackFunction::create(
                    [this, editor, pyScript, outFolder](int r)
                    {
                        if (r == 1)
                        {
                            juce::String q = editor->getTextEditorContents("query").trim();
                            if (q.isNotEmpty())
                                launchPinterestDownload(pinterestBtn_, pinterestThread_,
                                                        pyScript, q, outFolder);
                        }
                        delete editor;
                    }), true);
            }
            else if (result == 3)
            {
                // Login — just open a visible cmd window
                SHELLEXECUTEINFOW sei = {};
                sei.cbSize = sizeof(sei);
                sei.lpVerb = L"open";
                sei.lpFile = L"cmd.exe";
                juce::String loginArgs = "/k python \"" + pyScript + "\" --login";
                sei.lpParameters = loginArgs.toWideCharPointer();
                sei.nShow = SW_SHOWNORMAL;
                ShellExecuteExW(&sei);
            }
            else if (result == 4)
            {
                juce::File(outFolder).createDirectory();
                juce::File(outFolder).revealToUser();
            }
        });
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
    menu.addSeparator();
    menu.addItem(7, "Frutiger Aero", true, Theme::currentPreset == Theme::Preset::FrutigerAero);

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
            else if (result == 7) applyThemePreset(Theme::Preset::FrutigerAero, true);
        });
}

void MainComponent::paint(juce::Graphics& g)
{
    if (Theme::liquidGlassMode)
        Theme::drawAeroGloss(g, getLocalBounds().toFloat(), 1.0f);  // iOS wallpaper mesh fills whole window
    else if (Theme::aeroMode)
        Theme::drawAeroPanel(g, getLocalBounds().toFloat());
    else
        g.fillAll(juce::Colour(0xff09090b));

    int w = getWidth();
    constexpr int TB_H = 28;
    
    // ── Top Title Bar — engineered control panel chassis ──────
    auto titleBar = juce::Rectangle<float>(0, 0, (float)w, (float)TB_H);

    if (Theme::liquidGlassMode)
    {
        // iOS 26 title bar: floating frosted glass strip.
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.fillRect(titleBar);
        // Bright top specular.
        juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.75f), 0.0f, 0.0f,
                                   juce::Colours::white.withAlpha(0.0f), 0.0f, (float)TB_H, false);
        g.setGradientFill(sheen);
        g.fillRect(titleBar);
        // Hairline divider at bottom.
        g.setColour(juce::Colours::black.withAlpha(0.10f));
        g.drawHorizontalLine(TB_H - 1, 0.0f, (float)w);
    }
    else if (Theme::aeroMode)
    {
        // Frutiger Aero: glossy sky-blue→white→green title strip with bubbles.
        Theme::drawAeroGloss(g, titleBar, 0.9f);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawHorizontalLine(0, 0.0f, (float)w);
        g.setColour(juce::Colour(0xff0e7490));
        g.drawHorizontalLine(TB_H - 1, 0.0f, (float)w);
    }
    else
    {
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
    }

    // ── STRATUM wordmark (engraved) ──
    int x = 14;
    const juce::Colour wordmarkShadow = Theme::aeroMode ? juce::Colours::white.withAlpha(0.7f)
                                                        : juce::Colours::black.withAlpha(0.85f);
    const juce::Colour wordmarkMain   = Theme::aeroMode ? juce::Colour(0xff0c4a6e)
                                                        : juce::Colour(0xffe4e4e7);
    // Drop shadow
    g.setColour(wordmarkShadow);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f).withStyle("Bold"));
    g.drawText("STRATUM", x, 1, 70, TB_H, juce::Justification::centredLeft);
    // Main
    g.setColour(wordmarkMain);
    g.drawText("STRATUM", x, 0, 70, TB_H, juce::Justification::centredLeft);
    
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
    {
        int themeBtnRight = 120 + titleBarBadgeWidthForText(themeBtn_.getButtonText()) + 6;
        const int pinW = titleBarBadgeWidthForText("PINTEREST");
        pinterestBtn_.setBounds(themeBtnRight, 7, pinW, 14);
        const int midi808W = titleBarBadgeWidthForText("808 MIDI");
        midi808Btn_.setBounds(themeBtnRight + pinW + 6, 7, midi808W, 14);
        const int consW = titleBarBadgeWidthForText("CONSISTENCY");
        const int consX = themeBtnRight + pinW + 6 + midi808W + 6;
        consistencyTitleBtn_.setBounds(consX, 7, consW, 14);
        const int distroW = titleBarBadgeWidthForText("DISTROKID");
        const int distroX = consX + consW + 6;
        distrokidBtn_.setBounds(distroX, 7, distroW, 14);
        const int changelogW = titleBarBadgeWidthForText("CHANGELOG");
        changelogBtn_.setBounds(distroX + distroW + 6, 7, changelogW, 14);
        const int backupW = titleBarBadgeWidthForText(backupBtn_.getButtonText());
        const int backupX = distroX + distroW + 6 + changelogW + 6;
        backupBtn_.setBounds(backupX, 7, backupW, 14);
        const int updateW = titleBarBadgeWidthForText(updateBtn_.getButtonText());
        const int updateX = backupX + backupW + 6;
        updateBtn_.setBounds(updateX, 7, updateW, 14);
        const int dockW = titleBarBadgeWidthForText("DOCK");
        const int dockX = updateX + updateW + 6;
        dockBtn_.setBounds(dockX, 7, dockW, 14);
        const int albumW = titleBarBadgeWidthForText(albumBtn_.getButtonText());
        const int albumX = dockX + dockW + 6;
        albumBtn_.setBounds(albumX, 7, albumW, 14);
        const int chordW = titleBarBadgeWidthForText(chordifyBtn_.getButtonText());
        chordifyBtn_.setBounds(albumX + albumW + 6, 7, chordW, 14);
    }
    
    // Transport bar (60px)
    transportBar_->setBounds(area.removeFromTop(60));
    
    // Browser on left - responsive width (18% minimum 220px)
    int browserW = juce::jmax(220, (int)(w * 0.18));
    browser_->setBounds(area.removeFromLeft(browserW));
    
    // Bottom dock - responsive height (15% minimum 130px)
    if (!bottomDockHidden_)
    {
        int dockH = juce::jmax(130, (int)(h * 0.15));
        bottomDock_->setBounds(area.removeFromBottom(dockH));
    }
    
    // All center views share the same bounds; visibility toggles.
    // The mixer is the exception: a floating, resizable window (FL-style)
    // that overlays the arrangement.
    lastCentreArea_ = area;
    layoutMixerWindow(area);
    playlist_->setBounds(area);
    pianoRoll_->setBounds(area);
    if (consistencyPanel_) consistencyPanel_->setBounds(area);
    if (orgChartPanel_) orgChartPanel_->setBounds(area);
    if (youTubePanel_) youTubePanel_->setBounds(area);
    
    // Channel rack as floating window centered in main area - responsive
    int crW = juce::jmin(area.getWidth() - 40, (int)(w * 0.55));
    // Compact: fit height to the channel slots + "+" button, never stretch past them.
    int crH = juce::jmin(area.getHeight() - 40, channelRack_->getIdealHeight());
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
    if (changelogOverlay_)
        changelogOverlay_->setBounds(getLocalBounds());
    if (marketplaceOverlay_)
        marketplaceOverlay_->setBounds(getLocalBounds());
    if (exportToast_)
    {
        const int tw = exportToast_->getWidth(), th = exportToast_->getHeight();
        exportToast_->setBounds(getWidth() - tw - 20, getHeight() - th - 20, tw, th);
    }
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
    // Click-outside dismiss for ChannelRack overlay. We're registered as a
    // global mouse listener so this fires for ALL clicks in the app, not just
    // ones that land on MainComponent itself. Tab brings the rack back.
    if (channelRack_ && channelRack_->isVisible()
        && !(e.mods.isRightButtonDown() || e.mods.isPopupMenu()))
    {
        auto* src = e.eventComponent;
        bool insideRack = false;
        if (src != nullptr)
        {
            if (src == channelRack_.get() || channelRack_->isParentOf(src))
                insideRack = true;
            // Floating plugin / effect editor windows opened FROM the rack —
            // treat as part of the rack for dismiss purposes.
            for (auto& kv : pluginWindows_)
                if (kv.second && (src == kv.second.get() || kv.second->isParentOf(src)))
                    insideRack = true;
            for (auto& kv : nativeEffectWindows_)
                if (kv.second && (src == kv.second.get() || kv.second->isParentOf(src)))
                    insideRack = true;
        }
        // Don't dismiss when clicking the bottom dock — its CHANNEL RACK button
        // toggles the rack itself. If we also fade out here, the two fade-outs
        // race and leave the rack stuck visible-but-transparent (isVisible()==true,
        // alpha==0), so the toggle can never bring it back.
        bool inBottomDock = false;
        if (src != nullptr && bottomDock_
            && (src == bottomDock_.get() || bottomDock_->isParentOf(src)))
            inBottomDock = true;

        // Don't dismiss when clicking the title bar (top 28px) — those badges
        // are utility (DOCK, ALBUM, CHORDIFY, …) and shouldn't kill the rack.
        const auto localPt = getLocalPoint(nullptr, e.getScreenPosition());
        const bool inTitleBar = (localPt.getY() < 28);

        if (!insideRack && !inTitleBar && !inBottomDock)
        {
            if (bottomDock_) bottomDock_->setButtonActive(2, false);
            juce::Desktop::getInstance().getAnimator().fadeOut(channelRack_.get(), 130);
        }
    }

    // Title bar drag (top 28px, but not over window controls). Only fires for
    // direct hits on MainComponent — global-listener relays have a different
    // eventComponent so they skip this block.
    if (e.eventComponent == this
        && e.y < 28 && e.x < getWidth() - 100 && !themeBtn_.getBounds().contains(e.x, e.y))
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

void MainComponent::toggleFullScreen()
{
    auto* topWindow = getTopLevelComponent();
    if (!topWindow) return;

   #ifdef _WIN32
    if (auto* peer = topWindow->getPeer())
    {
        auto hwnd = (HWND) peer->getNativeHandle();
        if (!isFullScreen_)
        {
            // Save where we were.
            preFullScreenBounds_ = isMaximized_ ? preMaxBounds_ : topWindow->getBounds();

            HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi; mi.cbSize = sizeof(mi);
            if (!GetMonitorInfo(mon, &mi)) return;

            const RECT& r = mi.rcMonitor;

            // HWND_TOPMOST is required — the Windows taskbar is itself TOPMOST,
            // so HWND_TOP leaves the taskbar visible. HWND_TOPMOST goes above it.
            // SWP_SHOWWINDOW ensures the window is brought forward.
            SetWindowPos(hwnd, HWND_TOPMOST,
                         r.left, r.top,
                         r.right  - r.left,
                         r.bottom - r.top,
                         SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            isFullScreen_ = true;
            isMaximized_  = true;
        }
        else
        {
            // Drop topmost first, then restore position.
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
            ShowWindow(hwnd, SW_RESTORE);
            topWindow->setBounds(preFullScreenBounds_);
            isFullScreen_ = false;
            isMaximized_  = false;
        }
        topWindow->resized();
        topWindow->repaint();
        return;
    }
   #endif

    // Non-Windows fallback: full display totalArea (covers dock/taskbar).
    if (!isFullScreen_)
    {
        preFullScreenBounds_ = topWindow->getBounds();
        auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* display = displays.getDisplayForPoint(topWindow->getBounds().getCentre());
        if (display == nullptr) display = displays.getPrimaryDisplay();
        if (display != nullptr) topWindow->setBounds(display->totalArea);
        isFullScreen_ = true;
        isMaximized_  = true;
    }
    else
    {
        topWindow->setBounds(preFullScreenBounds_);
        isFullScreen_ = false;
        isMaximized_  = false;
    }
}

void MainComponent::runPerfStress(int mode)
{
    if (!transportBar_ || !channelRack_ || !playlist_)
        return;

    if (mode == 0)
    {
        transportBar_->stop();
        return;
    }

    // Build a busy beat: full trap drum kit + a busy multi-clip arrangement.
    channelRack_->applyDrumPreset("trap");
    if (channelRack_->onChannelsChanged) channelRack_->onChannelsChanged();
    playlist_->autoArrangePublic("trap");

    // Switch to playlist playback (drums-as-pattern-clips + any sample loops).
    transportBar_->setPlaybackMode(TransportBar::PlaybackMode::Playlist);
    applyPlaybackMode(TransportBar::PlaybackMode::Playlist);
    setCenterView(CenterView::Playlist);
    channelRack_->setVisible(true);   // rack overlay open (paints during play)

    if (!transportBar_->isPlaying())
        transportBar_->togglePlay();
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

bool MainComponent::isMixerOverlayVisible() const
{
    return mixer_ && mixer_->isVisible() && mixer_->getAlpha() > 0.5f;
}

void MainComponent::setMixerOverlayVisible(bool show)
{
    if (!mixer_) return;
    auto& anim = juce::Desktop::getInstance().getAnimator();

    if (show)
    {
        anim.cancelAnimation(mixer_.get(), false);
        layoutMixerWindow(lastCentreArea_);
        if (!mixer_->isVisible())
        {
            mixer_->setAlpha(0.0f);
            mixer_->setVisible(true);
        }
        anim.fadeIn(mixer_.get(), 160);
        mixer_->toFront(false);
        if (bottomDock_)   bottomDock_->setButtonActive(0, true);
        if (transportBar_) transportBar_->setSelectedView(1);
    }
    else
    {
        anim.cancelAnimation(mixer_.get(), false);
        anim.fadeOut(mixer_.get(), 120);
        mixer_->setVisible(false);
        if (bottomDock_)   bottomDock_->setButtonActive(0, false);
        if (transportBar_)
            transportBar_->setSelectedView(centerView_ == CenterView::PianoRoll   ? 0
                                         : centerView_ == CenterView::Consistency ? 3
                                         : centerView_ == CenterView::OrgChart    ? 4
                                         : centerView_ == CenterView::YouTube     ? 5 : 2);
    }
}

void MainComponent::toggleMixerOverlay()
{
    setMixerOverlayVisible(!isMixerOverlayVisible());
}

void MainComponent::layoutMixerWindow(juce::Rectangle<int> area)
{
    if (!mixer_) return;

    if (mixerMaximized_)
    {
        mixer_->setBounds(area);
        return;
    }

    juce::Rectangle<int> mb = mixerFloatBounds_;
    if (mb.isEmpty())
    {
        // First time: a comfortable centred window (~74% of the centre area).
        const int mw = juce::jlimit(560, juce::jmax(560, area.getWidth() - 20),  (int)(area.getWidth()  * 0.74f));
        const int mh = juce::jlimit(360, juce::jmax(360, area.getHeight() - 20), (int)(area.getHeight() * 0.74f));
        mb = juce::Rectangle<int>(area.getCentreX() - mw / 2, area.getCentreY() - mh / 2, mw, mh);
    }

    // Keep it sized no larger than the centre area and fully inside it.
    mb.setSize(juce::jmin(mb.getWidth(),  area.getWidth()),
               juce::jmin(mb.getHeight(), area.getHeight()));
    mb.setX(juce::jlimit(area.getX(), area.getRight()  - mb.getWidth(),  mb.getX()));
    mb.setY(juce::jlimit(area.getY(), area.getBottom() - mb.getHeight(), mb.getY()));

    mixerFloatBounds_ = mb;
    mixer_->setBounds(mb);
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

    // The mixer is a free-floating overlay — it is NOT part of the exclusive
    // centre-view set, so it never appears in these hide-lists and stays on top.
    switch (v)
    {
        case CenterView::Playlist:
            crossfade (playlist_.get(),         { pianoRoll_.get(), consistencyPanel_.get(), orgChartPanel_.get(), youTubePanel_.get() }); break;
        case CenterView::PianoRoll:
            crossfade (pianoRoll_.get(),         { playlist_.get(), consistencyPanel_.get(), orgChartPanel_.get(), youTubePanel_.get() }); break;
        case CenterView::Mixer:
            // Mixer is no longer an exclusive view; treat as Playlist behind the overlay.
            crossfade (playlist_.get(),          { pianoRoll_.get(), consistencyPanel_.get(), orgChartPanel_.get(), youTubePanel_.get() }); break;
        case CenterView::Consistency:
            crossfade (consistencyPanel_.get(),  { playlist_.get(), pianoRoll_.get(), orgChartPanel_.get(), youTubePanel_.get() }); break;
        case CenterView::OrgChart:
            crossfade (orgChartPanel_.get(),     { playlist_.get(), pianoRoll_.get(), consistencyPanel_.get(), youTubePanel_.get() }); break;
        case CenterView::YouTube:
            crossfade (youTubePanel_.get(),      { playlist_.get(), pianoRoll_.get(), consistencyPanel_.get(), orgChartPanel_.get() }); break;
    }

    // Keep the floating mixer above whatever view we just brought up.
    if (isMixerOverlayVisible())
        mixer_->toFront(false);

    if (bottomDock_)
        bottomDock_->setButtonActive(1, v == CenterView::PianoRoll);

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
        transportBar_->setSelectedView (isMixerOverlayVisible()         ? 1
                                       : v == CenterView::PianoRoll      ? 0
                                       : v == CenterView::Consistency    ? 3
                                       : v == CenterView::OrgChart       ? 4
                                       : v == CenterView::YouTube        ? 5 : 2);
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
    // 128 steps = 8 BAR (default channel-rack pattern length)
    playlistObj->setProperty("patternDefaultSteps", 128);
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

void MainComponent::openRenderedVideoWindow(const juce::File& videoFile)
{
    if (!videoPanel_ || !videoFile.existsAsFile())
        return;

    if (videoPanel_->isEmbeddedInSession())
    {
        videoPanel_->unembedPlayerFromSession();
        if (bottomDock_)
            bottomDock_->setSessionVideoMode(false);
    }

    if (!videoPanel_->loadVideoFile(videoFile, true))
        return;

    const int pw = juce::jmin(980, juce::jmax(420, getWidth() - 120));
    const int ph = juce::jmin(620, juce::jmax(300, getHeight() - 130));
    videoPanel_->setBounds((getWidth() - pw) / 2, (getHeight() - ph) / 2, pw, ph);
    videoPanel_->resized();
    videoPanel_->setAlpha(0.0f);
    videoPanel_->setVisible(true);
    videoPanel_->toFront(true);
    if (bottomDock_)
        bottomDock_->setButtonActive(4, true);

    juce::Desktop::getInstance().getAnimator().fadeIn(videoPanel_.get(), 160);
}

void MainComponent::saveProjectAndRenderWavCopy(const juce::String& cleanName, const juce::File& renderWavFile)
{
    auto root = stratumSaveRoot();
    root.createDirectory();

    auto safeName = cleanName.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_").trim();
    if (safeName.isEmpty())
        safeName = "Stratum Beat";

    const auto projectFile = root.getChildFile(safeName).withFileExtension(kProjectExt);
    const auto wavFile = root.getChildFile(safeName).withFileExtension(".wav");

    const bool projectSaved = saveProject(projectFile);
    bool wavSaved = false;
    if (renderWavFile.existsAsFile())
    {
        if (wavFile.existsAsFile())
            wavFile.deleteFile();
        wavSaved = renderWavFile.copyFileTo(wavFile);
    }

    if (bottomDock_)
    {
        if (projectSaved && wavSaved)
            bottomDock_->setSessionStatus("Saved project + WAV while rendering video");
        else if (projectSaved)
            bottomDock_->setSessionStatus("Saved project while rendering video");
        else
            bottomDock_->setSessionStatus("Video render started, but auto-save failed");
    }
}

void MainComponent::runPinterestDownloadAgent()
{
    auto scriptDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto scriptFile = scriptDir;
    for (int i = 0; i < 6; ++i)
    {
        if (scriptFile.getChildFile("pinterest_downloader.py").existsAsFile())
            break;
        scriptFile = scriptFile.getParentDirectory();
    }
    const juce::String pyScript = scriptFile.getChildFile("pinterest_downloader.py").getFullPathName();
    if (! juce::File(pyScript).existsAsFile())
    {
        AgentRegistry::get().setJobFailed(AgentIds::pinterest, "pinterest_downloader.py not found");
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Pinterest Agent", "Could not find pinterest_downloader.py");
        return;
    }
    auto pinterestDir = stratumPinterestOutputDir();
    pinterestDir.createDirectory();
    launchPinterestDownload(pinterestBtn_, pinterestThread_, pyScript, "aesthetic", pinterestDir.getFullPathName());
}

void MainComponent::runOrgChartAgent(const juce::String& agentId)
{
    auto& registry = AgentRegistry::get();
    if (! registry.isAgentEnabled(agentId))
        return;

    const auto rt = registry.runtimeFor(agentId);
    if (rt.state == AgentJobState::Running)
        return;

    if (agentId == AgentIds::pinterest)
    {
        runPinterestDownloadAgent();
        return;
    }

    if (agentId == AgentIds::createVideo)
    {
        if (transportBar_)
            transportBar_->startVideoRender();
        return;
    }

    if (agentId == AgentIds::beatstars)
    {
        showCloudUploadModal();
        return;
    }

    if (agentId == AgentIds::cloudUpload)
    {
        showExportAudioModal(false);
        return;
    }

    if (agentId == AgentIds::chordifyLoop || agentId == AgentIds::bassAnalysis)
    {
        if (! playlist_)
            return;

        Playlist::BassExtractionRequest request;
        if (! playlist_->findFirstSampleBassRequest(request))
        {
            const juce::String msg = "Add a sample loop to the playlist first.";
            registry.setJobFailed(agentId, msg);
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                "Agent needs a loop", msg);
            return;
        }

        request.useChordifyAutomation = (agentId == AgentIds::chordifyLoop);
        handleBassExtractionRequest(request,
            [this](std::vector<Playlist::ExtractedBassNote> notes)
            {
                if (! channelRack_ || notes.empty())
                    return;

                std::vector<ChannelRack::Channel::Note> rackNotes;
                rackNotes.reserve(notes.size());
                for (const auto& n : notes)
                    rackNotes.push_back({ n.pitch, n.startStep, n.lengthSteps, n.velocity });

                const int channelIndex = channelRack_->applyWaitFor808Midi(rackNotes);
                if (channelIndex >= 0 && channelRack_->onChannelClicked)
                    channelRack_->onChannelClicked(channelIndex);
            },
            true);
        return;
    }
}

void MainComponent::runAllEnabledOrgChartAgents()
{
    for (const auto& agentId : AgentRegistry::get().enabledAgentIds())
        runOrgChartAgent(agentId);
}

void MainComponent::handleChordifyRestart()
{
    bassAnalysisBusy_ = false;
    chordifyAutomationEngine_.cancelPending();
    ChordifyAutomationEngine::forceRestart();
    ChordifyAutomationEngine::refreshCdpStatus();

    juce::String status = "Chordify restart: cleared lock";
    if (ChordifyAutomationEngine::launchChrome())
        status = "Chordify restart: reopening Chrome...";
    else
        status = "Chordify restart: run launch-chordify-chrome.ps1 manually";

    if (bottomDock_)
        bottomDock_->setSessionStatus(status);

    juce::Timer::callAfterDelay(2500, [safe = juce::Component::SafePointer<MainComponent>(this)]()
    {
        if (safe == nullptr || safe->bottomDock_ == nullptr)
            return;

        if (ChordifyAutomationEngine::isFullyReady())
            safe->bottomDock_->setSessionStatus("Chordify ready - drag a loop");
        else if (ChordifyAutomationEngine::isReady())
            safe->bottomDock_->setSessionStatus("Chrome CDP not detected - wait for login");
        else
            safe->bottomDock_->setSessionStatus("Chordify not logged in - run run-chordify-login.ps1");
    });
}

void MainComponent::handleBassExtractionRequest(Playlist::BassExtractionRequest request,
                                                std::function<void(std::vector<Playlist::ExtractedBassNote>)> deliverNotes,
                                                bool autoApply)
{
    // Master kill-switch — title-bar CHORDIFY: OFF skips all Chordify analysis
    // on dropped loops so the user can work in peace when they don't need it.
    if (!chordifyEnabled_)
    {
        if (bottomDock_ && !autoApply)
            bottomDock_->setSessionStatus("Chordify is OFF - toggle title bar to re-enable");
        return;
    }

    if (bassAnalysisBusy_ || ChordifyAutomationEngine::isRunning())
    {
        // Make the rejection visible — silent autoApply skips were confusing
        // when the previous Chordify run was still in flight (or had hung).
        // Show status in bottom dock + a non-blocking alert so the user
        // actually sees what happened.
        if (bottomDock_)
            bottomDock_->setSessionStatus("Chordify busy - skipped new loop. Retry after current run.");
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "Chordify still running",
            "Stratum is still analysing the previous loop on Chordify. "
            "Wait until 'Chordify 808 ready' appears, then re-drag this loop.");
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

    if (ChordifyAutomationEngine::isReady() && request.useChordifyAutomation)
    {
        bassAnalysisBusy_ = true;
        if (bottomDock_)
            bottomDock_->setSessionStatus("Chordify: uploading + analyzing...");
        AgentRegistry::get().setJobRunning(AgentIds::chordifyLoop, "Uploading loop to Chordify...");

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
                        const auto imported = ChordifyMidiImporter::import(
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
                    juce::String failMsg = "Chordify failed";
                    if (result.error.isNotEmpty())
                        failMsg << ": " << result.error.substring(0, 80);
                    else if (notes.empty())
                        failMsg << ": no bass notes in MIDI";
                    if (safe->bottomDock_)
                        safe->bottomDock_->setSessionStatus(failMsg);
                    AgentRegistry::get().setJobFailed(AgentIds::chordifyLoop, failMsg);
                    notifyFailure(result.error.isNotEmpty()
                                      ? result.error
                                      : "Chordify MIDI empty. Try menu > MIDI Time aligned manually.");
                    return;
                }

                if (safe->bottomDock_)
                    safe->bottomDock_->setSessionStatus("Chordify 808 ready");
                AgentRegistry::get().setJobDone(AgentIds::chordifyLoop, "808 bass MIDI imported");

                if (! autoApply)
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                        "808 MIDI ready",
                        "Imported Chordify bass roots into the \"wait for 808\" channel.");
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
    AgentRegistry::get().setJobRunning(AgentIds::bassAnalysis, "Running BTC chord analysis...");

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
                AgentRegistry::get().setJobFailed(AgentIds::bassAnalysis, "BTC analysis failed");
                notifyFailure("BTC analysis failed. For Chordify-quality bass run:\n"
                              "powershell -File vst-host\\analysis\\setup-chordify.ps1\n"
                              "python vst-host\\analysis\\chordify_automation.py --login");
                return;
            }

            (*deliverNotesPtr)(notes);

            if (safe->bottomDock_)
                safe->bottomDock_->setSessionStatus("BTC bass ready");
            AgentRegistry::get().setJobDone(AgentIds::bassAnalysis, "Bass MIDI extracted via BTC");

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
    // Save goes into the active album folder if one is set, otherwise the
    // top-level docs root.
    auto root = stratumSaveRoot();
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
                showExportToast("Stems export complete", projectFile.getFullPathName(), stemsFolder.getFullPathName());
        }
        else
        {
            const auto wavFile = root.getChildFile(cleanName).withFileExtension(".wav");
            if (exportAudioToFile(wavFile))
                showExportToast("Export complete", projectFile.getFullPathName(), wavFile.getFullPathName());
        }

        projectSaveOverlay_.reset();
    };

    addAndMakeVisible(overlay.get());
    overlay->toFront(true);
    overlay->grabKeyboardFocus();
    projectSaveOverlay_ = std::move(overlay);
}

void MainComponent::showExportToast(const juce::String& title,
                                     const juce::String& projectPath,
                                     const juce::String& extraPath)
{
    exportToast_.reset();
    auto toast = std::make_unique<ExportSuccessToast>(title, projectPath, extraPath);
    // Position: bottom-right, 20px margin
    const int tw = toast->getWidth(), th = toast->getHeight();
    toast->setBounds(getWidth() - tw - 20, getHeight() - th - 20, tw, th);
    toast->onDismiss = [this]() { exportToast_.reset(); repaint(); };
    addAndMakeVisible(toast.get());
    toast->toFront(false);
    exportToast_ = std::move(toast);
}

void MainComponent::showMidi808SettingsModal()
{
    if (midi808SettingsOverlay_)
        return;

    auto overlay = std::make_unique<Midi808SettingsOverlay>();
    overlay->setBounds(getLocalBounds());
    overlay->onClose = [this]()
    {
        midi808SettingsOverlay_.reset();
        if (pianoRoll_)
            pianoRoll_->repaint();
        repaint();
    };

    addAndMakeVisible(overlay.get());
    overlay->toFront(true);
    overlay->grabKeyboardFocus();
    midi808SettingsOverlay_ = std::move(overlay);
}

void MainComponent::showChangelogModal()
{
    if (changelogOverlay_)
    {
        changelogOverlay_->toFront(true);
        changelogOverlay_->grabKeyboardFocus();
        return;
    }

    auto overlay = std::make_unique<ChangelogOverlay>();
    overlay->setBounds(getLocalBounds());
    overlay->onClose = [this]()
    {
        changelogOverlay_.reset();
        repaint();
    };

    addAndMakeVisible(overlay.get());
    overlay->toFront(true);
    overlay->grabKeyboardFocus();
    changelogOverlay_ = std::move(overlay);
}

void MainComponent::showMarketplacePanel()
{
    if (marketplaceOverlay_)
    {
        marketplaceOverlay_->toFront(true);
        marketplaceOverlay_->grabKeyboardFocus();
        return;
    }

    auto overlay = std::make_unique<MarketplacePanel>();
    overlay->setBounds(getLocalBounds());
    overlay->onClose = [this]()
    {
        marketplaceOverlay_.reset();
        repaint();
    };
    overlay->onStatus = [this](const juce::String& status)
    {
        if (bottomDock_)
            bottomDock_->setSessionStatus(status);
    };
    overlay->onLibraryChanged = [this]()
    {
        if (browser_)
            browser_->refreshCurrentLibrary();
    };

    addAndMakeVisible(overlay.get());
    overlay->toFront(true);
    overlay->grabKeyboardFocus();
    marketplaceOverlay_ = std::move(overlay);
}

void MainComponent::backupCurrentProject()
{
    auto backupDir = stratumDocumentsRoot().getChildFile("backups");
    backupDir.createDirectory();

    int nextIndex = 1;
    while (backupDir.getChildFile("backup " + juce::String(nextIndex)).withFileExtension(kProjectExt).existsAsFile())
        ++nextIndex;

    const auto backupFile = backupDir.getChildFile("backup " + juce::String(nextIndex)).withFileExtension(kProjectExt);
    const auto activeProject = currentProjectFile_;
    const bool ok = saveProject(backupFile);
    currentProjectFile_ = activeProject;

    if (bottomDock_)
    {
        bottomDock_->setSessionStatus(ok
            ? ("Saved " + backupFile.getFileName())
            : juce::String("Backup failed"));
    }

    backupBtn_.setButtonText(ok ? juce::String::fromUTF8("\xE2\x9C\x93") : juce::String("FAIL"));
    backupBtn_.repaint();

    juce::Component::SafePointer<MainComponent> safe(this);
    juce::Timer::callAfterDelay(850, [safe]()
    {
        if (safe == nullptr)
            return;
        safe->backupBtn_.setButtonText("BACKUP");
        safe->resized();
        safe->backupBtn_.repaint();
    });
}

void MainComponent::checkForUpdates(bool manual)
{
    bool expected = false;
    if (!updateCheckInFlight_.compare_exchange_strong(expected, true))
        return;

    updateBtn_.setButtonText("CHECK");
    resized();
    updateBtn_.repaint();
    if (bottomDock_ && manual)
        bottomDock_->setSessionStatus("Checking for Stratum DAW updates...");

    juce::Component::SafePointer<MainComponent> safe(this);
    std::thread([safe, manual]()
    {
        bool requestOk = false;
        juce::String latestTag;
        juce::String downloadUrl;
        juce::String releasePage = kStratumReleasesPage;

        auto url = juce::URL(kStratumLatestReleaseApi);
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(6000)
            .withExtraHeaders("Accept: application/vnd.github+json\r\nUser-Agent: Stratum-DAW");

        if (auto stream = url.createInputStream(options))
        {
            const auto body = stream->readEntireStreamAsString();
            const auto parsed = juce::JSON::parse(body);
            if (auto* obj = parsed.getDynamicObject())
            {
                requestOk = true;
                latestTag = obj->getProperty("tag_name").toString();
                const auto htmlUrl = obj->getProperty("html_url").toString();
                if (htmlUrl.isNotEmpty())
                    releasePage = htmlUrl;

                if (const auto* assets = obj->getProperty("assets").getArray())
                {
                    for (const auto& asset : *assets)
                    {
                        if (!asset.isObject())
                            continue;

                        const auto name = asset.getProperty("name", juce::String()).toString().toLowerCase();
                        const auto assetUrl = asset.getProperty("browser_download_url", juce::String()).toString();
                        if (assetUrl.isEmpty())
                            continue;

                       #if JUCE_MAC
                        if (name.endsWith(".dmg"))
                       #elif JUCE_WINDOWS
                        if (name.endsWith(".exe") || name.endsWith(".msi") || name.endsWith(".zip"))
                       #else
                        if (name.isNotEmpty())
                       #endif
                        {
                            downloadUrl = assetUrl;
                            break;
                        }
                    }
                }
            }
        }

        if (downloadUrl.isEmpty())
            downloadUrl = releasePage;

        juce::MessageManager::callAsync([safe, manual, requestOk, latestTag, downloadUrl]()
        {
            if (safe == nullptr)
                return;

            safe->updateCheckInFlight_.store(false);
            const bool newer = requestOk && isVersionNewer(latestTag, kStratumCurrentVersion);
            safe->updateAvailable_ = newer;
            safe->latestUpdateVersion_ = latestTag;
            safe->latestUpdateUrl_ = newer ? downloadUrl : juce::String();
            safe->updateBtn_.setButtonText(newer ? "UPDATE!" : "UPDATE");
            safe->resized();
            safe->updateBtn_.repaint();

            if (newer)
            {
                if (safe->bottomDock_)
                    safe->bottomDock_->setSessionStatus("Stratum DAW update available: " + latestTag);

                if (manual)
                {
                    juce::AlertWindow::showOkCancelBox(
                        juce::AlertWindow::InfoIcon,
                        "Update available",
                        "A newer Stratum DAW build is available: " + latestTag
                            + "\n\nOpen the download page now?",
                        "Download",
                        "Later",
                        safe.getComponent(),
                        juce::ModalCallbackFunction::create([url = downloadUrl](int result)
                        {
                            if (result != 0)
                                juce::URL(url).launchInDefaultBrowser();
                        }));
                }
            }
            else if (manual)
            {
                const auto message = requestOk
                    ? juce::String("You are already on the latest Stratum DAW version.")
                    : juce::String("Could not check for updates. Please check GitHub Releases.");
                if (safe->bottomDock_)
                    safe->bottomDock_->setSessionStatus(message);
                juce::AlertWindow::showMessageBoxAsync(
                    requestOk ? juce::AlertWindow::InfoIcon : juce::AlertWindow::WarningIcon,
                    "Update check",
                    message);
            }
        });
    }).detach();
}

void MainComponent::openUpdateDownload()
{
    if (latestUpdateUrl_.isEmpty())
    {
        checkForUpdates(true);
        return;
    }

    juce::URL(latestUpdateUrl_).launchInDefaultBrowser();
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
            AgentRegistry::get().setJobRunning(AgentIds::beatstars, "Publishing to BeatStars...");

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
                            AgentRegistry::get().setJobProgress(AgentIds::beatstars, 55, status);
                        });
                    });

                juce::MessageManager::callAsync([safe, uploadResult, req, endpoint]()
                {
                    if (safe == nullptr)
                        return;

                    if (safe->bottomDock_)
                        safe->bottomDock_->setSessionStatus(uploadResult.ok ? "Published on BeatStars"
                                                                            : "BeatStars publish failed");
                    if (uploadResult.ok)
                        AgentRegistry::get().setJobDone(AgentIds::beatstars, "Published on BeatStars");
                    else
                        AgentRegistry::get().setJobFailed(AgentIds::beatstars,
                            uploadResult.error.isNotEmpty() ? uploadResult.error : "BeatStars publish failed");

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

void MainComponent::renderVideoInBeatsStudio()
{
    if (!transportBar_)
        return;

    // 1) Work out a clean beat name from the current pattern.
    juce::String beatName = "Stratum Beat";
    {
        auto patterns = transportBar_->getPatterns();
        const int idx = transportBar_->getCurrentPattern();
        if (idx >= 0 && idx < patterns.size() && patterns[idx].isNotEmpty())
            beatName = patterns[idx];
    }
    juce::String safeName = beatName.retainCharacters(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_");
    if (safeName.trim().isEmpty())
        safeName = "Stratum Beat";

    // 2) Export the full mix to a stable folder (not a temp dir, so the file
    //    survives until Beats Studio's renderer reads it).
    auto outDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                      .getChildFile("Stratum DAW").getChildFile("VideoBeats");
    outDir.createDirectory();
    const juce::String stamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    auto wavFile = outDir.getChildFile(safeName + "_" + stamp + ".wav");

    if (!exportAudioToFile(wavFile))
    {
        if (transportBar_) transportBar_->setVideoRenderIdle();
        AgentRegistry::get().setJobFailed(AgentIds::createVideo, "Could not export beat audio");
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Render Video", "Could not export the beat audio for rendering.");
        return;
    }

    saveProjectAndRenderWavCopy(safeName, wavFile);

    AgentRegistry::get().setJobRunning(AgentIds::createVideo, "Rendering video in Beats Studio...");

    // 3) Build the bridge command (JSON-escape the Windows path).
    juce::String escapedPath = wavFile.getFullPathName().replace("\\", "\\\\");
    juce::String escapedName = safeName.replace("\\", "\\\\").replace("\"", "\\\"");
    juce::String cmd = "{\"action\":\"renderVideo\",\"audioPath\":\""
                     + escapedPath + "\",\"outputName\":\"" + escapedName + "\"}";

    // 4) Open the bridge socket, send the command, then keep it open to read
    //    progress / completion messages streamed back by Beats Studio. The
    //    transport's Create-Video button reflects the state live.
    juce::Thread::launch([this, cmd]()
    {
        auto resetWithMessage = [this](const juce::String& m)
        {
            juce::MessageManager::callAsync([this, m]()
            {
                if (transportBar_) transportBar_->setVideoRenderIdle();
                AgentRegistry::get().setJobFailed(AgentIds::createVideo, m);
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                    "Render Video", m);
            });
        };

        juce::StreamingSocket socket;
        if (!socket.connect("127.0.0.1", 9003, 1000))
        {
            resetWithMessage("Beats Studio isn't running. Click BEATS STUDIO to launch it, then try again.");
            return;
        }

        socket.write(cmd.toRawUTF8(), (int)cmd.getNumBytesAsUTF8());

        juce::String buffer;
        char chunk[2048];
        const juce::int64 startMs = juce::Time::getMillisecondCounter();
        const juce::int64 maxWaitMs = 900000; // 15 min ceiling
        bool finished = false;

        while (juce::Time::getMillisecondCounter() - startMs < maxWaitMs)
        {
            const int ready = socket.waitUntilReady(true, 1000);
            if (ready < 0) break;        // socket error
            if (ready == 0) continue;    // nothing yet — keep waiting

            const int n = socket.read(chunk, (int)sizeof(chunk) - 1, false);
            if (n <= 0) break;           // disconnected
            buffer += juce::String::fromUTF8(chunk, n);

            for (;;)
            {
                const int nl = buffer.indexOfChar('\n');
                if (nl < 0) break;
                const juce::String line = buffer.substring(0, nl).trim();
                buffer = buffer.substring(nl + 1);
                if (line.isEmpty()) continue;

                auto v = juce::JSON::parse(line);
                if (auto* o = v.getDynamicObject())
                {
                    const juce::String type = o->getProperty("type").toString();
                    if (type == "progress")
                    {
                        const int pct = (int)o->getProperty("value");
                        juce::MessageManager::callAsync([this, pct]()
                        {
                            if (transportBar_) transportBar_->setVideoRenderProgress(pct);
                            AgentRegistry::get().setJobProgress(AgentIds::createVideo, pct, "Rendering video...");
                        });
                    }
                    else if (type == "done")
                    {
                        const juce::String out = o->getProperty("outputPath").toString();
                        juce::MessageManager::callAsync([this, out]()
                        {
                            if (transportBar_) transportBar_->setVideoRenderDone(out);
                            AgentRegistry::get().setJobDone(AgentIds::createVideo, "Video ready: " + out);
                            openRenderedVideoWindow(juce::File(out));
                        });
                        finished = true;
                    }
                    else if (type == "error")
                    {
                        resetWithMessage("Render failed: " + o->getProperty("error").toString());
                        finished = true;
                    }
                }
            }
            if (finished) break;
        }

        if (!finished)
            juce::MessageManager::callAsync([this]()
            {
                if (transportBar_) transportBar_->setVideoRenderIdle();
                AgentRegistry::get().setJobFailed(AgentIds::createVideo, "Render timed out");
            });
    });
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
    // Refresh the open piano roll so its notes match the restored channel data.
    if (reloadPianoRollFromState_)
        reloadPianoRollFromState_();
    restoringSnapshot_ = false;
    repaint();
}

void MainComponent::timerCallback()
{
    // ── Perf sampling: log objective numbers ~every 2s to perf.log while
    //    playing (only — no idle disk chatter). Drivable for diagnostics. ──
    {
        static int perfCounter = 0;
        const bool   playing  = transportBar_ && transportBar_->isPlaying();
        if (playing && ++perfCounter >= 5) // 5 × 400ms ≈ 2s
        {
            perfCounter = 0;
            const double peakMs   = pluginHost_.readAndResetAudioPeakMs();
            const double budgetMs = pluginHost_.getAudioBlockBudgetMs();
            const int    voices   = pluginHost_.getActiveSampleVoiceCount();
            const double jitterMs = channelRack_ ? channelRack_->readAndResetTimerJitterMs() : 0.0;
            const double load     = (budgetMs > 0.0) ? (peakMs / budgetMs * 100.0) : 0.0;
            const juce::String lineOut =
                  juce::Time::getCurrentTime().toString(false, true)
                + (playing ? "  [PLAY]" : "  [stop]")
                + "  audioPeak=" + juce::String(peakMs, 3) + "ms"
                + " /" + juce::String(budgetMs, 2) + "ms (" + juce::String(load, 1) + "% load)"
                + "  seqJitter=" + juce::String(jitterMs, 2) + "ms"
                + "  voices=" + juce::String(voices) + "\n";
            // Write off the message thread — synchronous disk I/O here would
            // stall the UI/sequencer clock and pollute the very jitter metric.
            juce::Thread::launch([lineOut]
            {
                juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("Stratum DAW").getChildFile("perf.log")
                    .appendText(lineOut);
            });
        }
    }

    if (restoringSnapshot_) return;

    // Undo polling serializes the whole project to JSON. On big projects that
    // gets expensive, and at 400ms it competes with the 16ms sequencer/UI
    // timers on the message thread. Adapt: if a snapshot costs real time,
    // poll less often (snappy undo matters less than playback smoothness).
    const auto snapT0 = juce::Time::getMillisecondCounterHiRes();
    auto current = captureSnapshotJson();
    const double snapMs = juce::Time::getMillisecondCounterHiRes() - snapT0;

    const int desiredInterval = snapMs > 4.0 ? 2000
                              : snapMs > 1.5 ? 1000
                              :                400;
    if (desiredInterval != getTimerInterval())
        startTimer(desiredInterval);

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
    // Flush any edit that the ~400ms poll hasn't captured yet so the most
    // recent change (piano roll, mixer, playlist, anything) is undoable
    // immediately — no waiting for the next poll tick.
    auto current = captureSnapshotJson();
    if (current != lastSnapshotJson_)
    {
        undoStack_.push_back(lastSnapshotJson_);
        if (undoStack_.size() > kMaxUndo)
            undoStack_.erase(undoStack_.begin(),
                             undoStack_.begin() + (undoStack_.size() - kMaxUndo));
        redoStack_.clear();
        lastSnapshotJson_ = current;
    }

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
