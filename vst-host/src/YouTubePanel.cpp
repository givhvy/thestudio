#include "YouTubePanel.h"
#include "Theme.h"

YouTubePanel::YouTubePanel(juce::File docsRoot)
    : docsRoot_(std::move(docsRoot))
{
    setWantsKeyboardFocus(true);
    thumbDir().createDirectory();
    load();
    startTimerHz(4);   // poll for freshly-downloaded thumbnails to repaint
}

YouTubePanel::~YouTubePanel() = default;

juce::File YouTubePanel::jsonFile() const  { return docsRoot_.getChildFile("youtube_competitors.json"); }
juce::File YouTubePanel::thumbDir() const  { return docsRoot_.getChildFile("youtube_thumbs"); }

juce::String YouTubePanel::todayDate()
{
    auto t = juce::Time::getCurrentTime();
    return juce::String::formatted("%04d-%02d-%02d", t.getYear(), t.getMonth() + 1, t.getDayOfMonth());
}

juce::String YouTubePanel::formatCount(juce::int64 v)
{
    if (v >= 1000000) return juce::String(v / 1000000.0, 1) + "M";
    if (v >= 1000)    return juce::String(v / 1000.0, 1) + "K";
    return juce::String(v);
}

juce::String YouTubePanel::parseVideoId(const juce::String& url)
{
    // Handles youtu.be/<id>, watch?v=<id>, shorts/<id>, embed/<id>.
    auto u = url.trim();
    if (u.contains("youtu.be/"))
        return u.fromFirstOccurrenceOf("youtu.be/", false, false)
                .upToFirstOccurrenceOf("?", false, false)
                .upToFirstOccurrenceOf("&", false, false).trim();
    if (u.contains("v="))
        return u.fromFirstOccurrenceOf("v=", false, false)
                .upToFirstOccurrenceOf("&", false, false).trim();
    for (auto* seg : { "shorts/", "embed/" })
        if (u.contains(seg))
            return u.fromFirstOccurrenceOf(seg, false, false)
                    .upToFirstOccurrenceOf("?", false, false)
                    .upToFirstOccurrenceOf("&", false, false).trim();
    // Bare id pasted.
    if (u.length() == 11 && !u.containsChar('/')) return u;
    return {};
}

// ─────────────────────────── persistence ───────────────────────────
void YouTubePanel::load()
{
    videos_.clear();
    auto f = jsonFile();
    if (!f.existsAsFile()) return;
    auto parsed = juce::JSON::parse(f.loadFileAsString());
    auto* arr = parsed.getProperty("videos", juce::var()).getArray();
    if (arr == nullptr) return;
    for (auto& v : *arr)
    {
        Video vid;
        vid.id      = v.getProperty("id", "").toString();
        vid.url     = v.getProperty("url", "").toString();
        vid.title   = v.getProperty("title", "").toString();
        vid.channel = v.getProperty("channel", "").toString();
        vid.niche   = v.getProperty("niche", "").toString();
        if (auto* snaps = v.getProperty("snapshots", juce::var()).getArray())
            for (auto& s : *snaps)
            {
                Snapshot sn;
                sn.date  = s.getProperty("date", "").toString();
                sn.views = (juce::int64) (double) s.getProperty("views", 0);
                vid.snapshots.push_back(sn);
            }
        // Load cached thumbnail if present.
        auto tf = thumbDir().getChildFile(vid.id + ".jpg");
        if (tf.existsAsFile())
        {
            vid.thumb = juce::ImageFileFormat::loadFrom(tf);
            vid.thumbRequested = true;
        }
        videos_.push_back(std::move(vid));
    }
}

void YouTubePanel::save() const
{
    juce::Array<juce::var> arr;
    for (auto& v : videos_)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("id", v.id);
        o->setProperty("url", v.url);
        o->setProperty("title", v.title);
        o->setProperty("channel", v.channel);
        o->setProperty("niche", v.niche);
        juce::Array<juce::var> snaps;
        for (auto& s : v.snapshots)
        {
            auto* so = new juce::DynamicObject();
            so->setProperty("date", s.date);
            so->setProperty("views", (double) s.views);
            snaps.add(juce::var(so));
        }
        o->setProperty("snapshots", snaps);
        arr.add(juce::var(o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty("videos", arr);
    jsonFile().replaceWithText(juce::JSON::toString(juce::var(root)));
}

// ─────────────────────────── thumbnails ───────────────────────────
void YouTubePanel::requestThumbnail(int videoIdx)
{
    if (videoIdx < 0 || videoIdx >= (int)videos_.size()) return;
    auto& v = videos_[(size_t)videoIdx];
    if (v.thumbRequested || v.id.isEmpty()) return;
    v.thumbRequested = true;

    const juce::String id = v.id;
    const juce::File dest = thumbDir().getChildFile(id + ".jpg");
    juce::Component::SafePointer<YouTubePanel> safe(this);

    juce::Thread::launch([safe, id, dest]()
    {
        // Try maxres first, fall back to hqdefault.
        for (auto* variant : { "maxresdefault", "hqdefault" })
        {
            juce::URL url("https://img.youtube.com/vi/" + id + "/" + variant + ".jpg");
            std::unique_ptr<juce::InputStream> in(
                url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                          .withConnectionTimeoutMs(6000)));
            if (in == nullptr) continue;
            juce::MemoryBlock mb;
            in->readIntoMemoryBlock(mb);
            if (mb.getSize() < 2000) continue;  // youtube returns a tiny placeholder when missing
            dest.replaceWithData(mb.getData(), mb.getSize());
            auto img = juce::ImageFileFormat::loadFrom(dest);
            if (img.isValid())
            {
                juce::MessageManager::callAsync([safe, id, img]()
                {
                    if (safe == nullptr) return;
                    for (auto& vv : safe->videos_)
                        if (vv.id == id) { vv.thumb = img; break; }
                    safe->repaint();
                });
                return;
            }
        }
    });
}

// ─────────────────────────── dialogs ───────────────────────────
void YouTubePanel::addVideoDialog()
{
    auto* aw = new juce::AlertWindow("Track competitor video",
        "Paste the YouTube link. Add a niche tag (e.g. dark trap, jersey club) so you can\n"
        "spot which style is blowing up. Views are tracked manually as snapshots.",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor("url", "", "YouTube link:");
    aw->addTextEditor("title", "", "Title (optional):");
    aw->addTextEditor("channel", "", "Channel (optional):");
    aw->addTextEditor("niche", "", "Niche tag:");
    aw->addTextEditor("views", "", "Current views (number):");
    aw->addButton("Add", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<YouTubePanel> safe(this);
    aw->enterModalState(true, juce::ModalCallbackFunction::create([safe, aw](int r)
    {
        if (r == 1 && safe != nullptr)
        {
            Video v;
            v.url     = aw->getTextEditorContents("url").trim();
            v.id      = parseVideoId(v.url);
            v.title   = aw->getTextEditorContents("title").trim();
            v.channel = aw->getTextEditorContents("channel").trim();
            v.niche   = aw->getTextEditorContents("niche").trim();
            auto viewsStr = aw->getTextEditorContents("views").retainCharacters("0123456789");
            if (v.id.isNotEmpty())
            {
                if (v.title.isEmpty()) v.title = "Video " + v.id;
                v.snapshots.push_back({ todayDate(), viewsStr.isNotEmpty() ? viewsStr.getLargeIntValue() : 0 });
                safe->videos_.push_back(std::move(v));
                safe->requestThumbnail((int)safe->videos_.size() - 1);
                safe->save();
                safe->repaint();
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Bad link", "Could not find a YouTube video id in that link.");
            }
        }
        delete aw;
    }), true);
}

void YouTubePanel::updateViewsDialog(int videoIdx)
{
    if (videoIdx < 0 || videoIdx >= (int)videos_.size()) return;
    auto& v = videos_[(size_t)videoIdx];
    auto* aw = new juce::AlertWindow("Update views",
        "New view count for \"" + v.title + "\".\nAppends a dated snapshot so growth is tracked.",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor("views", juce::String(v.latestViews()), "Views today:");
    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<YouTubePanel> safe(this);
    aw->enterModalState(true, juce::ModalCallbackFunction::create([safe, aw, videoIdx](int r)
    {
        if (r == 1 && safe != nullptr && videoIdx < (int)safe->videos_.size())
        {
            auto viewsStr = aw->getTextEditorContents("views").retainCharacters("0123456789");
            if (viewsStr.isNotEmpty())
            {
                auto& vid = safe->videos_[(size_t)videoIdx];
                const auto today = todayDate();
                // Overwrite same-day snapshot instead of stacking duplicates.
                if (!vid.snapshots.empty() && vid.snapshots.back().date == today)
                    vid.snapshots.back().views = viewsStr.getLargeIntValue();
                else
                    vid.snapshots.push_back({ today, viewsStr.getLargeIntValue() });
                safe->save();
                safe->repaint();
            }
        }
        delete aw;
    }), true);
}

void YouTubePanel::editVideoDialog(int videoIdx)
{
    if (videoIdx < 0 || videoIdx >= (int)videos_.size()) return;
    auto& v = videos_[(size_t)videoIdx];
    auto* aw = new juce::AlertWindow("Edit video", "", juce::AlertWindow::NoIcon);
    aw->addTextEditor("title", v.title, "Title:");
    aw->addTextEditor("channel", v.channel, "Channel:");
    aw->addTextEditor("niche", v.niche, "Niche tag:");
    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<YouTubePanel> safe(this);
    aw->enterModalState(true, juce::ModalCallbackFunction::create([safe, aw, videoIdx](int r)
    {
        if (r == 1 && safe != nullptr && videoIdx < (int)safe->videos_.size())
        {
            auto& vid = safe->videos_[(size_t)videoIdx];
            vid.title   = aw->getTextEditorContents("title").trim();
            vid.channel = aw->getTextEditorContents("channel").trim();
            vid.niche   = aw->getTextEditorContents("niche").trim();
            safe->save();
            safe->repaint();
        }
        delete aw;
    }), true);
}

// ─────────────────────────── layout helpers ───────────────────────────
std::vector<YouTubePanel::Video> YouTubePanel::sortedIndexOrder() const
{
    std::vector<Video> out = videos_;
    if (nicheFilter_.isNotEmpty())
        out.erase(std::remove_if(out.begin(), out.end(),
            [this](const Video& v){ return !v.niche.equalsIgnoreCase(nicheFilter_); }), out.end());
    auto cmp = [this](const Video& a, const Video& b)
    {
        switch (sort_)
        {
            case Sort::Growth: return a.recentGrowth() > b.recentGrowth();
            case Sort::Views:  return a.latestViews()  > b.latestViews();
            case Sort::Recent: return a.id > b.id;   // newest added last → reverse-ish; fine
        }
        return false;
    };
    std::stable_sort(out.begin(), out.end(), cmp);
    return out;
}

// ─────────────────────────── paint ───────────────────────────
void YouTubePanel::paint(juce::Graphics& g)
{
    if (Theme::aeroMode) Theme::drawAeroPanel(g, getLocalBounds().toFloat());
    else                 g.fillAll(juce::Colour(0xff0c0c0e));

    const int w = getWidth();
    const int pad = 20;

    // ── Header ──
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(22.0f).withStyle("Bold"));
    g.drawText("YOUTUBE", pad, 14, 200, 28, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffef4444));
    g.fillRoundedRectangle((float)pad + 92, 20.0f, 16.0f, 16.0f, 3.0f);
    g.setColour(juce::Colours::white);
    juce::Path tri; tri.addTriangle((float)pad + 97, 24.0f, (float)pad + 97, 32.0f, (float)pad + 104, 28.0f);
    g.fillPath(tri);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
    g.drawText("COMPETITOR TYPE-BEAT TRACKER - spot what niche is blowing up",
               pad, 42, w - pad * 2, 16, juce::Justification::centredLeft);

    // ── Toolbar buttons ──
    addBtnRect_  = juce::Rectangle<int>(w - pad - 130, 16, 130, 28);
    sortBtnRect_ = juce::Rectangle<int>(w - pad - 130 - 8 - 150, 16, 150, 28);

    g.setColour(juce::Colour(0xffef4444));
    g.fillRoundedRectangle(addBtnRect_.toFloat(), 6.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
    g.drawText("+ TRACK VIDEO", addBtnRect_, juce::Justification::centred);

    g.setColour(juce::Colour(0xff27272a));
    g.fillRoundedRectangle(sortBtnRect_.toFloat(), 6.0f);
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
    const char* sortLbl = sort_ == Sort::Growth ? "SORT: BLOWING UP"
                        : sort_ == Sort::Views  ? "SORT: TOP VIEWS" : "SORT: RECENT";
    g.drawText(sortLbl, sortBtnRect_, juce::Justification::centred);

    // ── Cards grid (scrollable) ──
    const int gridTop = 70;
    const int cardW = 300;
    const int thumbH = (int)(cardW * 9.0 / 16.0);   // 16:9
    const int cardH = thumbH + 96;
    const int gap = 16;
    const int cols = juce::jmax(1, (w - pad * 2 + gap) / (cardW + gap));
    const int gridW = cols * cardW + (cols - 1) * gap;
    const int startX = (w - gridW) / 2;

    cardHits_.clear();
    auto display = sortedIndexOrder();

    if (display.empty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(15.0f));
        g.drawText("No videos tracked yet. Click  + TRACK VIDEO  to paste a competitor's YouTube link.",
                   pad, gridTop + 40, w - pad * 2, 30, juce::Justification::centredTop);
        return;
    }

    g.saveState();
    g.reduceClipRegion(0, gridTop, w, getHeight() - gridTop);

    for (size_t i = 0; i < display.size(); ++i)
    {
        const int col = (int)i % cols;
        const int row = (int)i / cols;
        const int cx = startX + col * (cardW + gap);
        const int cy = gridTop + row * (cardH + gap) - scrollY_;
        juce::Rectangle<int> card(cx, cy, cardW, cardH);

        // Find the real index in videos_ for actions.
        int realIdx = -1;
        for (size_t k = 0; k < videos_.size(); ++k)
            if (videos_[k].id == display[i].id) { realIdx = (int)k; break; }

        const auto& v = display[i];
        const bool hovered = (hoverCard_ == (int)i);

        // Card background.
        auto fr = card.toFloat();
        for (int s = 4; s > 0; --s)
        {
            g.setColour(juce::Colours::black.withAlpha((hovered ? 0.10f : 0.06f) / (float)s));
            g.fillRoundedRectangle(fr.translated(0, (float)s * 1.5f).expanded((float)s * 0.3f), 10.0f);
        }
        g.setColour(juce::Colour(0xff18181b));
        g.fillRoundedRectangle(fr, 10.0f);

        // Thumbnail.
        auto thumbR = juce::Rectangle<float>(fr.getX(), fr.getY(), fr.getWidth(), (float)thumbH);
        g.saveState();
        juce::Path clip; clip.addRoundedRectangle(thumbR.getX(), thumbR.getY(), thumbR.getWidth(), thumbR.getHeight(),
                                                  10.0f, 10.0f, true, true, false, false);
        g.reduceClipRegion(clip);
        if (v.thumb.isValid())
            g.drawImage(v.thumb, thumbR, juce::RectanglePlacement::fillDestination);
        else
        {
            g.setColour(juce::Colour(0xff27272a));
            g.fillRect(thumbR);
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f));
            g.drawText("loading thumbnail...", thumbR.toNearestInt(), juce::Justification::centred);
        }
        g.restoreState();

        // Niche tag chip (top-left over thumbnail).
        if (v.niche.isNotEmpty())
        {
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f).withStyle("Bold"));
            const int tagW = juce::jmin(cardW - 16, (int)juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), v.niche.toUpperCase()) + 16);
            juce::Rectangle<int> tag(cx + 8, (int)thumbR.getY() + 8, tagW, 18);
            g.setColour(juce::Colour(0xff000000).withAlpha(0.65f));
            g.fillRoundedRectangle(tag.toFloat(), 9.0f);
            g.setColour(juce::Colour(0xffef4444));
            g.drawText(v.niche.toUpperCase(), tag, juce::Justification::centred);
        }

        // Body text.
        int ty = (int)thumbR.getBottom() + 8;
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
        g.drawText(v.title, cx + 10, ty, cardW - 20, 18, juce::Justification::centredLeft, true);
        ty += 18;
        if (v.channel.isNotEmpty())
        {
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f));
            g.drawText(v.channel, cx + 10, ty, cardW - 20, 14, juce::Justification::centredLeft, true);
        }
        ty += 16;

        // Views big + growth.
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(18.0f).withStyle("Bold"));
        g.drawText(formatCount(v.latestViews()) + " views", cx + 10, ty, cardW - 20, 22, juce::Justification::centredLeft);

        const juce::int64 growth = v.recentGrowth();
        if (v.snapshots.size() >= 2)
        {
            const bool up = growth >= 0;
            g.setColour(up ? juce::Colour(0xff22c55e) : juce::Colour(0xffef4444));
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(12.0f).withStyle("Bold"));
            g.drawText((up ? "+" : "") + formatCount(growth) + (up ? "  \xe2\x96\xb2" : "  \xe2\x96\xbc"),
                       cx + 10, ty + 22, cardW - 20, 16, juce::Justification::centredLeft);
        }

        // Mini sparkline of view history (bottom-right of body).
        if (v.snapshots.size() >= 2)
        {
            juce::Rectangle<float> spark((float)(cx + cardW - 110), (float)(ty + 2), 100.0f, 34.0f);
            juce::int64 mn = v.snapshots.front().views, mx = mn;
            for (auto& s : v.snapshots) { mn = juce::jmin(mn, s.views); mx = juce::jmax(mx, s.views); }
            const double range = juce::jmax<juce::int64>(1, mx - mn);
            juce::Path p;
            for (size_t k = 0; k < v.snapshots.size(); ++k)
            {
                const float fx = spark.getX() + spark.getWidth() * (float)k / (float)(v.snapshots.size() - 1);
                const float fy = spark.getBottom() - (float)((v.snapshots[k].views - mn) / range) * spark.getHeight();
                if (k == 0) p.startNewSubPath(fx, fy); else p.lineTo(fx, fy);
            }
            g.setColour(juce::Colour(0xffef4444));
            g.strokePath(p, juce::PathStrokeType(1.8f));
        }

        // Action buttons row (bottom of card).
        const int bh = 22;
        const int by = cy + cardH - bh - 8;
        juce::Rectangle<int> updateBtn(cx + 10, by, 92, bh);
        juce::Rectangle<int> openBtn(updateBtn.getRight() + 6, by, 56, bh);
        juce::Rectangle<int> editBtn(openBtn.getRight() + 6, by, 44, bh);
        juce::Rectangle<int> delBtn(cx + cardW - 10 - 28, by, 28, bh);

        auto miniBtn = [&](juce::Rectangle<int> r, const juce::String& lbl, juce::Colour bg, juce::Colour fg)
        {
            g.setColour(bg); g.fillRoundedRectangle(r.toFloat(), 5.0f);
            g.setColour(fg); g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f).withStyle("Bold"));
            g.drawText(lbl, r, juce::Justification::centred);
        };
        miniBtn(updateBtn, "+ VIEWS", juce::Colour(0xffea580c), juce::Colours::white);
        miniBtn(openBtn,   "OPEN",    juce::Colour(0xff27272a), juce::Colours::white.withAlpha(0.85f));
        miniBtn(editBtn,   "EDIT",    juce::Colour(0xff27272a), juce::Colours::white.withAlpha(0.85f));
        miniBtn(delBtn,    "\xc3\x97", juce::Colour(0xff3f1212), juce::Colour(0xffef4444));

        if (hovered)
        {
            g.setColour(juce::Colour(0xffef4444).withAlpha(0.6f));
            g.drawRoundedRectangle(fr, 10.0f, 1.5f);
        }

        cardHits_.push_back({ card, updateBtn, openBtn, editBtn, delBtn, realIdx });

        // Kick off thumbnail download lazily for anything visible without one.
        if (realIdx >= 0 && !videos_[(size_t)realIdx].thumbRequested)
            requestThumbnail(realIdx);
    }
    g.restoreState();
}

void YouTubePanel::resized() {}

void YouTubePanel::mouseMove(const juce::MouseEvent& e)
{
    int h = -1;
    for (size_t i = 0; i < cardHits_.size(); ++i)
        if (cardHits_[i].card.contains(e.getPosition())) { h = (int)i; break; }
    if (h != hoverCard_) { hoverCard_ = h; repaint(); }
}
void YouTubePanel::mouseExit(const juce::MouseEvent&) { if (hoverCard_ != -1) { hoverCard_ = -1; repaint(); } }

void YouTubePanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    scrollY_ = juce::jmax(0, scrollY_ - (int)(wheel.deltaY * 80));
    repaint();
}

void YouTubePanel::mouseDown(const juce::MouseEvent& e)
{
    auto p = e.getPosition();
    if (addBtnRect_.contains(p))  { addVideoDialog(); return; }
    if (sortBtnRect_.contains(p))
    {
        sort_ = sort_ == Sort::Growth ? Sort::Views
              : sort_ == Sort::Views  ? Sort::Recent : Sort::Growth;
        repaint();
        return;
    }

    for (auto& hit : cardHits_)
    {
        if (hit.videoIndex < 0) continue;
        if (hit.updateBtn.contains(p)) { updateViewsDialog(hit.videoIndex); return; }
        if (hit.editBtn.contains(p))   { editVideoDialog(hit.videoIndex); return; }
        if (hit.openBtn.contains(p))
        {
            juce::URL(videos_[(size_t)hit.videoIndex].url).launchInDefaultBrowser();
            return;
        }
        if (hit.delBtn.contains(p))
        {
            const int idx = hit.videoIndex;
            const auto title = videos_[(size_t)idx].title;
            juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon, "Remove video",
                "Stop tracking \"" + title + "\"?", "Remove", "Cancel", nullptr,
                juce::ModalCallbackFunction::create([this, idx](int ok)
                {
                    if (ok && idx < (int)videos_.size())
                    {
                        videos_.erase(videos_.begin() + idx);
                        save();
                        repaint();
                    }
                }));
            return;
        }
        if (hit.card.contains(p))
        {
            juce::URL(videos_[(size_t)hit.videoIndex].url).launchInDefaultBrowser();
            return;
        }
    }
}

void YouTubePanel::timerCallback() { /* repaint driven by async thumbnail loads */ }

int YouTubePanel::videoAtPoint(juce::Point<int> p) const
{
    for (size_t i = 0; i < cardHits_.size(); ++i)
        if (cardHits_[i].card.contains(p)) return (int)i;
    return -1;
}
