#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

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
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Browser)
};
