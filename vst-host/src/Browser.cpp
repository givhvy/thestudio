#include "Browser.h"
#include "BrowserSettingsDialog.h"
#include "PluginHost.h"
#include "MarketplacePanel.h"
#include "Theme.h"
#include "LoopBpmUtils.h"
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef _WIN32
static BOOL CALLBACK collectChordifyWindowsForDrag(HWND hwnd, LPARAM lParam)
{
    if (!IsWindowVisible(hwnd))
        return TRUE;

    wchar_t title[512] {};
    GetWindowTextW(hwnd, title, 512);
    if (juce::String(title).containsIgnoreCase("Chordify"))
        reinterpret_cast<std::vector<HWND>*>(lParam)->push_back(hwnd);

    return TRUE;
}

static std::vector<HWND> keepChordifyWindowsOnTopForDrag()
{
    std::vector<HWND> windows;
    EnumWindows(collectChordifyWindowsForDrag, reinterpret_cast<LPARAM>(&windows));

    for (auto hwnd : windows)
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    return windows;
}

static void releaseChordifyWindowsAfterDrag(const std::vector<HWND>& windows)
{
    for (auto hwnd : windows)
        if (IsWindow(hwnd))
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static void keepChordifyWindowsVisibleForDrag()
{
    std::thread([]()
    {
        auto windows = keepChordifyWindowsOnTopForDrag();
        if (windows.empty())
            return;

        juce::Thread::sleep(10000);
        releaseChordifyWindowsAfterDrag(windows);
    }).detach();
}
#else
static std::vector<void*> keepChordifyWindowsOnTopForDrag() { return {}; }
static void releaseChordifyWindowsAfterDrag(const std::vector<void*>&) {}
static void keepChordifyWindowsVisibleForDrag() {}
#endif

static juce::File getBundledStratumPianoVst3ForBrowser()
{
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto repoRoot = exeDir.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
    return repoRoot.getChildFile("vst-plugins")
                   .getChildFile("_installed")
                   .getChildFile("VST3")
                   .getChildFile("Stratum Piano.vst3");
}

static juce::File getBundledStratumGuitarVst3ForBrowser()
{
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto repoRoot = exeDir.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
    return repoRoot.getChildFile("vst-plugins")
                   .getChildFile("_installed")
                   .getChildFile("VST3")
                   .getChildFile("Stratum Guitar.vst3");
}

static juce::File getRepoRootForBrowser()
{
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    return exeDir.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
}

static juce::String browserNodeDisplayName(const juce::File& file, bool isFolder)
{
    juce::String name = isFolder ? file.getFileName() : file.getFileNameWithoutExtension();
    // Display-only: hide a leading "_" / "-" used to group/sort folders on disk
    // (e.g. "_Afrobeat" → "Afrobeat"). The real path on disk is unchanged.
    if (isFolder)
        while (name.startsWithChar('_') || name.startsWithChar('-'))
            name = name.substring(1);
    return name;
}

struct BrowserNfoSkin
{
    bool valid = false;
    juce::Colour colour = juce::Colour(0xffd6bd72);
    juce::String skinText;
    juce::Image bitmap;
};

static juce::Colour parseBrowserNfoColour(const juce::String& value)
{
    auto text = value.trim();
    if (text.startsWithChar('$'))
        text = text.substring(1);
    if (text.startsWithIgnoreCase("0x"))
        text = text.substring(2);

    if (text.length() < 6)
        return juce::Colour(0xffd6bd72);

    const auto rgb = (juce::uint32)text.substring(0, 6).getHexValue32();
    return juce::Colour((juce::uint8)((rgb >> 16) & 0xff),
                        (juce::uint8)((rgb >> 8) & 0xff),
                        (juce::uint8)(rgb & 0xff));
}

static juce::File findBrowserNfoSidecar(const juce::File& folder, const juce::String& baseName)
{
    if (!folder.isDirectory())
        return {};

    juce::Array<juce::File> nfos;
    folder.findChildFiles(nfos, juce::File::findFiles, false, "*.nfo");
    folder.findChildFiles(nfos, juce::File::findFiles, false, "*.NFO");

    for (const auto& nfo : nfos)
        if (nfo.getFileNameWithoutExtension().equalsIgnoreCase(baseName))
            return nfo;

    return {};
}

static BrowserNfoSkin loadBrowserNfoSkin(const juce::File& nfoFile)
{
    BrowserNfoSkin skin;
    if (!nfoFile.existsAsFile())
        return skin;

    skin.valid = true;
    skin.skinText = nfoFile.getFileNameWithoutExtension().toUpperCase();

    juce::String bitmapPath;
    juce::StringArray lines;
    lines.addLines(nfoFile.loadFileAsString());
    for (auto line : lines)
    {
        line = line.trim();
        const int eq = line.indexOfChar('=');
        if (eq <= 0)
            continue;

        const auto key = line.substring(0, eq).trim().toLowerCase();
        const auto value = line.substring(eq + 1).trim();
        if (key == "color")
            skin.colour = parseBrowserNfoColour(value);
        else if (key == "bitmap")
            bitmapPath = value.trimCharactersAtStart("\"").trimCharactersAtEnd("\"");
    }

    if (bitmapPath.isNotEmpty())
    {
        juce::File bitmapFile(bitmapPath);
        if (!juce::File::isAbsolutePath(bitmapPath))
            bitmapFile = nfoFile.getParentDirectory().getChildFile(bitmapPath);
        if (bitmapFile.existsAsFile())
            skin.bitmap = juce::ImageFileFormat::loadFrom(bitmapFile);
    }

    return skin;
}


static juce::String getBrowserLibraryHeaderName(int libraryIndex)
{
    switch (libraryIndex)
    {
        case 1:  return "Drum Kit";
        case 2:  return "Loops";
        case 3:  return "Acapella";
        case 4:  return "Tags";
        case 5:  return "Marketplace";
        default: return "All Libraries";
    }
}

Browser::Browser(PluginHost& pluginHost)
    : pluginHost_(pluginHost)
{
    userPaths_ = loadLibraryPaths();
    restorePanelHeight();
    refreshPluginList();

    // Default to the Drums library; setLibrary picks an existing path and
    // rebuilds the tree.
    setLibrary(Library::Drums);

    // Folder search editor (hidden until user clicks "BROWSER" header)
    searchEditor_.reset(new juce::TextEditor());
    searchEditor_->setMultiLine(false);
    searchEditor_->setReturnKeyStartsNewLine(false);
    searchEditor_->setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
    searchEditor_->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff141417));
    searchEditor_->setColour(juce::TextEditor::textColourId,       juce::Colour(0xfff97316));
    searchEditor_->setColour(juce::TextEditor::outlineColourId,    juce::Colour(0xff3f3f46));
    searchEditor_->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xfff97316));
    searchEditor_->setTextToShowWhenEmpty("Search folders...", juce::Colour(0xff52525b));
    searchEditor_->setEscapeAndReturnKeysConsumed(true);
    searchEditor_->onTextChange  = [this]() { performSearch(searchEditor_->getText()); };
    searchEditor_->onReturnKey   = [this]() { performSearch(searchEditor_->getText()); };
    searchEditor_->onEscapeKey   = [this]() { endSearch(); };
    searchEditor_->onFocusLost   = [this]() { if (searchEditor_->isEmpty()) endSearch(); };
    addChildComponent(searchEditor_.get());
}

Browser::~Browser() = default;

juce::File Browser::resolveLoopsRootFolder()
{
    const juce::Array<juce::File> candidates {
        juce::File("F:\\1500 LOOPS FOLDER"),
        juce::File("E:\\1500 LOOPS FOLDER"),
    };

    for (const auto& c : candidates)
        if (c.isDirectory())
            return c;

    return {};
}

namespace
{
bool isLoopAudioFile(const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".ogg"
        || ext == ".aif" || ext == ".aiff";
}

void collectLoopsMatchingBpmRecursive(const juce::File& folder,
                                      double targetBpm,
                                      double tolerance,
                                      std::vector<juce::File>& out,
                                      int maxResults)
{
    if (!folder.isDirectory() || (int)out.size() >= maxResults)
        return;

    juce::Array<juce::File> children;
    folder.findChildFiles(children, juce::File::findFilesAndDirectories, false);

    for (const auto& child : children)
    {
        if ((int)out.size() >= maxResults)
            break;

        if (child.isDirectory())
            collectLoopsMatchingBpmRecursive(child, targetBpm, tolerance, out, maxResults);
        else if (isLoopAudioFile(child) && loopFileMatchesTargetBpm(child, targetBpm, tolerance))
            out.push_back(child);
    }
}
}

std::vector<juce::File> Browser::findLoopsMatchingBpm(double targetBpm, double tolerance)
{
    std::vector<juce::File> matches;
    const auto root = resolveLoopsRootFolder();
    if (root.isDirectory())
        collectLoopsMatchingBpmRecursive(root, targetBpm, tolerance, matches, 250);

    std::sort(matches.begin(), matches.end(), [](const juce::File& a, const juce::File& b) {
        return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
    });
    return matches;
}

void Browser::focusLoopsLibrary()
{
    setLibrary(Library::Loops);
}

void Browser::refreshCurrentLibrary()
{
    setLibrary(currentLibrary_);
}

bool Browser::isAudioFile(const juce::File& f) const
{
    auto ext = f.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".ogg" 
        || ext == ".aiff" || ext == ".aif";
}

std::vector<Browser::TreeNode> Browser::listFolderChildren(const juce::File& folder, int depth) const
{
    std::vector<TreeNode> nodes;
    if (!folder.isDirectory())
        return nodes;

    juce::Array<juce::File> dirs = folder.findChildFiles(juce::File::findDirectories, false, "*");
    juce::Array<juce::File> files = folder.findChildFiles(juce::File::findFiles, false, "*");

    auto sortByName = [](const juce::File& a, const juce::File& b) {
        return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
    };
    std::sort(dirs.begin(), dirs.end(), sortByName);
    std::sort(files.begin(), files.end(), sortByName);

    auto addNode = [&](const juce::File& child, bool isDir)
    {
        TreeNode node;
        node.file = child;
        node.displayName = browserNodeDisplayName(child, isDir);
        node.depth = depth;
        node.isFolder = isDir;
        node.isAudio = !isDir && isAudioFile(child);
        node.isExpanded = false;
        if (isDir)
        {
            const auto nfo = findBrowserNfoSidecar(folder, child.getFileName());
            const auto skin = loadBrowserNfoSkin(nfo);
            if (skin.valid)
            {
                node.hasNfoSkin = true;
                node.nfoColour = skin.colour;
                node.nfoSkinText = skin.skinText;
                node.nfoBitmap = skin.bitmap;
            }
        }
        if (isDir || node.isAudio)
            nodes.push_back(std::move(node));
    };

    for (auto& d : dirs)
        addNode(d, true);

    for (auto& nfo : files)
    {
        if (!nfo.getFileExtension().equalsIgnoreCase(".nfo"))
            continue;

        const auto baseName = nfo.getFileNameWithoutExtension();
        if (baseName.startsWithIgnoreCase("info"))
            continue;

        bool hasRealFolder = false;
        for (const auto& d : dirs)
        {
            if (d.getFileName().equalsIgnoreCase(baseName))
            {
                hasRealFolder = true;
                break;
            }
        }
        if (hasRealFolder)
            continue;

        const auto skin = loadBrowserNfoSkin(nfo);
        if (!skin.valid)
            continue;

        TreeNode node;
        node.file = nfo;
        node.displayName = baseName;
        node.depth = depth;
        node.isFolder = true;
        node.isAudio = false;
        node.isExpanded = false;
        node.hasNfoSkin = true;
        node.virtualNfoFolder = true;
        node.nfoColour = skin.colour;
        node.nfoSkinText = skin.skinText;
        node.nfoBitmap = skin.bitmap;
        nodes.push_back(std::move(node));
    }

    for (auto& f : files)
        addNode(f, false);

    return nodes;
}

void Browser::loadFolderChildrenAsync(int parentNodeIndex)
{
    if (parentNodeIndex < 0 || parentNodeIndex >= (int)allNodes_.size())
        return;

    const auto parent = allNodes_[(size_t)parentNodeIndex];
    if (!parent.isFolder || !parent.file.isDirectory())
        return;

    const int loadGen = ++folderLoadGeneration_;
    const juce::File folder = parent.file;
    const int childDepth = parent.depth + 1;

    juce::Component::SafePointer<Browser> safeThis(this);
    std::thread([safeThis, folder, parentNodeIndex, childDepth, loadGen]()
    {
        std::vector<TreeNode> kids;
        if (safeThis != nullptr)
            kids = safeThis->listFolderChildren(folder, childDepth);

        juce::MessageManager::callAsync([safeThis, kids = std::move(kids), parentNodeIndex, loadGen]() mutable
        {
            if (safeThis == nullptr || loadGen != safeThis->folderLoadGeneration_.load())
                return;
            if (parentNodeIndex < 0 || parentNodeIndex >= (int)safeThis->allNodes_.size())
                return;

            const int insertAt = parentNodeIndex + 1;
            safeThis->allNodes_.insert(safeThis->allNodes_.begin() + insertAt,
                                      kids.begin(), kids.end());
            safeThis->allNodes_[(size_t)parentNodeIndex].isExpanded = true;
            safeThis->rebuildVisible();
            safeThis->repaint();
        });
    }).detach();
}

void Browser::scanFolder(const juce::File& folder, int depth)
{
    if (!folder.isDirectory()) return;

    auto children = listFolderChildren(folder, depth);
    allNodes_.insert(allNodes_.end(), children.begin(), children.end());
}

void Browser::buildTree()
{
    allNodes_.clear();
    
    auto headerName = getBrowserLibraryHeaderName((int)currentLibrary_);

    // Root header (always shown, expanded)
    TreeNode header;
    header.displayName = rootFolder_.exists() ? headerName : (headerName + " (folder not found)");
    header.depth = 0;
    header.isFolder = true;
    header.isExpanded = true;
    header.file = rootFolder_;
    allNodes_.push_back(header);

    if (rootFolder_.isDirectory())
        scanFolder(rootFolder_, 1);
    
    rebuildVisible();
}

void Browser::rebuildVisible()
{
    visibleIndices_.clear();
    
    int hideUntilDepth = -1;
    for (int i = 0; i < (int)allNodes_.size(); ++i)
    {
        const auto& n = allNodes_[i];
        // If a parent collapsed, hide children deeper than that
        if (hideUntilDepth >= 0 && n.depth > hideUntilDepth) continue;
        hideUntilDepth = -1;
        
        visibleIndices_.push_back(i);
        
        if (n.isFolder && !n.isExpanded)
            hideUntilDepth = n.depth;
    }
}

juce::Rectangle<int> Browser::getAllFilterRect() const
{
    int top = ADMIN_H + 5;
    return juce::Rectangle<int>(getWidth() - 92, top, 84, 18);
}

juce::Rectangle<int> Browser::getZoomMinusRect() const
{
    return juce::Rectangle<int>(116, ADMIN_H + 6, 18, 16);
}

juce::Rectangle<int> Browser::getZoomPlusRect() const
{
    return juce::Rectangle<int>(138, ADMIN_H + 6, 18, 16);
}

juce::String Browser::libraryLabel() const
{
    switch (currentLibrary_)
    {
        case Library::Drums: return "DRUMS";
        case Library::Loops: return "LOOPS";
        case Library::Acapella: return "ACAPELLA";
        case Library::Tags: return "TAGS";
        case Library::Marketplace: return "MARKET";
        case Library::All:   return "ALL";
    }
    return "ALL";
}

// ── User-configured library folders ───────────────────────────
//
// The user can point each library category at any folder on disk via
// the "Set Library Folders…" item in the library dropdown. These
// paths are persisted to ~/Library/Application Support/Stratum DAW/
// and take priority over the built-in fallbacks (and over the
// MarketplacePanel defaults) in setLibrary() below.

Browser::LibraryPaths Browser::loadLibraryPaths()
{
    LibraryPaths p;
    juce::PropertiesFile::Options opts;
    opts.applicationName = "Stratum DAW";
    opts.osxLibrarySubFolder = "Application Support";
    opts.filenameSuffix = "library-paths";
    juce::PropertiesFile f (opts);
    p.drums       = f.getValue ("drums",       {});
    p.loops       = f.getValue ("loops",       {});
    p.acapella    = f.getValue ("acapella",    {});
    p.tags        = f.getValue ("tags",        {});
    p.marketplace = f.getValue ("marketplace", {});
    p.all         = f.getValue ("all",         {});
    return p;
}

void Browser::saveLibraryPaths(const LibraryPaths& p)
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "Stratum DAW";
    opts.osxLibrarySubFolder = "Application Support";
    opts.filenameSuffix = "library-paths";
    juce::PropertiesFile f (opts);
    f.setValue ("drums",       p.drums);
    f.setValue ("loops",       p.loops);
    f.setValue ("acapella",    p.acapella);
    f.setValue ("tags",        p.tags);
    f.setValue ("marketplace", p.marketplace);
    f.setValue ("all",         p.all);
    f.save();
}

juce::File Browser::userLibraryFolder(Library lib) const
{
    const juce::String* s = nullptr;
    switch (lib)
    {
        case Library::Drums:      s = &userPaths_.drums;       break;
        case Library::Loops:      s = &userPaths_.loops;       break;
        case Library::Acapella:   s = &userPaths_.acapella;    break;
        case Library::Tags:       s = &userPaths_.tags;        break;
        case Library::Marketplace:s = &userPaths_.marketplace; break;
        case Library::All:        s = &userPaths_.all;         break;
    }
    if (s == nullptr || s->isEmpty()) return {};
    juce::File f (*s);
    return f.isDirectory() ? f : juce::File();
}

void Browser::showSettingsDialog()
{
    BrowserSettingsDialog::Paths p;
    p.drums       = userPaths_.drums;
    p.loops       = userPaths_.loops;
    p.acapella    = userPaths_.acapella;
    p.tags        = userPaths_.tags;
    p.marketplace = userPaths_.marketplace;
    p.all         = userPaths_.all;
    auto* dlg = new BrowserSettingsDialog (p, this);
    addAndMakeVisible (dlg);
    juce::Component::SafePointer<Browser> safe (this);
    dlg->runModal ([safe] (const BrowserSettingsDialog::Paths& p)
    {
        if (auto* self = safe.getComponent())
        {
            // Mirror into our struct (same field names).
            self->userPaths_.drums       = p.drums;
            self->userPaths_.loops       = p.loops;
            self->userPaths_.acapella    = p.acapella;
            self->userPaths_.tags        = p.tags;
            self->userPaths_.marketplace = p.marketplace;
            self->userPaths_.all         = p.all;
            saveLibraryPaths (self->userPaths_);
            self->refreshCurrentLibrary();
        }
    });
}

void Browser::setLibrary(Library lib)
{
    currentLibrary_ = lib;
    const int scanGeneration = ++libraryScanGeneration_;

    juce::Array<juce::File> candidates;
    switch (lib)
    {
        case Library::Drums:
            candidates = {
                MarketplacePanel::libraryRootForCategory("Drum Kits"),
                juce::File("E:\\!Storage\\1500 THE DRUMS LORD COLLECTION"),
                juce::File("E:\\Storage\\1500 THE DRUMS LORD COLLECTION"),
                juce::File("E:\\1500 THE DRUMS LORD COLLECTION"),
                juce::File("F:\\1500 THE DRUMS LORD COLLECTION"),
            };
            break;
        case Library::Loops:
            candidates = {
                juce::File("F:\\1500 LOOPS FOLDER"),
                juce::File("E:\\1500 LOOPS FOLDER"),
                MarketplacePanel::libraryRootForCategory("Loops"),
            };
            break;
        case Library::Acapella:
            candidates = {
                juce::File("D:\\Acapella"),
                juce::File("F:\\1500 ACAPELLA FOLDER"),
                juce::File("E:\\1500 ACAPELLA FOLDER"),
                juce::File("F:\\ACAPELLA"),
                juce::File("E:\\ACAPELLA"),
                getRepoRootForBrowser().getChildFile("audio").getChildFile("acapella"),
                getRepoRootForBrowser().getChildFile("samples").getChildFile("acapella"),
            };
            break;
        case Library::Tags:
            candidates = {
                juce::File("D:\\tags"),
            };
            break;
        case Library::Marketplace:
            candidates = {
                MarketplacePanel::marketplaceRoot(),
            };
            break;
        case Library::All:
            // "All" = whichever drive has both libraries' parent. Pick the
            // common root if it exists, else fall back to current.
            candidates = {
                MarketplacePanel::marketplaceRoot(),
                juce::File("F:\\"),
                juce::File("E:\\"),
            };
            break;
    }

    // User-configured path takes priority over the built-in fallbacks.
    {
        const auto user = userLibraryFolder (lib);
        if (user.isDirectory()) { rootFolder_ = user; return; }
    }

    rootFolder_ = juce::File();
    for (auto& c : candidates)
        if (c.isDirectory()) { rootFolder_ = c; break; }

    if (lib == Library::Acapella && !rootFolder_.isDirectory())
    {
        rootFolder_ = getRepoRootForBrowser().getChildFile("audio").getChildFile("acapella");
        rootFolder_.createDirectory();
    }
    if (lib == Library::Tags && !rootFolder_.isDirectory())
    {
        rootFolder_ = juce::File("D:\\tags");
        rootFolder_.createDirectory();
    }
    if ((lib == Library::Drums || lib == Library::Loops) && !rootFolder_.isDirectory())
    {
        rootFolder_ = MarketplacePanel::libraryRootForCategory(lib == Library::Loops ? "Loops" : "Drum Kits");
        rootFolder_.createDirectory();
    }
    if (lib == Library::Marketplace && !rootFolder_.isDirectory())
    {
        rootFolder_ = MarketplacePanel::marketplaceRoot();
        rootFolder_.createDirectory();
    }

    selectedIdx_ = -1;
    scrollY_     = 0;

    auto headerName = getBrowserLibraryHeaderName((int)currentLibrary_);

    TreeNode header;
    header.displayName = rootFolder_.exists() ? headerName : (headerName + " (folder not found)");
    header.depth = 0;
    header.isFolder = true;
    header.isExpanded = true;
    header.file = rootFolder_;

    allNodes_.clear();
    allNodes_.push_back(header);
    rebuildVisible();

    isLibraryLoading_ = rootFolder_.isDirectory();
    repaint();

    if (!rootFolder_.isDirectory())
        return;

    const auto rootToScan = rootFolder_;
    juce::Component::SafePointer<Browser> safeThis(this);

    std::thread([safeThis, rootToScan, header, scanGeneration]()
    {
        std::vector<TreeNode> scanned;
        scanned.push_back(header);

        if (safeThis != nullptr)
        {
            juce::Array<juce::File> dirs = rootToScan.findChildFiles(
                juce::File::findDirectories, false, "*");
            std::sort(dirs.begin(), dirs.end(), [](const juce::File& a, const juce::File& b) {
                return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
            });

            for (auto& child : dirs)
            {
                TreeNode node;
                node.file = child;
                node.displayName = browserNodeDisplayName(child, true);
                node.depth = 1;
                node.isFolder = true;
                node.isAudio = false;
                node.isExpanded = false;
                scanned.push_back(std::move(node));
            }
        }

        // Phase 1 — show folders immediately (FL-style fast tree).
        juce::MessageManager::callAsync([safeThis, scanGeneration, scanned = std::move(scanned)]() mutable
        {
            if (safeThis == nullptr || scanGeneration != safeThis->libraryScanGeneration_.load())
                return;

            safeThis->allNodes_ = std::move(scanned);
            safeThis->isLibraryLoading_ = false;
            safeThis->selectedIdx_ = -1;
            safeThis->scrollY_ = 0;
            safeThis->rebuildVisible();
            safeThis->repaint();
        });

        if (safeThis == nullptr)
            return;

        juce::Array<juce::File> audioFiles;
        for (juce::DirectoryIterator it(rootToScan, false, "*", juce::File::findFiles); it.next();)
        {
            auto f = it.getFile();
            if (safeThis->isAudioFile(f))
                audioFiles.add(f);
        }
        std::sort(audioFiles.begin(), audioFiles.end(), [](const juce::File& a, const juce::File& b) {
            return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
        });

        constexpr int kBatch = 100;
        for (int i = 0; i < audioFiles.size(); i += kBatch)
        {
            std::vector<TreeNode> batch;
            const int end = juce::jmin(i + kBatch, audioFiles.size());
            batch.reserve((size_t)(end - i));
            for (int j = i; j < end; ++j)
            {
                TreeNode node;
                node.file = audioFiles[j];
                node.displayName = audioFiles[j].getFileNameWithoutExtension();
                node.depth = 1;
                node.isFolder = false;
                node.isAudio = true;
                node.isExpanded = false;
                batch.push_back(std::move(node));
            }

            juce::MessageManager::callAsync([safeThis, scanGeneration, batch = std::move(batch)]() mutable
            {
                if (safeThis == nullptr || scanGeneration != safeThis->libraryScanGeneration_.load())
                    return;

                safeThis->allNodes_.insert(safeThis->allNodes_.end(), batch.begin(), batch.end());
                safeThis->rebuildVisible();
                safeThis->repaint();
            });
        }
    }).detach();
}

int Browser::effectivePluginPanelH() const
{
    if (pluginPanelHidden_) return 0;
    // Clamp so the folder list keeps at least 80 px and the panel can't
    // exceed the window minus header.
    int maxPanel = juce::jmax(0, getHeight() - (ADMIN_H + BROWSER_HEAD_H) - 80);
    return juce::jlimit(0, maxPanel, pluginPanelH_);
}

int Browser::treeRowHeightForNode(const TreeNode& node) const
{
    const int base = treeItemH();
    if (!node.hasNfoSkin)
        return base;

    return juce::jlimit(base + 22, 82, juce::roundToInt(58.0f * treeScale_));
}

int Browser::treeContentHeight() const
{
    int total = 0;
    for (int idx : visibleIndices_)
        if (idx >= 0 && idx < (int)allNodes_.size())
            total += treeRowHeightForNode(allNodes_[(size_t)idx]);
    return total;
}

int Browser::visibleRowAtY(int yWithinContent) const
{
    int y = 0;
    for (int row = 0; row < (int)visibleIndices_.size(); ++row)
    {
        const int idx = visibleIndices_[(size_t)row];
        if (idx < 0 || idx >= (int)allNodes_.size())
            continue;

        y += treeRowHeightForNode(allNodes_[(size_t)idx]);
        if (yWithinContent < y)
            return row;
    }
    return -1;
}

juce::Rectangle<int> Browser::getDrumKitListRect() const
{
    int top = ADMIN_H + BROWSER_HEAD_H;
    int panelH = effectivePluginPanelH();
    int bottom = getHeight() - (panelH > 0 ? (panelH + DIVIDER_H) : 0);
    if (bottom < top + 50) bottom = top + 50;
    return juce::Rectangle<int>(0, top, getWidth(), bottom - top);
}

juce::Rectangle<int> Browser::getDividerRect() const
{
    auto list = getDrumKitListRect();
    return juce::Rectangle<int>(0, list.getBottom(), getWidth(), DIVIDER_H);
}

juce::Rectangle<int> Browser::getTabsRect() const
{
    auto div = getDividerRect();
    if (effectivePluginPanelH() <= 0) return juce::Rectangle<int>(0, getHeight(), getWidth(), 0);
    return juce::Rectangle<int>(0, div.getBottom(), getWidth(), TAB_H);
}

juce::Rectangle<int> Browser::getInstrumentsRect() const
{
    auto tabs = getTabsRect();
    if (tabs.isEmpty()) return juce::Rectangle<int>(0, getHeight(), getWidth(), 0);
    int top = tabs.getBottom() + SECTION_LABEL_H;
    return juce::Rectangle<int>(0, top, getWidth(), getHeight() - top);
}

void Browser::paint(juce::Graphics& g)
{
    int w = getWidth();
    int h = getHeight();

    // Background
    if (Theme::aeroMode)
        Theme::drawAeroPanel(g, getLocalBounds().toFloat());
    else
        g.fillAll(juce::Colour(0xff09090b));

    // Right border
    g.setColour(juce::Colours::black);
    g.drawVerticalLine(w - 1, 0.0f, (float)h);
    g.setColour(juce::Colour(0xff222226));
    g.drawVerticalLine(w - 2, 0.0f, (float)h);
    
    // (ADMIN header removed)
    
    // ── BROWSER header row ──────────────────────────────────────
    auto browseRect = juce::Rectangle<int>(0, ADMIN_H, w, BROWSER_HEAD_H);
    g.setColour(juce::Colour(0xff0d0d10));
    g.fillRect(browseRect);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(browseRect.getBottom() - 1, 0.0f, (float)w);
    
    if (!isSearching_)
    {
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
        g.drawText("BROWSER", 36, browseRect.getY(), 80, BROWSER_HEAD_H, juce::Justification::centredLeft);
        // Tiny magnifier hint to the right of the label so users know it's clickable
        g.setColour(Theme::zinc600);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
        g.drawText(juce::String::fromUTF8("\xf0\x9f\x94\x8d"),
                   100, browseRect.getY(), 16, BROWSER_HEAD_H, juce::Justification::centredLeft);
    }
    
    auto allFilter = getAllFilterRect().toFloat();
    juce::ColourGradient pillGrad(juce::Colour(0xff2a2a2e), 0.0f, allFilter.getY(),
                                    juce::Colour(0xff18181b), 0.0f, allFilter.getBottom(), false);
    g.setGradientFill(pillGrad);
    g.fillRoundedRectangle(allFilter, 3.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(allFilter, 3.0f, 1.0f);
    g.setColour(Theme::zinc300);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
    g.drawText(libraryLabel(),
               (int)allFilter.getX() + 4, (int)allFilter.getY(),
               (int)allFilter.getWidth() - 16, (int)allFilter.getHeight(),
               juce::Justification::centredLeft);
    juce::Path allArrow;
    float aax = allFilter.getRight() - 10;
    float aay = allFilter.getCentreY();
    allArrow.addTriangle(aax - 3, aay - 2, aax + 3, aay - 2, aax, aay + 2);
    g.setColour(Theme::zinc500);
    g.fillPath(allArrow);

    auto drawZoomButton = [&](juce::Rectangle<int> r, const juce::String& label)
    {
        auto rf = r.toFloat();
        juce::ColourGradient zg(juce::Colour(0xff27272a), rf.getX(), rf.getY(),
                                juce::Colour(0xff111114), rf.getX(), rf.getBottom(), false);
        g.setGradientFill(zg);
        g.fillRoundedRectangle(rf, 3.0f);
        g.setColour(juce::Colour(0xff3f3f46));
        g.drawRoundedRectangle(rf, 3.0f, 1.0f);
        g.setColour(Theme::orange1);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
        g.drawText(label, r, juce::Justification::centred);
    };
    drawZoomButton(getZoomMinusRect(), "-");
    drawZoomButton(getZoomPlusRect(), "+");
    
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f));
    g.drawText(juce::String::fromUTF8("\xe2\x96\xb6"),
               14, browseRect.getY(), 16, BROWSER_HEAD_H, juce::Justification::centredLeft);
    
    // ── Tree list ───────────────────────────────────────────────
    auto listRect = getDrumKitListRect();
    g.saveState();
    g.reduceClipRegion(listRect);
    
    int itemY = listRect.getY() - scrollY_;
    for (int v = 0; v < (int)visibleIndices_.size(); ++v)
    {
        int idx = visibleIndices_[v];
        const auto& n = allNodes_[idx];
        const int rowH = treeRowHeightForNode(n);
        
        if (itemY + rowH < listRect.getY()) { itemY += rowH; continue; }
        if (itemY > listRect.getBottom()) break;
        
        bool isHeader = (idx == 0);
        bool isSelected = (idx == selectedIdx_);
        bool isHovered  = (idx == hoverIdx_) && !isSelected && !isHeader;

        // Row background — FL Studio: soft hover tint, brighter selected bar
        // spanning the full width with a thin orange accent on the left edge.
        if (isHovered)
        {
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.fillRect(0, itemY, w, rowH);
        }
        if (isSelected && !isHeader)
        {
            juce::ColourGradient selG(juce::Colour(0xff34343c), 0.0f, (float)itemY,
                                      juce::Colour(0xff26262c), 0.0f, (float)itemY + rowH, false);
            g.setGradientFill(selG);
            g.fillRect(0, itemY, w, rowH);
            g.setColour(Theme::orange2);
            g.fillRect(0, itemY, 2, rowH);
        }

        // Indent (depth-based)
        int indent = juce::roundToInt(8.0f * treeScale_) + juce::roundToInt((float)n.depth * 12.0f * treeScale_);

        // FL Studio look: teal folder-with-arrow icon for folders, a small
        // colored file-type badge (mp3 / wav …) for audio files.
        const float gscale  = treeScale_;
        const float iconCy  = (float)itemY + rowH * 0.5f;
        const juce::Colour folderTeal = isSelected ? juce::Colour(0xffaef0db)
                                                   : juce::Colour(0xff79d6b8);
        int nameX = indent + juce::roundToInt(26.0f * gscale);

        if (n.isFolder)
        {
            // Folder glyph — open folder with a corner return-arrow (FL style).
            const float iw = juce::jlimit(11.0f, 18.0f, 15.0f * gscale);
            const float ih = iw * 0.72f;
            const float ix = (float)indent + 2.0f;
            const float iy = iconCy - ih * 0.5f;

            juce::Path folder;
            const float tab = iw * 0.42f;             // back flap width
            folder.startNewSubPath(ix, iy + ih);
            folder.lineTo(ix, iy + ih * 0.22f);
            folder.lineTo(ix + tab * 0.78f, iy + ih * 0.22f);
            folder.lineTo(ix + tab, iy + ih * 0.42f);
            folder.lineTo(ix + iw, iy + ih * 0.42f);
            folder.lineTo(ix + iw, iy + ih);
            folder.closeSubPath();

            g.setColour(folderTeal);
            g.strokePath(folder, juce::PathStrokeType(juce::jmax(1.0f, 1.3f * gscale)));

            // Little return-arrow in the top-right corner (FL signature mark).
            const float ax2 = ix + iw - iw * 0.30f;
            const float ay2 = iy + ih * 0.10f;
            juce::Path arr;
            arr.startNewSubPath(ax2, ay2 + ih * 0.16f);
            arr.lineTo(ax2 + iw * 0.22f, ay2);
            arr.lineTo(ax2 + iw * 0.22f, ay2 + ih * 0.32f);
            g.strokePath(arr, juce::PathStrokeType(juce::jmax(1.0f, 1.1f * gscale)));
        }
        else
        {
            // File-type badge, e.g. [mp3] / [wav].
            juce::String ext = n.file.getFileExtension().removeCharacters(".").toLowerCase();
            if (ext.isEmpty()) ext = "snd";
            if (ext.length() > 4) ext = ext.substring(0, 4);

            const float bh = juce::jlimit(11.0f, 16.0f, 13.0f * gscale);
            const float bw = juce::jlimit(18.0f, 26.0f, 22.0f * gscale);
            const float bx = (float)indent + 1.0f;
            const float by = iconCy - bh * 0.5f;
            juce::Rectangle<float> badge(bx, by, bw, bh);

            // Color by family — audio tan/orange, anything else slate.
            const bool isAud = n.isAudio || ext == "mp3" || ext == "wav"
                               || ext == "flac" || ext == "ogg" || ext == "aif"
                               || ext == "aiff" || ext == "m4a";
            juce::Colour badgeBg = isAud ? juce::Colour(0xffd9a441)
                                         : juce::Colour(0xff5a6172);
            if (isSelected) badgeBg = badgeBg.brighter(0.18f);
            g.setColour(badgeBg);
            g.fillRoundedRectangle(badge, 2.5f);
            g.setColour(juce::Colours::black.withAlpha(0.78f));
            g.setFont(juce::FontOptions().withName("Segoe UI")
                                          .withHeight(juce::jlimit(7.5f, 10.0f, 8.5f * gscale))
                                          .withStyle("Bold"));
            g.drawText(ext, badge, juce::Justification::centred);
            nameX = juce::roundToInt(bx + bw + 6.0f * gscale);
        }

        // Name — header & folders teal-ish, files light grey (FL palette).
        g.setColour(isHeader ? folderTeal
                             : (n.isFolder ? folderTeal : Theme::zinc300));
        if (isSelected) g.setColour(juce::Colours::white);
        else if (isHovered && !n.isFolder) g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI")
                                       .withHeight((isHeader ? 11.5f : 10.5f) * gscale)
                                       .withStyle((isHeader || n.isFolder) ? "Bold" : "Regular"));
        const int titleH = n.hasNfoSkin ? juce::roundToInt(20.0f * gscale) : rowH;
        g.drawText(n.displayName, nameX, itemY, w - nameX - 8, titleH, juce::Justification::centredLeft);

        if (n.hasNfoSkin)
        {
            const int skinY = itemY + titleH - juce::roundToInt(1.0f * treeScale_);
            const int skinH = rowH - titleH;
            const int skinX = nameX + juce::roundToInt(12.0f * treeScale_);
            const int skinW = w - skinX - 10;

            if (n.nfoBitmap.isValid())
            {
                g.drawImageWithin(n.nfoBitmap, skinX, skinY, skinW, skinH,
                                  juce::RectanglePlacement::xLeft
                                    | juce::RectanglePlacement::yMid
                                    | juce::RectanglePlacement::onlyReduceInSize,
                                  false);
            }
            else
            {
                g.setFont(juce::FontOptions().withName("Old English Text MT")
                                             .withHeight(juce::jlimit(16.0f, 34.0f, 25.0f * treeScale_))
                                             .withStyle("Bold"));
                g.setColour(juce::Colours::black.withAlpha(0.45f));
                g.drawText(n.nfoSkinText, skinX + 1, skinY + 1, skinW, skinH,
                           juce::Justification::centredLeft, true);
                g.setColour(n.nfoColour.withMultipliedSaturation(1.25f).brighter(0.2f));
                g.drawText(n.nfoSkinText, skinX, skinY, skinW, skinH,
                           juce::Justification::centredLeft, true);
            }
        }
        
        itemY += rowH;
    }
    
    if (allNodes_.size() <= 1)
    {
        g.setColour(Theme::zinc600);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
        if (isLibraryLoading_)
        {
            g.drawMultiLineText("Loading " + libraryLabel().toLowerCase() + "...",
                                12, listRect.getY() + 50, w - 24);
        }
        else
        {
            g.drawMultiLineText(libraryLabel().toLowerCase() + " folder not found.\nSet path in Browser.cpp",
                                12, listRect.getY() + 50, w - 24);
        }
    }
    g.restoreState();
    
    // ── Resize divider ─────────────────────────────────────────
    {
        auto div = getDividerRect();
        if (!div.isEmpty())
        {
            g.setColour(juce::Colour(0xff09090b));
            g.fillRect(div);
            g.setColour(juce::Colours::black);
            g.drawHorizontalLine(div.getY(), 0.0f, (float)w);
            g.drawHorizontalLine(div.getBottom() - 1, 0.0f, (float)w);
            // Grip dots
            g.setColour(draggingDivider_ ? Theme::orange1 : juce::Colour(0xff52525b));
            int cx = w / 2;
            int cy = div.getCentreY();
            for (int i = -2; i <= 2; ++i)
                g.fillEllipse((float)(cx + i * 8 - 1), (float)(cy - 1), 2.0f, 2.0f);
        }
    }

    // If the panel is fully collapsed, we're done.
    if (effectivePluginPanelH() <= 0) return;

    // ── Single PLUGINS header (PLUGINS label + BROWSE... button) ──
    auto tabsRect = getTabsRect();
    g.setColour(juce::Colour(0xff0a0a0c));
    g.fillRect(tabsRect);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(tabsRect.getY(), 0.0f, (float)w);

    // Orange bar = "PLUGINS" title
    auto titleR = juce::Rectangle<float>(4, (float)tabsRect.getY() + 4, (float)w - 84, (float)TAB_H - 8);
    juce::ColourGradient tGrad(Theme::orange1, 0.0f, titleR.getY(),
                               Theme::orange3, 0.0f, titleR.getBottom(), false);
    g.setGradientFill(tGrad);
    g.fillRoundedRectangle(titleR, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawHorizontalLine((int)titleR.getY() + 1, titleR.getX() + 4, titleR.getRight() - 4);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(titleR, 3.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
    g.drawText("PLUGINS", titleR.toNearestInt(), juce::Justification::centred);

    // Zinc bar = "BROWSE..." opens file picker for an unscanned .vst3/.dll
    auto browseR = juce::Rectangle<float>((float)w - 76, titleR.getY(), 72.0f, titleR.getHeight());
    juce::ColourGradient bGrad(juce::Colour(0xff2a2a2e), 0.0f, browseR.getY(),
                               juce::Colour(0xff18181b), 0.0f, browseR.getBottom(), false);
    g.setGradientFill(bGrad);
    g.fillRoundedRectangle(browseR, 3.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(browseR, 3.0f, 1.0f);
    g.setColour(Theme::zinc300);
    g.drawText("BROWSE...", browseR.toNearestInt(), juce::Justification::centred);
    
    // ── Section Label ───────────────────────────────────────────
    auto labelRect = juce::Rectangle<int>(0, tabsRect.getBottom(), w, SECTION_LABEL_H);
    g.setColour(Theme::zinc600);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
    g.drawText(juce::String(instruments_.size()) + " INSTALLED PLUGINS — CLICK",
               12, labelRect.getY() + 2, w - 24, 12, juce::Justification::centredLeft);
    g.drawText("LOAD TO ATTACH TO SELECTED TRACK",
               12, labelRect.getY() + 12, w - 24, 12, juce::Justification::centredLeft);
    
    // ── Instruments list (clipped + scrolled) ────────────────────
    auto instrRect = getInstrumentsRect();
    g.saveState();
    g.reduceClipRegion(instrRect);

    int totalH    = (int)instruments_.size() * INSTR_H;
    int maxScroll = juce::jmax(0, totalH - instrRect.getHeight());
    pluginScrollY_ = juce::jlimit(0, maxScroll, pluginScrollY_);

    int iy = instrRect.getY() - pluginScrollY_;
    for (size_t i = 0; i < instruments_.size(); ++i)
    {
        // Skip rows entirely outside the viewport for speed.
        if (iy + INSTR_H < instrRect.getY()) { iy += INSTR_H; continue; }
        if (iy >= instrRect.getBottom()) break;

        auto& ins = instruments_[i];
        auto rowRect = juce::Rectangle<int>(0, iy, w, INSTR_H);

        g.setColour(juce::Colour(0xff141417));
        g.drawHorizontalLine(rowRect.getBottom() - 1, 0.0f, (float)w);

        auto dot = juce::Rectangle<float>(14, (float)rowRect.getCentreY() - 3, 6, 6);
        Theme::drawGlowLED(g, dot, Theme::orange2, true);

        g.setColour(Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f));
        g.drawText(ins.name, 28, iy, w - 90, INSTR_H, juce::Justification::centredLeft);

        auto loadBtn = juce::Rectangle<float>((float)w - 50, (float)iy + 7, 42, 18);
        juce::ColourGradient loadGrad(Theme::orange1, 0.0f, loadBtn.getY(),
                                        Theme::orange3, 0.0f, loadBtn.getBottom(), false);
        g.setGradientFill(loadGrad);
        g.fillRoundedRectangle(loadBtn, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawHorizontalLine((int)loadBtn.getY() + 1, loadBtn.getX() + 3, loadBtn.getRight() - 3);
        g.setColour(juce::Colour(0xff431407));
        g.drawRoundedRectangle(loadBtn, 3.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
        g.drawText("Load", loadBtn.toNearestInt(), juce::Justification::centred);

        iy += INSTR_H;
    }
    g.restoreState();
}

void Browser::resized()
{
    if (searchEditor_)
        searchEditor_->setBounds(getSearchEditorRect());
}

juce::Rectangle<int> Browser::getSearchEditorRect() const
{
    // Sits inside the BROWSER header row, leaving room for the ALL pill on the right
    return juce::Rectangle<int>(34, ADMIN_H + 4, getWidth() - 34 - 92, BROWSER_HEAD_H - 8);
}

void Browser::startSearch()
{
    if (isSearching_) return;
    isSearching_ = true;
    savedTree_   = allNodes_;
    if (searchEditor_)
    {
        searchEditor_->setVisible(true);
        searchEditor_->setBounds(getSearchEditorRect());
        searchEditor_->setText({}, juce::dontSendNotification);
        searchEditor_->grabKeyboardFocus();
    }
    repaint();
}

void Browser::endSearch()
{
    if (!isSearching_) return;
    isSearching_ = false;
    searchQuery_ = {};
    if (searchEditor_)
    {
        searchEditor_->setText({}, juce::dontSendNotification);
        searchEditor_->setVisible(false);
    }
    if (!savedTree_.empty())
    {
        allNodes_ = std::move(savedTree_);
        savedTree_.clear();
    }
    rebuildVisible();
    scrollY_ = 0;
    repaint();
}

void Browser::collectFoldersRecursive(const juce::File& folder, const juce::String& q,
                                      std::vector<TreeNode>& out, int maxDepth, int curDepth)
{
    if (!folder.isDirectory() || curDepth > maxDepth) return;
    juce::Array<juce::File> kids = folder.findChildFiles(juce::File::findDirectories, false, "*");
    for (auto& k : kids)
    {
        if (k.getFileName().containsIgnoreCase(q))
        {
            TreeNode n;
            n.file        = k;
            n.displayName = browserNodeDisplayName(k, true);
            n.depth       = 0;       // flat list while searching
            n.isFolder    = true;
            n.isExpanded  = false;
            out.push_back(n);
        }
        collectFoldersRecursive(k, q, out, maxDepth, curDepth + 1);
    }
}

void Browser::performSearch(const juce::String& query)
{
    searchQuery_ = query.trim();
    allNodes_.clear();

    TreeNode header;
    header.depth      = 0;
    header.isFolder   = true;
    header.isExpanded = true;
    if (searchQuery_.isEmpty())
    {
        // Empty query: show full tree again (without exiting search mode).
        if (!savedTree_.empty()) allNodes_ = savedTree_;
        rebuildVisible();
        scrollY_ = 0;
        repaint();
        return;
    }

    header.displayName = "Folders matching \"" + searchQuery_ + "\"";
    header.file        = rootFolder_;
    allNodes_.push_back(header);

    if (rootFolder_.isDirectory())
    {
        std::vector<TreeNode> hits;
        collectFoldersRecursive(rootFolder_, searchQuery_, hits, /*maxDepth*/ 6, 1);
        // Promote depth so they nest under the synthetic header.
        for (auto& h : hits) { h.depth = 1; allNodes_.push_back(h); }
    }
    rebuildVisible();
    scrollY_ = 0;
    repaint();
}

void Browser::mouseDown(const juce::MouseEvent& e)
{
    if (getZoomMinusRect().contains(e.x, e.y))
    {
        treeScale_ = juce::jlimit(0.75f, 1.6f, treeScale_ - 0.1f);
        savePanelHeight();
        repaint();
        return;
    }

    if (getZoomPlusRect().contains(e.x, e.y))
    {
        treeScale_ = juce::jlimit(0.75f, 1.6f, treeScale_ + 0.1f);
        savePanelHeight();
        repaint();
        return;
    }

    // ── Resize divider drag start ──
    if (getDividerRect().contains(e.x, e.y))
    {
        draggingDivider_  = true;
        dragStartY_       = e.y;
        dragStartPanelH_  = effectivePluginPanelH();
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        repaint();
        return;
    }

    // ── Library selector pill (top-right "DRUMS / LOOPS / ACAPELLA / ALL") ──
    if (getAllFilterRect().contains(e.x, e.y))
    {
        juce::PopupMenu menu;
        menu.addSectionHeader("Library");
        menu.addItem(1, "Drum Kits", true, currentLibrary_ == Library::Drums);
        menu.addItem(2, "Loops",     true, currentLibrary_ == Library::Loops);
        menu.addItem(3, "Acapella",  true, currentLibrary_ == Library::Acapella);
        menu.addItem(4, "Tags",      true, currentLibrary_ == Library::Tags);
        menu.addItem(5, "All",       true, currentLibrary_ == Library::All);
        menu.addItem(6, "Marketplace", true, currentLibrary_ == Library::Marketplace);
        menu.addSeparator();
        menu.addItem(7, "Open Marketplace Store...");
        menu.addSeparator();
        menu.addItem(8, "Set Library Folders…");

        menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetComponent(this)
            .withTargetScreenArea(localAreaToGlobal(getAllFilterRect())),
            [this](int result) {
                if (result == 1) setLibrary(Library::Drums);
                else if (result == 2) setLibrary(Library::Loops);
                else if (result == 3) setLibrary(Library::Acapella);
                else if (result == 4) setLibrary(Library::Tags);
                else if (result == 5) setLibrary(Library::All);
                else if (result == 6) setLibrary(Library::Marketplace);
                else if (result == 7 && onOpenMarketplace) onOpenMarketplace();
                else if (result == 8) showSettingsDialog();
            });
        return;
    }

    // Click on the BROWSER header (left of the ALL pill) → open folder-search field
    juce::Rectangle<int> headerClick(0, ADMIN_H, 112, BROWSER_HEAD_H);
    if (headerClick.contains(e.x, e.y))
    {
        if (!isSearching_)
        {
            startSearch();
            return;
        }
        // Already in search mode → leave click to the editor
    }

    auto listRect = getDrumKitListRect();

    // Right-click context menu on any tree row (folder or file)
    if (e.mods.isRightButtonDown() && listRect.contains(e.x, e.y))
    {
        int relY = e.y - listRect.getY() + scrollY_;
        int row = visibleRowAtY(relY);
        if (row >= 0 && row < (int)visibleIndices_.size())
        {
            int idx = visibleIndices_[row];
            if (idx < 0 || idx >= (int)allNodes_.size()) return;
            selectedIdx_ = idx;
            repaint();

            const juce::File target = allNodes_[idx].file;
            if (!target.exists()) return;

            juce::PopupMenu menu;
            menu.addItem(1, "Copy Path");
            menu.addItem(2, "Copy Name");
            menu.addSeparator();
            menu.addItem(3, target.isDirectory() ? "Open Folder in Explorer"
                                                  : "Reveal in Explorer");

            menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetComponent(this),
                [target](int result)
                {
                    if (result == 1)
                        juce::SystemClipboard::copyTextToClipboard(target.getFullPathName());
                    else if (result == 2)
                        juce::SystemClipboard::copyTextToClipboard(target.getFileName());
                    else if (result == 3)
                        target.revealToUser();
                });
        }
        return;
    }

    if (listRect.contains(e.x, e.y))
    {
        int relY = e.y - listRect.getY() + scrollY_;
        int row = visibleRowAtY(relY);
        if (row >= 0 && row < (int)visibleIndices_.size())
        {
            int idx = visibleIndices_[row];
            selectedIdx_ = idx;
            auto& n = allNodes_[idx];
            
            if (n.isFolder)
            {
                if (idx == 0)
                {
                    // Don't toggle root header
                    repaint();
                    return;
                }
                if (n.virtualNfoFolder)
                {
                    repaint();
                    return;
                }
                
                // Lazy-load children on first expansion
                if (!n.isExpanded)
                {
                    // Check if any children are already in allNodes_ at depth+1
                    bool hasChildren = false;
                    for (int j = idx + 1; j < (int)allNodes_.size(); ++j)
                    {
                        if (allNodes_[j].depth <= n.depth) break;
                        if (allNodes_[j].depth == n.depth + 1) { hasChildren = true; break; }
                    }
                    
                    if (!hasChildren && n.file.isDirectory())
                    {
                        allNodes_[idx].isExpanded = true;
                        rebuildVisible();
                        loadFolderChildrenAsync(idx);
                        repaint();
                        return;
                    }
                }

                allNodes_[idx].isExpanded = !allNodes_[idx].isExpanded;
                rebuildVisible();
            }
            else if (n.isAudio)
            {
                // Fade the current preview before starting the next one so
                // rapid browsing does not hard-cut non-zero sample data.
                pluginHost_.playSamplePreview(n.file);
                // Arm drag-to-channel-rack
                pendingDragFile_ = n.file;
                pendingDragPayload_ = {};
                pendingDragLabel_ = {};
                dragStarted_ = false;
            }
            
            repaint();
        }
        return;
    }
    
    // BROWSE... button on the right of the PLUGINS header → file picker.
    auto tabsRect = getTabsRect();
    if (tabsRect.contains(e.x, e.y))
    {
        if (e.x >= getWidth() - 76 && onLoadVstPicker)
            onLoadVstPicker();
        return;
    }

    // ── Load button hit-test in the instruments list ──
    auto instrRect = getInstrumentsRect();
    if (instrRect.contains(e.x, e.y))
    {
        int w = getWidth();
        int rowIdx = (e.y - instrRect.getY() + pluginScrollY_) / INSTR_H;
        int rowTop = instrRect.getY() + rowIdx * INSTR_H - pluginScrollY_;
        juce::Rectangle<int> loadBtn(w - 50, rowTop + 7, 42, 18);
        if (loadBtn.contains(e.x, e.y))
        {
            if (rowIdx >= 0 && rowIdx < (int)instruments_.size() && onLoadPlugin)
                onLoadPlugin(instruments_[rowIdx].name,
                             instruments_[rowIdx].fileOrIdentifier);
            return;
        }

        if (rowIdx >= 0 && rowIdx < (int)instruments_.size())
        {
            const auto& ins = instruments_[(size_t)rowIdx];
            pendingDragFile_ = juce::File();
            pendingDragLabel_ = ins.name;
            pendingDragPayload_ = "plugin\n" + ins.name + "\n" + ins.fileOrIdentifier;
            dragStarted_ = false;
            repaint();
            return;
        }
    }
}

void Browser::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingDivider_)
    {
        int delta = e.y - dragStartY_;
        // Dragging DOWN shrinks the plugin panel → larger folder area.
        pluginPanelH_ = juce::jmax(0, dragStartPanelH_ - delta);
        // Snap fully closed under 18 px so it feels intentional.
        if (pluginPanelH_ < 18) pluginPanelH_ = 0;
        pluginScrollY_ = 0;
        repaint();
        return;
    }

    if (dragStarted_) return;
    const bool draggingPlugin = pendingDragPayload_.startsWith("plugin\n");
    if (!draggingPlugin && !pendingDragFile_.existsAsFile()) return;
    if (e.getDistanceFromDragStart() < 6) return;
    
    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor(this))
    {
        dragStarted_ = true;
        
        // Create a custom drag image showing the filename
        juce::String fileName = draggingPlugin ? pendingDragLabel_ : pendingDragFile_.getFileName();
        juce::Font dragFont(14.0f);
        int textW = juce::GlyphArrangement::getStringWidthInt(dragFont, fileName);
        int imageW = textW + 40;
        int imageH = 32;
        
        juce::Image dragImage(juce::Image::ARGB, imageW, imageH, true);
        juce::Graphics g(dragImage);
        
        // Draw background
        g.setColour(juce::Colours::darkgrey.withAlpha(0.9f));
        g.fillRoundedRectangle(0, 0, imageW, imageH, 4.0f);
        
        // Draw border
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawRoundedRectangle(0, 0, imageW, imageH, 4.0f, 1.0f);
        
        // Draw audio icon
        g.setColour(draggingPlugin ? Theme::accent : juce::Colours::orange);
        g.fillEllipse(8, 8, 16, 16);
        g.setColour(juce::Colours::white);
        g.drawEllipse(8, 8, 16, 16, 1.5f);
        
        // Draw filename
        g.setColour(juce::Colours::white);
        g.setFont(dragFont);
        g.drawText(fileName, 32, 0, textW + 8, imageH, juce::Justification::centredLeft, true);
        
        juce::String dragDescription = pendingDragPayload_;
        if (!draggingPlugin)
        {
            dragDescription = "audio\n";
            dragDescription << libraryLabel() << "\n" << pendingDragFile_.getFullPathName();
            keepChordifyWindowsVisibleForDrag();
        }

        dnd->startDragging(dragDescription, this, dragImage);
    }
}

void Browser::mouseUp(const juce::MouseEvent&)
{
    if (draggingDivider_)
    {
        draggingDivider_ = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
        savePanelHeight();
    }
    pendingDragFile_ = juce::File{};
    pendingDragPayload_ = {};
    pendingDragLabel_ = {};
    dragStarted_ = false;
}

juce::File Browser::panelStateFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
              .getChildFile ("Stratum DAW")
              .getChildFile ("browser.state");
}

void Browser::savePanelHeight() const
{
    auto f = panelStateFile();
    f.getParentDirectory().createDirectory();
    // Line 1 = plugin panel height, Line 2 = tree zoom scale
    f.replaceWithText(juce::String(pluginPanelH_) + "\n" + juce::String(treeScale_, 2));
}

void Browser::restorePanelHeight()
{
    auto f = panelStateFile();
    if (! f.existsAsFile()) return;
    auto lines = juce::StringArray::fromLines(f.loadFileAsString());
    if (lines.size() >= 1)
    {
        int saved = lines[0].getIntValue();
        if (saved >= 0 && saved < 4000)
            pluginPanelH_ = saved;
    }
    if (lines.size() >= 2)
    {
        float zoom = lines[1].getFloatValue();
        if (zoom >= 0.75f && zoom <= 1.6f)
            treeScale_ = zoom;
    }
}

void Browser::mouseMove(const juce::MouseEvent& e)
{
    if (getDividerRect().contains(e.x, e.y))
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);

    // Track hovered tree row for an FL-style highlight.
    int newHover = -1;
    auto listRect = getDrumKitListRect();
    if (listRect.contains(e.x, e.y))
    {
        const int relY = e.y - listRect.getY() + scrollY_;
        const int row = visibleRowAtY(relY);
        if (row >= 0 && row < (int)visibleIndices_.size())
            newHover = visibleIndices_[(size_t)row];
    }
    if (newHover != hoverIdx_)
    {
        hoverIdx_ = newHover;
        repaint();
    }
}

void Browser::mouseExit(const juce::MouseEvent&)
{
    if (hoverIdx_ != -1)
    {
        hoverIdx_ = -1;
        repaint();
    }
}

void Browser::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto listRect = getDrumKitListRect();
    if (listRect.contains(e.x, e.y))
    {
        // Scale to ~3 rows per notch on Windows (deltaY ≈ 0.15 per notch).
        scrollY_ -= (int)(wheel.deltaY * (float)(treeItemH() * 20));
        int total = treeContentHeight();
        int maxScroll = std::max(0, total - listRect.getHeight());
        scrollY_ = juce::jlimit(0, maxScroll, scrollY_);
        repaint();
        return;
    }

    // Scroll inside the plugin list.
    auto instrRect = getInstrumentsRect();
    if (instrRect.contains(e.x, e.y))
    {
        pluginScrollY_ -= (int)(wheel.deltaY * (float)(INSTR_H * 12));
        int total = (int)instruments_.size() * INSTR_H;
        int maxScroll = std::max(0, total - instrRect.getHeight());
        pluginScrollY_ = juce::jlimit(0, maxScroll, pluginScrollY_);
        repaint();
    }
}

void Browser::refreshPluginList()
{
    instruments_.clear();

    auto bundledPiano = getBundledStratumPianoVst3ForBrowser();
    if (bundledPiano.exists())
        instruments_.push_back({ "Stratum Piano", "VST3 inst", bundledPiano.getFullPathName() });
    auto bundledGuitar = getBundledStratumGuitarVst3ForBrowser();
    if (bundledGuitar.exists())
        instruments_.push_back({ "Stratum Guitar", "VST3 inst", bundledGuitar.getFullPathName() });

    // Cheap on subsequent calls — only freshly-added paths are deep-scanned.
    auto types = pluginHost_.getKnownPluginList().getTypes();
    std::sort(types.begin(), types.end(),
              [](const juce::PluginDescription& a, const juce::PluginDescription& b)
              { return a.name.compareIgnoreCase(b.name) < 0; });

    // Instruments first, then effects (FL-style ordering)
    for (const auto& d : types)
        if (d.isInstrument && d.name != "Stratum Piano" && d.name != "Stratum Guitar")
            instruments_.push_back({ d.name, d.pluginFormatName + " inst",   d.fileOrIdentifier });
    for (const auto& d : types)
        if (!d.isInstrument)
            instruments_.push_back({ d.name, d.pluginFormatName + " effect", d.fileOrIdentifier });

    repaint();
}
