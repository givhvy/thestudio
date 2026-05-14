#include "Browser.h"
#include "PluginHost.h"
#include "Theme.h"

Browser::Browser(PluginHost& pluginHost)
    : pluginHost_(pluginHost)
{
    instruments_ = {
        { "PolySaw (Analog Lead)", "Synth" },
        { "FM Synth (DX-style)", "Synth" },
        { "Sub Bass", "Bass" },
        { "Lush Pad", "Pad" },
        { "Pluck / Karplus-Strong", "Pluck" },
        { "Stereo Delay (FX)", "Delay" },
    };
    
    // Try to find drum kit folder
    juce::Array<juce::File> candidates = {
        juce::File("E:\\!Storage\\1500 THE DRUMS LORD COLLECTION"),
        juce::File("E:\\Storage\\1500 THE DRUMS LORD COLLECTION"),
        juce::File("E:\\1500 THE DRUMS LORD COLLECTION"),
    };
    for (auto& c : candidates)
    {
        if (c.isDirectory()) { rootFolder_ = c; break; }
    }
    
    buildTree();

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
    
    // Root header (always shown, expanded)
    TreeNode header;
    header.displayName = rootFolder_.exists() ? "Drum Kit" : "Drum Kit (folder not found)";
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

juce::Rectangle<int> Browser::getDrumKitListRect() const
{
    int top = ADMIN_H + BROWSER_HEAD_H;
    int bottom = getHeight() - TAB_H - SECTION_LABEL_H - (int)instruments_.size() * INSTR_H;
    if (bottom < top + 50) bottom = top + 50;
    return juce::Rectangle<int>(0, top, getWidth(), bottom - top);
}

juce::Rectangle<int> Browser::getTabsRect() const
{
    auto list = getDrumKitListRect();
    return juce::Rectangle<int>(0, list.getBottom(), getWidth(), TAB_H);
}

juce::Rectangle<int> Browser::getInstrumentsRect() const
{
    auto tabs = getTabsRect();
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
    
    auto allFilter = juce::Rectangle<float>((float)w - 56, (float)browseRect.getY() + 5, 48, 18);
    juce::ColourGradient pillGrad(juce::Colour(0xff2a2a2e), 0.0f, allFilter.getY(),
                                    juce::Colour(0xff18181b), 0.0f, allFilter.getBottom(), false);
    g.setGradientFill(pillGrad);
    g.fillRoundedRectangle(allFilter, 3.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(allFilter, 3.0f, 1.0f);
    g.setColour(Theme::zinc300);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f));
    g.drawText("ALL", (int)allFilter.getX(), (int)allFilter.getY(), 24, 18, juce::Justification::centred);
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
    
    // ── Tabs (WASM PLUGINS / VST/DLL) ───────────────────────────
    auto tabsRect = getTabsRect();
    g.setColour(juce::Colour(0xff0a0a0c));
    g.fillRect(tabsRect);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(tabsRect.getY(), 0.0f, (float)w);
    
    int tabW = w / 2;
    auto tab1 = juce::Rectangle<float>(0, (float)tabsRect.getY() + 4, (float)tabW - 2, (float)TAB_H - 8);
    auto tab2 = juce::Rectangle<float>((float)tabW + 2, tab1.getY(), (float)tabW - 4, tab1.getHeight());
    
    juce::ColourGradient t1Grad(activeTab_ == 0 ? Theme::orange1 : juce::Colour(0xff2a2a2e), 0.0f, tab1.getY(),
                                  activeTab_ == 0 ? Theme::orange3 : juce::Colour(0xff18181b), 0.0f, tab1.getBottom(), false);
    g.setGradientFill(t1Grad);
    g.fillRoundedRectangle(tab1, 3.0f);
    if (activeTab_ == 0) {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawHorizontalLine((int)tab1.getY() + 1, tab1.getX() + 4, tab1.getRight() - 4);
    }
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(tab1, 3.0f, 1.0f);
    g.setColour(activeTab_ == 0 ? juce::Colours::white : Theme::zinc400);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
    g.drawText("WASM PLUGINS", tab1.toNearestInt(), juce::Justification::centred);
    
    juce::ColourGradient t2Grad(activeTab_ == 1 ? Theme::orange1 : juce::Colour(0xff2a2a2e), 0.0f, tab2.getY(),
                                  activeTab_ == 1 ? Theme::orange3 : juce::Colour(0xff18181b), 0.0f, tab2.getBottom(), false);
    g.setGradientFill(t2Grad);
    g.fillRoundedRectangle(tab2, 3.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(tab2, 3.0f, 1.0f);
    g.setColour(activeTab_ == 1 ? juce::Colours::white : Theme::zinc400);
    g.drawText("VST / DLL", tab2.toNearestInt(), juce::Justification::centred);
    
    // ── Section Label ───────────────────────────────────────────
    auto labelRect = juce::Rectangle<int>(0, tabsRect.getBottom(), w, SECTION_LABEL_H);
    g.setColour(Theme::zinc600);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(8.5f).withStyle("Bold"));
    g.drawText("FREE WASM INSTRUMENTS — NO", 12, labelRect.getY() + 2, w - 24, 12, juce::Justification::centredLeft);
    g.drawText("INSTALL NEEDED", 12, labelRect.getY() + 12, w - 24, 12, juce::Justification::centredLeft);
    
    // ── Instruments list ────────────────────────────────────────
    auto instrRect = getInstrumentsRect();
    int iy = instrRect.getY();
    for (size_t i = 0; i < instruments_.size(); ++i)
    {
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
                // Play sample preview
                pluginHost_.playSampleFile(n.file);
                // Arm drag-to-channel-rack
                pendingDragFile_ = n.file;
                dragStarted_ = false;
            }
            
            repaint();
        }
        return;
    }
    
    auto tabsRect = getTabsRect();
    if (tabsRect.contains(e.x, e.y))
    {
        activeTab_ = (e.x < getWidth() / 2) ? 0 : 1;
        repaint();
        return;
    }

    // ── Load button hit-test in the instruments list ──
    auto instrRect = getInstrumentsRect();
    if (instrRect.contains(e.x, e.y))
    {
        int w = getWidth();
        // Load button is at: x = w-50, y = rowTop+7, size 42x18
        int rowIdx = (e.y - instrRect.getY()) / INSTR_H;
        int rowTop = instrRect.getY() + rowIdx * INSTR_H;
        juce::Rectangle<int> loadBtn(w - 50, rowTop + 7, 42, 18);
        if (loadBtn.contains(e.x, e.y))
        {
            if (activeTab_ == 0)
            {
                if (rowIdx >= 0 && rowIdx < (int)instruments_.size() && onLoadWasm)
                    onLoadWasm(instruments_[rowIdx].name, instruments_[rowIdx].type);
            }
            else
            {
                if (onLoadVstPicker) onLoadVstPicker();
            }
            return;
        }
    }
}

void Browser::mouseDrag(const juce::MouseEvent& e)
{
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
    }
}
