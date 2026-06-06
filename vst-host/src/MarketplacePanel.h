#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

class MarketplacePanel final : public juce::Component
{
public:
    MarketplacePanel();
    ~MarketplacePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

    std::function<void()> onClose;
    std::function<void(const juce::String&)> onStatus;
    std::function<void()> onLibraryChanged;

    static juce::File marketplaceRoot();
    static juce::File libraryRootForCategory(const juce::String& category);

private:
    struct Pack
    {
        juce::String id;
        juce::String title;
        juce::String creator;
        juce::String category;
        juce::String genre;
        juce::String description;
        juce::String url;
        juce::String sizeLabel;
        bool installed = false;
        bool localDraft = false;
        juce::Rectangle<int> rect;
        juce::Rectangle<int> actionRect;
    };

    juce::TextEditor searchEditor_;
    juce::TextButton closeButton_ { "X" };
    juce::TextButton refreshButton_ { "REFRESH" };
    juce::TextButton postButton_ { "+ POST KIT" };

    std::vector<Pack> packs_;
    std::vector<int> visiblePacks_;
    juce::String selectedCategory_ = "All";
    juce::String status_ = "Ready";
    int selectedIndex_ = -1;
    int scrollY_ = 0;
    bool loading_ = false;
    std::atomic<bool> destroyed_ { false };
    std::unique_ptr<juce::FileChooser> postChooser_;

    juce::Rectangle<int> panel_;
    juce::Rectangle<int> sidebar_;
    juce::Rectangle<int> content_;
    juce::Rectangle<int> listRect_;
    juce::Rectangle<int> closeRect_;
    juce::Rectangle<int> postRect_;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> categoryRects_;

    void updateLayout();
    void loadCatalogAsync();
    void loadLocalCatalog();
    void rebuildVisible();
    void startInstall(int packIndex);
    void installLocalPack(int packIndex);
    void installDownloadedFile(int packIndex, const juce::File& sourceFile);
    void startPostKit();
    void setStatus(const juce::String& text);
    bool isPackInstalled(const Pack& pack) const;
    static juce::String safeFolderName(const juce::String& text);
    static juce::File localCatalogFile();
    static juce::File uploadsStagingRoot();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MarketplacePanel)
};
