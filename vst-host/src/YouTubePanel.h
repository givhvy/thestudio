#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// Competitor-tracking board for YouTube type-beat videos.
// Each tracked video stores: url, parsed videoId, title, channel, niche tag,
// and a history of {date, views} snapshots so you can see growth over time and
// spot which niche is blowing up. Thumbnails auto-download from img.youtube.com
// (public, no API key). Views are entered manually per snapshot (reliable, no
// scraping). Persists to <docsRoot>/youtube_competitors.json.
class YouTubePanel : public juce::Component, private juce::Timer
{
public:
    explicit YouTubePanel(juce::File docsRoot);
    ~YouTubePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    struct Snapshot
    {
        juce::String date;   // YYYY-MM-DD
        juce::int64  views = 0;
    };
    struct Video
    {
        juce::String id;        // youtube video id
        juce::String url;
        juce::String title;
        juce::String channel;
        juce::String niche;     // e.g. "dark trap", "jersey club"
        std::vector<Snapshot> snapshots;
        juce::Image  thumb;     // cached thumbnail (may be empty until loaded)
        bool thumbRequested = false;

        juce::int64 latestViews() const { return snapshots.empty() ? 0 : snapshots.back().views; }
        juce::int64 firstViews()  const { return snapshots.empty() ? 0 : snapshots.front().views; }
        // Growth between last two snapshots (absolute).
        juce::int64 recentGrowth() const
        {
            if (snapshots.size() < 2) return 0;
            return snapshots.back().views - snapshots[snapshots.size() - 2].views;
        }
    };

    enum class Sort { Growth, Views, Recent };

    void timerCallback() override;
    void load();
    void save() const;
    juce::File jsonFile() const;
    juce::File thumbDir() const;

    void addVideoDialog();
    void updateViewsDialog(int videoIdx);
    void editVideoDialog(int videoIdx);
    void requestThumbnail(int videoIdx);
    static juce::String parseVideoId(const juce::String& url);
    static juce::String todayDate();
    static juce::String formatCount(juce::int64 v);

    std::vector<Video> sortedIndexOrder() const;  // returns videos sorted per sort_
    int videoAtPoint(juce::Point<int> p) const;    // returns index in sorted layout, -1 none

    juce::File docsRoot_;
    std::vector<Video> videos_;
    Sort  sort_ = Sort::Growth;
    juce::String nicheFilter_;   // empty = all

    int   scrollY_ = 0;
    int   hoverCard_ = -1;
    juce::Rectangle<int> addBtnRect_, sortBtnRect_;
    // Per-card hit rects in current layout (parallel to drawn order).
    struct CardHit { juce::Rectangle<int> card, updateBtn, openBtn, editBtn, delBtn; int videoIndex; };
    std::vector<CardHit> cardHits_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YouTubePanel)
};
