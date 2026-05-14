#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

class PluginHost;

class Browser : public juce::Component
{
public:
    Browser(PluginHost& pluginHost);
    ~Browser() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp  (const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override;

    // Fired when the user clicks "Load" on a real scanned plugin in the
    // PLUGINS tab. fileOrIdentifier is the VST3/DLL path (or JUCE identifier
    // string from KnownPluginList).
    std::function<void(const juce::String& name, const juce::String& fileOrIdentifier)> onLoadPlugin;

    // Fired when the user clicks "Load" on the VST/DLL tab.
    // The host should open a file picker and load the chosen plugin.
    std::function<void()> onLoadVstPicker;

    // Re-scan default plugin folders and refresh the list. Called once on
    // construction; the user can also trigger via "Rescan" pseudo-row.
    void refreshPluginList();

private:
    PluginHost& pluginHost_;
    
    struct TreeNode
    {
        juce::File file;        // empty for synth-list items
        juce::String displayName;
        int depth = 0;
        bool isFolder = false;
        bool isExpanded = false;
        bool isAudio = false;
    };
    
    std::vector<TreeNode> allNodes_;     // full tree (only used to rebuild visible)
    std::vector<int> visibleIndices_;     // indexes into allNodes_ currently visible
    
    int selectedIdx_ = -1;
    int scrollY_ = 0;
    int pluginScrollY_ = 0;
    int activeTab_ = 0; // 0 = PLUGINS, 1 = VST/DLL

    // Height (in px) of the bottom plugins panel — drag the divider above
    // the tabs to resize. 0 collapses the panel completely.
    int pluginPanelH_ = 280;
    bool draggingDivider_ = false;
    int  dragStartY_ = 0;
    int  dragStartPanelH_ = 0;

    static constexpr int DIVIDER_H = 6;
    juce::File rootFolder_;
    juce::File pendingDragFile_;
    bool dragStarted_ = false;

    // Folder search
    bool                              isSearching_ = false;
    juce::String                      searchQuery_;
    std::unique_ptr<juce::TextEditor> searchEditor_;
    std::vector<TreeNode>             savedTree_;       // backup of allNodes_ while searching
    
    struct Instrument {
        juce::String name;                // Display name
        juce::String type;                // "VST3 inst", "VST3 fx", etc.
        juce::String fileOrIdentifier;    // Path/ID passed to PluginHost::loadPlugin
    };
    std::vector<Instrument> instruments_;
    
    static constexpr int ADMIN_H = 0;   // (removed; kept as 0 to avoid touching layout math)
    static constexpr int BROWSER_HEAD_H = 28;
    static constexpr int ITEM_H = 22;
    static constexpr int TAB_H = 30;
    static constexpr int SECTION_LABEL_H = 26;
    static constexpr int INSTR_H = 32;
    
    juce::Rectangle<int> getDrumKitListRect() const;
    juce::Rectangle<int> getDividerRect() const;
    juce::Rectangle<int> getTabsRect() const;
    juce::Rectangle<int> getInstrumentsRect() const;
    int  effectivePluginPanelH() const;
    
    void buildTree();
    void rebuildVisible();
    void scanFolder(const juce::File& folder, int depth);
    bool isAudioFile(const juce::File& f) const;

    // Folder search
    void startSearch();
    void endSearch();
    void performSearch(const juce::String& query);
    void collectFoldersRecursive(const juce::File& folder, const juce::String& q,
                                 std::vector<TreeNode>& out, int maxDepth, int curDepth);
    juce::Rectangle<int> getSearchEditorRect() const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Browser)
};
