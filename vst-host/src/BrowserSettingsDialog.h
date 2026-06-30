#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

class BrowserSettingsDialog : public juce::Component
{
public:
    // Five library categories that match Browser::Library. Tags + Marketplace
    // are kept for completeness; the dialog exposes every category the
    // Browser's path resolution can consult.
    static constexpr int kNumLibs = 6;
    enum Lib { Drums = 0, Loops, Acapella, Tags, Marketplace, All };

    struct Paths
    {
        juce::String drums;
        juce::String loops;
        juce::String acapella;
        juce::String tags;
        juce::String marketplace;
        juce::String all;
    };

    BrowserSettingsDialog (const Paths& current, juce::Component* parentForBounds);
    ~BrowserSettingsDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // Modal entry. On OK, invokes onResult(paths). On Cancel/close, onResult
    // is NOT invoked.
    using OnResult = std::function<void(const Paths&)>;
    void runModal(OnResult onResult);

    // Public so callers can read the user's edits after a successful OK.
    const Paths& getPaths() const { return paths_; }

private:
    Paths paths_;
    juce::Component* parentForBounds_ = nullptr;
    OnResult onResult_;

    // 6 rows: [label, text field, browse button, reset button]
    juce::Rectangle<int> rowRects_[kNumLibs][4];
    juce::Rectangle<int> okRect_;
    juce::Rectangle<int> cancelRect_;
    juce::Rectangle<int> resetAllRect_;

    std::array<juce::String, kNumLibs> rowLabels_ {
        "Drum Kits", "Loops", "Acapellas", "Tags", "Marketplace Store", "All (root for all)"
    };

    void openFolderPicker(int row);
    void doOk();
    void doCancel();
    void doReset(int row);
    void drawChrome(juce::Graphics& g, juce::Rectangle<int> r, bool isOk);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserSettingsDialog)
};
