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
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override;

    // Fired when the user clicks a "Load" button on a WASM-tab instrument.
    // The instrument is built-in / virtual; the host should attach it to the selected mixer track.
    std::function<void(const juce::String& name, const juce::String& type)> onLoadWasm;

    // Fired when the user clicks "Load" on the VST/DLL tab.
    // The host should open a file picker and load the chosen plugin.
    std::function<void()> onLoadVstPicker;

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
    int activeTab_ = 0; // 0 = WASM, 1 = VST/DLL
    juce::File rootFolder_;
    juce::File pendingDragFile_;
    bool dragStarted_ = false;

    // Folder search
    bool                              isSearching_ = false;
    juce::String                      searchQuery_;
    std::unique_ptr<juce::TextEditor> searchEditor_;
    std::vector<TreeNode>             savedTree_;       // backup of allNodes_ while searching
    
    struct Instrument { juce::String name; juce::String type; };
    std::vector<Instrument> instruments_;
    
    static constexpr int ADMIN_H = 28;
    static constexpr int BROWSER_HEAD_H = 28;
    static constexpr int ITEM_H = 22;
    static constexpr int TAB_H = 30;
    static constexpr int SECTION_LABEL_H = 26;
    static constexpr int INSTR_H = 32;
    
    juce::Rectangle<int> getDrumKitListRect() const;
    juce::Rectangle<int> getTabsRect() const;
    juce::Rectangle<int> getInstrumentsRect() const;
    
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
