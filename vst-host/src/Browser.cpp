#include "Browser.h"
#include "PluginHost.h"
#include "Theme.h"

Browser::Browser(PluginHost& pluginHost)
    : pluginHost_(pluginHost)
{
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

bool Browser::isAudioFile(const juce::File& f) const
{
    auto ext = f.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".ogg" 
        || ext == ".aiff" || ext == ".aif";
}

void Browser::scanFolder(const juce::File& folder, int depth)
{
    if (!folder.isDirectory()) return;
    
    juce::Array<juce::File> children = folder.findChildFiles(
        juce::File::findFilesAndDirectories, false, "*");
    
    // Sort: folders first, then files; both alphabetical
    std::sort(children.begin(), children.end(), [](const juce::File& a, const juce::File& b) {
        bool ad = a.isDirectory(), bd = b.isDirectory();
        if (ad != bd) return ad;
        return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
    });
    
    for (auto& child : children)
    {
        TreeNode node;
        node.file = child;
        node.displayName = child.getFileName();
        node.depth = depth;
        node.isFolder = child.isDirectory();
        node.isAudio = !node.isFolder && isAudioFile(child);
        node.isExpanded = false;
        
        // Skip irrelevant files
        if (!node.isFolder && !node.isAudio) continue;
        
        allNodes_.push_back(node);
    }
}

void Browser::buildTree()
{
    allNodes_.clear();
    
    auto headerName = (currentLibrary_ == Library::Loops) ? juce::String("Loops")
                    : (currentLibrary_ == Library::All)   ? juce::String("All Libraries")
                                                          : juce::String("Drum Kit");

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
    return juce::Rectangle<int>(getWidth() - 64, top, 56, 18);
}

juce::String Browser::libraryLabel() const
{
    switch (currentLibrary_)
    {
        case Library::Drums: return "DRUMS";
        case Library::Loops: return "LOOPS";
        case Library::All:   return "ALL";
    }
    return "ALL";
}

void Browser::setLibrary(Library lib)
{
    currentLibrary_ = lib;

    juce::Array<juce::File> candidates;
    switch (lib)
    {
        case Library::Drums:
            candidates = {
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
            };
            break;
        case Library::All:
            // "All" = whichever drive has both libraries' parent. Pick the
            // common root if it exists, else fall back to current.
            candidates = {
                juce::File("F:\\"),
                juce::File("E:\\"),
            };
            break;
    }

    rootFolder_ = juce::File();
    for (auto& c : candidates)
        if (c.isDirectory()) { rootFolder_ = c; break; }

    selectedIdx_ = -1;
    scrollY_     = 0;
    buildTree();
    repaint();
}

int Browser::effectivePluginPanelH() const
{
    // Clamp so the folder list keeps at least 80 px and the panel can't
    // exceed the window minus header.
    int maxPanel = juce::jmax(0, getHeight() - (ADMIN_H + BROWSER_HEAD_H) - 80);
    return juce::jlimit(0, maxPanel, pluginPanelH_);
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
    
    // Background (very dark)
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
    
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f));
    g.drawText("\xe2\x96\xb6", 14, browseRect.getY(), 16, BROWSER_HEAD_H, juce::Justification::centredLeft);
    
    // ── Tree list ───────────────────────────────────────────────
    auto listRect = getDrumKitListRect();
    g.saveState();
    g.reduceClipRegion(listRect);
    
    int itemY = listRect.getY() - scrollY_;
    for (int v = 0; v < (int)visibleIndices_.size(); ++v)
    {
        int idx = visibleIndices_[v];
        const auto& n = allNodes_[idx];
        
        if (itemY + ITEM_H < listRect.getY()) { itemY += ITEM_H; continue; }
        if (itemY > listRect.getBottom()) break;
        
        bool isHeader = (idx == 0);
        bool isSelected = (idx == selectedIdx_);
        
        // Selected background
        if (isSelected && !isHeader)
        {
            g.setColour(juce::Colour(0xff1f1f23));
            g.fillRect(0, itemY, w, ITEM_H);
            g.setColour(Theme::orange2);
            g.fillRect(0, itemY, 2, ITEM_H);
        }
        
        // Indent (depth-based)
        int indent = 8 + n.depth * 12;
        
        // Triangle / dot bullet
        g.setColour(Theme::orange2);
        if (n.isFolder)
        {
            juce::Path tri;
            if (n.isExpanded)
                tri.addTriangle((float)indent + 2, (float)itemY + 7, (float)indent + 10, (float)itemY + 7, (float)indent + 6, (float)itemY + 13);
            else
                tri.addTriangle((float)indent + 4, (float)itemY + 6, (float)indent + 4, (float)itemY + 14, (float)indent + 10, (float)itemY + 10);
            g.fillPath(tri);
        }
        else
        {
            // Audio file dot
            g.fillEllipse((float)indent + 4, (float)itemY + 9, 4, 4);
        }
        
        // Name
        g.setColour(isHeader ? Theme::zinc100 
                              : (n.isFolder ? Theme::zinc300 : Theme::zinc400));
        if (isSelected) g.setColour(Theme::orange1);
        g.setFont(juce::FontOptions().withName("Segoe UI")
                                       .withHeight(isHeader ? 11.0f : 10.0f)
                                       .withStyle((isHeader || n.isFolder) ? "Bold" : "Regular"));
        int nameX = indent + 16;
        g.drawText(n.displayName, nameX, itemY, w - nameX - 8, ITEM_H, juce::Justification::centredLeft);
        
        itemY += ITEM_H;
    }
    
    if (allNodes_.size() <= 1)
    {
        // Empty state
        g.setColour(Theme::zinc600);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
        g.drawMultiLineText("Drum kit folder not found.\nSet path in Browser.cpp",
                             12, listRect.getY() + 50, w - 24);
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
    return juce::Rectangle<int>(34, ADMIN_H + 4, getWidth() - 34 - 64, BROWSER_HEAD_H - 8);
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
            n.displayName = k.getFileName();
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

    // ── Library selector pill (top-right "DRUMS / LOOPS / ALL") ──
    if (getAllFilterRect().contains(e.x, e.y))
    {
        juce::PopupMenu menu;
        menu.addSectionHeader("Library");
        menu.addItem(1, "Drum Kits", true, currentLibrary_ == Library::Drums);
        menu.addItem(2, "Loops",     true, currentLibrary_ == Library::Loops);
        menu.addItem(3, "All",       true, currentLibrary_ == Library::All);

        menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetComponent(this)
            .withTargetScreenArea(localAreaToGlobal(getAllFilterRect())),
            [this](int result) {
                if (result == 1) setLibrary(Library::Drums);
                else if (result == 2) setLibrary(Library::Loops);
                else if (result == 3) setLibrary(Library::All);
            });
        return;
    }

    // Click on the BROWSER header (left of the ALL pill) → open folder-search field
    juce::Rectangle<int> headerClick(0, ADMIN_H, getWidth() - 64, BROWSER_HEAD_H);
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
        int row = relY / ITEM_H;
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
        int row = relY / ITEM_H;
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
                        // Insert children right after this node
                        std::vector<TreeNode> kids;
                        juce::Array<juce::File> files = n.file.findChildFiles(
                            juce::File::findFilesAndDirectories, false, "*");
                        std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b) {
                            bool ad = a.isDirectory(), bd = b.isDirectory();
                            if (ad != bd) return ad;
                            return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
                        });
                        for (auto& f : files)
                        {
                            TreeNode k;
                            k.file = f;
                            k.displayName = f.getFileName();
                            k.depth = n.depth + 1;
                            k.isFolder = f.isDirectory();
                            k.isAudio = !k.isFolder && isAudioFile(f);
                            if (!k.isFolder && !k.isAudio) continue;
                            kids.push_back(k);
                        }
                        allNodes_.insert(allNodes_.begin() + idx + 1, kids.begin(), kids.end());
                    }
                }
                
                allNodes_[idx].isExpanded = !allNodes_[idx].isExpanded;
                rebuildVisible();
            }
            else if (n.isAudio)
            {
                // Stop any previous preview first so clicking through samples
                // plays one at a time instead of stacking on top of each other.
                pluginHost_.stopSamplePlayback();
                pluginHost_.playSampleFile(n.file);
                // Arm drag-to-channel-rack
                pendingDragFile_ = n.file;
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
    if (!pendingDragFile_.existsAsFile()) return;
    if (e.getDistanceFromDragStart() < 6) return;
    
    if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor(this))
    {
        dragStarted_ = true;
        
        // Create a custom drag image showing the filename
        juce::String fileName = pendingDragFile_.getFileName();
        juce::Font dragFont(14.0f);
        int textW = dragFont.getStringWidth(fileName);
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
        g.setColour(juce::Colours::orange);
        g.fillEllipse(8, 8, 16, 16);
        g.setColour(juce::Colours::white);
        g.drawEllipse(8, 8, 16, 16, 1.5f);
        
        // Draw filename
        g.setColour(juce::Colours::white);
        g.setFont(dragFont);
        g.drawText(fileName, 32, 0, textW + 8, imageH, juce::Justification::centredLeft, true);
        
        dnd->startDragging(pendingDragFile_.getFullPathName(), this, dragImage);
    }
}

void Browser::mouseUp(const juce::MouseEvent&)
{
    if (draggingDivider_)
    {
        draggingDivider_ = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
    pendingDragFile_ = juce::File{};
    dragStarted_ = false;
}

void Browser::mouseMove(const juce::MouseEvent& e)
{
    if (getDividerRect().contains(e.x, e.y))
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void Browser::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto listRect = getDrumKitListRect();
    if (listRect.contains(e.x, e.y))
    {
        // Scale to ~3 rows per notch on Windows (deltaY ≈ 0.15 per notch).
        scrollY_ -= (int)(wheel.deltaY * (float)(ITEM_H * 20));
        int total = (int)visibleIndices_.size() * ITEM_H;
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

    // Cheap on subsequent calls — only freshly-added paths are deep-scanned.
    pluginHost_.scanDefaultLocations();

    auto types = pluginHost_.getKnownPluginList().getTypes();
    std::sort(types.begin(), types.end(),
              [](const juce::PluginDescription& a, const juce::PluginDescription& b)
              { return a.name.compareIgnoreCase(b.name) < 0; });

    // Instruments first, then effects (FL-style ordering)
    for (const auto& d : types)
        if (d.isInstrument)
            instruments_.push_back({ d.name, d.pluginFormatName + " inst",   d.fileOrIdentifier });
    for (const auto& d : types)
        if (!d.isInstrument)
            instruments_.push_back({ d.name, d.pluginFormatName + " effect", d.fileOrIdentifier });

    repaint();
}
