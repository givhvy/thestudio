#include "MarketplacePanel.h"
#include "Theme.h"
#include <algorithm>

namespace
{
constexpr int titleH = 46;
constexpr int sideW = 178;
constexpr int rowH = 106;

juce::StringArray categoryList()
{
    return { "All", "Drum Kits", "MIDI Kits", "Loops", "One Shots", "Local Drafts" };
}

juce::String normaliseCategory(juce::String category)
{
    category = category.trim();
    if (category.equalsIgnoreCase("drums") || category.equalsIgnoreCase("drum kits"))
        return "Drum Kits";
    if (category.equalsIgnoreCase("midi") || category.equalsIgnoreCase("midi kits"))
        return "MIDI Kits";
    if (category.equalsIgnoreCase("one shots") || category.equalsIgnoreCase("oneshots"))
        return "One Shots";
    if (category.equalsIgnoreCase("loops"))
        return "Loops";
    return category.isNotEmpty() ? category : "Drum Kits";
}

juce::Colour categoryColour(const juce::String& category)
{
    if (category.equalsIgnoreCase("MIDI Kits")) return juce::Colour(0xff8b5cf6);
    if (category.equalsIgnoreCase("Loops")) return juce::Colour(0xff3b82f6);
    if (category.equalsIgnoreCase("One Shots")) return juce::Colour(0xff22c55e);
    return Theme::accentBright;
}
}

MarketplacePanel::MarketplacePanel()
{
    setWantsKeyboardFocus(true);

    searchEditor_.setMultiLine(false);
    searchEditor_.setReturnKeyStartsNewLine(false);
    searchEditor_.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f));
    searchEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff111114));
    searchEditor_.setColour(juce::TextEditor::textColourId, juce::Colour(0xfff4f4f5));
    searchEditor_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff34343a));
    searchEditor_.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accentBright);
    searchEditor_.setTextToShowWhenEmpty("Search kits, MIDI, loops, genres...", Theme::zinc500);
    searchEditor_.onTextChange = [this] { rebuildVisible(); repaint(); };
    addAndMakeVisible(searchEditor_);

    closeButton_.onClick = [this] { if (onClose) onClose(); };
    refreshButton_.onClick = [this] { loadCatalogAsync(); };
    postButton_.onClick = [this] { startPostKit(); };
    for (auto* b : { &closeButton_, &refreshButton_, &postButton_ })
    {
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b1b20));
        b->setColour(juce::TextButton::buttonOnColourId, Theme::accent);
        b->setColour(juce::TextButton::textColourOffId, Theme::zinc100);
        addAndMakeVisible(*b);
    }

    loadCatalogAsync();
}

MarketplacePanel::~MarketplacePanel()
{
    destroyed_ = true;
}

juce::File MarketplacePanel::marketplaceRoot()
{
   #if JUCE_WINDOWS
    auto root = juce::File("D:\\Stratum Marketplace");
    if (!root.createDirectory())
        root = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Stratum Marketplace");
   #else
    auto root = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Stratum Marketplace");
   #endif
    root.createDirectory();
    return root;
}

juce::File MarketplacePanel::libraryRootForCategory(const juce::String& category)
{
    const auto c = normaliseCategory(category);
    if (c == "Loops")
        return marketplaceRoot().getChildFile("Loops");
    if (c == "MIDI Kits")
        return marketplaceRoot().getChildFile("MIDI Kits");
    if (c == "One Shots")
        return marketplaceRoot().getChildFile("One Shots");
    return marketplaceRoot().getChildFile("Drum Kits");
}

juce::File MarketplacePanel::localCatalogFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stratum DAW");
    dir.createDirectory();
    return dir.getChildFile("marketplace.json");
}

juce::File MarketplacePanel::uploadsStagingRoot()
{
    return marketplaceRoot().getChildFile("Uploads").getChildFile("Staging");
}

juce::String MarketplacePanel::safeFolderName(const juce::String& text)
{
    juce::String out = text.trim();
    const juce::String invalidChars = "<>:\"/\\|?*";
    for (int i = 0; i < invalidChars.length(); ++i)
        out = out.replaceCharacter(invalidChars[i], '-');
    while (out.contains("  ")) out = out.replace("  ", " ");
    return out.isNotEmpty() ? out : "Untitled Pack";
}

void MarketplacePanel::loadCatalogAsync()
{
    loading_ = true;
    setStatus("Loading marketplace catalog...");
    packs_.clear();
    rebuildVisible();
    repaint();

    juce::Component::SafePointer<MarketplacePanel> safe(this);
    std::thread([safe]()
    {
        std::vector<Pack> loaded {
            { "stratum-boom-bap-starter", "Stratum Boom Bap Starter Kit", "Stratum", "Drum Kits", "Boom Bap",
              "Kick, snare, hat and rim layout for fast dusty beat starts.", "", "Local" },
            { "stratum-trap-midi-808", "Trap 808 MIDI Toolkit", "Stratum", "MIDI Kits", "Trap",
              "808 slides, kick support rhythms and hat rolls for piano roll learning.", "", "Local" },
            { "stratum-rnb-loop-pocket", "R&B Loop Pocket", "Stratum", "Loops", "RnB",
              "Small loop pack placeholder wired to the same install path as future cloud packs.", "", "Local" },
            { "stratum-one-shot-clean", "Clean One Shot Essentials", "Stratum", "One Shots", "Mixed",
              "A starter one-shot category so marketplace installs show in the app browser.", "", "Local" },
        };

        const auto local = localCatalogFile();
        if (local.existsAsFile())
        {
            auto json = juce::JSON::parse(local);
            if (auto* arr = json.getArray())
            {
                for (const auto& item : *arr)
                {
                    if (!item.isObject())
                        continue;
                    Pack p;
                    p.id = item.getProperty("id", {}).toString();
                    p.title = item.getProperty("title", {}).toString();
                    p.creator = item.getProperty("creator", "Local").toString();
                    p.category = normaliseCategory(item.getProperty("category", "Drum Kits").toString());
                    p.genre = item.getProperty("genre", "User").toString();
                    p.description = item.getProperty("description", {}).toString();
                    p.url = item.getProperty("url", {}).toString();
                    p.sizeLabel = item.getProperty("size", "Local").toString();
                    p.localDraft = (bool)item.getProperty("localDraft", false);
                    if (p.id.isNotEmpty() && p.title.isNotEmpty())
                        loaded.push_back(std::move(p));
                }
            }
        }

        juce::MessageManager::callAsync([safe, loaded = std::move(loaded)]() mutable
        {
            if (safe == nullptr)
                return;
            safe->packs_ = std::move(loaded);
            for (auto& p : safe->packs_)
                p.installed = safe->isPackInstalled(p);
            safe->loading_ = false;
            safe->setStatus("Marketplace ready. Packs install into " + marketplaceRoot().getFullPathName());
            safe->rebuildVisible();
            safe->repaint();
        });
    }).detach();
}

void MarketplacePanel::loadLocalCatalog()
{
    loadCatalogAsync();
}

bool MarketplacePanel::isPackInstalled(const Pack& pack) const
{
    return libraryRootForCategory(pack.category).getChildFile(safeFolderName(pack.title)).isDirectory();
}

void MarketplacePanel::rebuildVisible()
{
    visiblePacks_.clear();
    const auto q = searchEditor_.getText().trim().toLowerCase();
    for (int i = 0; i < (int)packs_.size(); ++i)
    {
        const auto& p = packs_[(size_t)i];
        const bool catOk = selectedCategory_ == "All"
            || (selectedCategory_ == "Local Drafts" && p.localDraft)
            || p.category.equalsIgnoreCase(selectedCategory_);
        const auto haystack = (p.title + " " + p.creator + " " + p.category + " " + p.genre + " " + p.description).toLowerCase();
        const bool searchOk = q.isEmpty() || haystack.contains(q);
        if (catOk && searchOk)
            visiblePacks_.push_back(i);
    }
    scrollY_ = juce::jlimit(0, juce::jmax(0, (int)visiblePacks_.size() * rowH - listRect_.getHeight()), scrollY_);
}

void MarketplacePanel::updateLayout()
{
    panel_ = getLocalBounds().reduced(12);
    closeRect_ = panel_.withTrimmedLeft(panel_.getWidth() - 34).withHeight(28).reduced(4);
    auto header = panel_.removeFromTop(titleH);
    closeButton_.setBounds(closeRect_);
    postRect_ = header.removeFromRight(118).reduced(4, 8);
    refreshButton_.setBounds(header.removeFromRight(86).reduced(4, 8));
    postButton_.setBounds(postRect_);
    header.removeFromLeft(265);
    searchEditor_.setBounds(header.reduced(4, 9));

    sidebar_ = panel_.removeFromLeft(sideW).reduced(8, 12);
    content_ = panel_.reduced(10, 12);
    listRect_ = content_.withTrimmedTop(34);

    categoryRects_.clear();
    int y = sidebar_.getY() + 42;
    for (const auto& c : categoryList())
    {
        categoryRects_.push_back({ c, { sidebar_.getX(), y, sidebar_.getWidth(), 30 } });
        y += 36;
    }
}

void MarketplacePanel::paint(juce::Graphics& g)
{
    updateLayout();
    g.fillAll(juce::Colours::black.withAlpha(0.62f));
    g.setColour(juce::Colour(0xff0d0d10));
    g.fillRoundedRectangle(getLocalBounds().reduced(12).toFloat(), 10.0f);
    g.setColour(Theme::accent.withAlpha(0.85f));
    g.drawRoundedRectangle(getLocalBounds().reduced(12).toFloat(), 10.0f, 1.2f);

    auto full = getLocalBounds().reduced(12);
    auto header = full.removeFromTop(titleH);
    g.setColour(Theme::accentBright);
    g.fillRect(header.withTrimmedRight(0).withHeight(2).withY(header.getBottom() - 2));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(17.0f).withStyle("Bold"));
    g.setColour(Theme::zinc100);
    g.drawText("STRATUM MARKETPLACE", header.withWidth(250).reduced(16, 0), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
    g.setColour(Theme::zinc500);
    g.drawText(status_, content_.withHeight(24), juce::Justification::centredLeft, true);

    g.setColour(juce::Colour(0xff111114));
    g.fillRoundedRectangle(sidebar_.toFloat(), 7.0f);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.setColour(Theme::zinc500);
    g.drawText("BROWSE", sidebar_.withHeight(28).reduced(10, 0), juce::Justification::centredLeft);
    for (const auto& [name, rect] : categoryRects_)
    {
        const bool active = selectedCategory_.equalsIgnoreCase(name);
        g.setColour(active ? Theme::accentBright : juce::Colour(0xff1a1a1f));
        g.fillRoundedRectangle(rect.toFloat(), 5.0f);
        g.setColour(active ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
        g.drawText(name, rect.reduced(12, 0), juce::Justification::centredLeft, true);
    }

    g.saveState();
    g.reduceClipRegion(listRect_);
    int y = listRect_.getY() - scrollY_;
    for (int visible = 0; visible < (int)visiblePacks_.size(); ++visible)
    {
        auto& p = packs_[(size_t)visiblePacks_[(size_t)visible]];
        p.rect = { listRect_.getX(), y, listRect_.getWidth(), rowH - 10 };
        y += rowH;
        if (!p.rect.intersects(listRect_))
            continue;

        const bool selected = visiblePacks_[(size_t)visible] == selectedIndex_;
        g.setColour(selected ? juce::Colour(0xff25170e) : juce::Colour(0xff17171b));
        g.fillRoundedRectangle(p.rect.toFloat(), 7.0f);
        g.setColour(selected ? Theme::accentBright : juce::Colour(0xff2d2d33));
        g.drawRoundedRectangle(p.rect.toFloat().reduced(0.5f), 7.0f, 1.0f);

        auto r = p.rect.reduced(14, 10);
        auto art = r.removeFromLeft(72);
        g.setColour(categoryColour(p.category).withAlpha(0.9f));
        g.fillRoundedRectangle(art.toFloat(), 6.0f);
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(art.reduced(9).toFloat(), 5.0f);
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
        g.drawText(p.category.upToFirstOccurrenceOf(" ", false, false).toUpperCase(), art.reduced(6), juce::Justification::centred, true);

        auto action = r.removeFromRight(96).withHeight(30).withY(p.rect.getCentreY() - 15);
        p.actionRect = action;
        g.setColour(p.installed ? juce::Colour(0xff14532d) : Theme::accentBright);
        g.fillRoundedRectangle(action.toFloat(), 5.0f);
        g.setColour(p.installed ? juce::Colour(0xff86efac) : juce::Colours::black);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
        g.drawText(p.installed ? "INSTALLED" : "DOWNLOAD", action, juce::Justification::centred, true);

        r.removeFromLeft(14);
        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.5f).withStyle("Bold"));
        g.drawText(p.title, r.removeFromTop(20), juce::Justification::centredLeft, true);
        g.setColour(categoryColour(p.category));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
        g.drawText(p.category.toUpperCase() + "  /  " + p.genre.toUpperCase() + "  /  " + p.sizeLabel,
                   r.removeFromTop(18), juce::Justification::centredLeft, true);
        g.setColour(Theme::zinc400);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
        g.drawText(p.description, r.removeFromTop(34), juce::Justification::centredLeft, true);
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
        g.drawText("by " + p.creator, r, juce::Justification::centredLeft, true);
    }
    g.restoreState();

    if (loading_)
    {
        g.setColour(Theme::accentBright);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
        g.drawText("Loading...", listRect_, juce::Justification::centred);
    }
    else if (visiblePacks_.empty())
    {
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f));
        g.drawText("No packs found.", listRect_, juce::Justification::centred);
    }
}

void MarketplacePanel::resized()
{
    updateLayout();
}

void MarketplacePanel::mouseDown(const juce::MouseEvent& e)
{
    updateLayout();
    for (const auto& [name, rect] : categoryRects_)
    {
        if (rect.contains(e.getPosition()))
        {
            selectedCategory_ = name;
            rebuildVisible();
            repaint();
            return;
        }
    }

    if (!listRect_.contains(e.getPosition()))
        return;

    for (int idx : visiblePacks_)
    {
        const auto& p = packs_[(size_t)idx];
        if (!p.rect.contains(e.getPosition()))
            continue;
        selectedIndex_ = idx;
        if (p.actionRect.contains(e.getPosition()))
            startInstall(idx);
        repaint();
        return;
    }
}

void MarketplacePanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const int maxScroll = juce::jmax(0, (int)visiblePacks_.size() * rowH - listRect_.getHeight());
    scrollY_ = juce::jlimit(0, maxScroll, scrollY_ - (int)std::round(wheel.deltaY * 90.0f));
    repaint();
}

bool MarketplacePanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }
    return false;
}

void MarketplacePanel::setStatus(const juce::String& text)
{
    status_ = text;
    if (onStatus)
        onStatus(text);
}

void MarketplacePanel::startInstall(int packIndex)
{
    if (packIndex < 0 || packIndex >= (int)packs_.size())
        return;

    if (packs_[(size_t)packIndex].installed)
    {
        libraryRootForCategory(packs_[(size_t)packIndex].category)
            .getChildFile(safeFolderName(packs_[(size_t)packIndex].title))
            .revealToUser();
        return;
    }

    const auto url = packs_[(size_t)packIndex].url;
    if (url.trim().isEmpty())
    {
        installLocalPack(packIndex);
        return;
    }

    setStatus("Downloading " + packs_[(size_t)packIndex].title + "...");
    juce::Component::SafePointer<MarketplacePanel> safe(this);
    std::thread([safe, packIndex, url]()
    {
        auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("stratum_marketplace_" + juce::String(packIndex) + ".download");
        bool ok = false;
        if (auto stream = juce::URL(url).createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(10000)
                    .withExtraHeaders("User-Agent: Stratum-DAW\r\n")))
        {
            juce::FileOutputStream out(temp);
            ok = out.openedOk() && out.writeFromInputStream(*stream, -1) > 0;
        }

        juce::MessageManager::callAsync([safe, packIndex, temp, ok]()
        {
            if (safe == nullptr)
                return;
            if (!ok)
            {
                safe->setStatus("Download failed. Check the pack URL or your internet connection.");
                safe->repaint();
                return;
            }
            safe->installDownloadedFile(packIndex, temp);
        });
    }).detach();
}

void MarketplacePanel::installLocalPack(int packIndex)
{
    if (packIndex < 0 || packIndex >= (int)packs_.size())
        return;

    const auto& pack = packs_[(size_t)packIndex];
    auto folder = libraryRootForCategory(pack.category).getChildFile(safeFolderName(pack.title));
    folder.createDirectory();
    folder.getChildFile("README.txt").replaceWithText(
        pack.title + "\n"
        "Creator: " + pack.creator + "\n"
        "Category: " + pack.category + "\n"
        "Genre: " + pack.genre + "\n\n"
        + pack.description + "\n\n"
        "This is a marketplace placeholder pack. Connect a real marketplace manifest URL to download full files.");

    packs_[(size_t)packIndex].installed = true;
    setStatus("Installed " + pack.title + " to " + folder.getFullPathName());
    if (onLibraryChanged)
        onLibraryChanged();
    repaint();
}

void MarketplacePanel::installDownloadedFile(int packIndex, const juce::File& sourceFile)
{
    if (packIndex < 0 || packIndex >= (int)packs_.size())
        return;

    const auto& pack = packs_[(size_t)packIndex];
    auto folder = libraryRootForCategory(pack.category).getChildFile(safeFolderName(pack.title));
    folder.createDirectory();

    const auto ext = sourceFile.getFileExtension().toLowerCase();
    if (ext == ".zip")
    {
        juce::ZipFile zip(sourceFile);
        zip.uncompressTo(folder, true);
    }
    else
    {
        sourceFile.copyFileTo(folder.getChildFile(pack.title).withFileExtension(sourceFile.getFileExtension()));
    }

    packs_[(size_t)packIndex].installed = true;
    setStatus("Installed " + pack.title + " to " + folder.getFullPathName());
    if (onLibraryChanged)
        onLibraryChanged();
    repaint();
}

void MarketplacePanel::startPostKit()
{
    auto staging = uploadsStagingRoot();
    staging.createDirectory();
    postChooser_ = std::make_unique<juce::FileChooser>("Choose a kit folder or ZIP to stage for marketplace upload",
                                                       juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                       "*.zip;*.mid;*.midi;*.wav;*.aif;*.aiff;*.mp3");
    postChooser_->launchAsync(juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectDirectories,
        [this, staging](const juce::FileChooser& fc)
        {
            auto picked = fc.getResult();
            if (!picked.exists())
                return;

            const auto targetName = safeFolderName(picked.getFileNameWithoutExtension().isNotEmpty()
                ? picked.getFileNameWithoutExtension()
                : picked.getFileName());
            auto target = staging.getChildFile(targetName);
            bool ok = false;
            if (picked.isDirectory())
                ok = picked.copyDirectoryTo(target);
            else
            {
                target.createDirectory();
                ok = picked.copyFileTo(target.getChildFile(picked.getFileName()));
            }

            if (!ok)
            {
                setStatus("Could not stage selected kit.");
                repaint();
                return;
            }

            auto* obj = new juce::DynamicObject();
            obj->setProperty("id", "local-" + targetName.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-"));
            obj->setProperty("title", targetName);
            obj->setProperty("creator", "You");
            obj->setProperty("category", "Drum Kits");
            obj->setProperty("genre", "User");
            obj->setProperty("description", "Local marketplace draft staged from " + picked.getFullPathName());
            obj->setProperty("size", "Draft");
            obj->setProperty("localDraft", true);

            juce::Array<juce::var> arr;
            auto catalog = localCatalogFile();
            auto parsed = juce::JSON::parse(catalog);
            if (auto* existing = parsed.getArray())
                arr = *existing;
            arr.add(juce::var(obj));
            catalog.replaceWithText(juce::JSON::toString(juce::var(arr), true));

            setStatus("Staged kit for marketplace upload: " + target.getFullPathName());
            loadLocalCatalog();
        });
}
