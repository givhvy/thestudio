#include "PatternsPanel.h"
#include "Theme.h"

namespace
{
    PatternsPanel::PatternDefinition makePattern(const juce::String& id,
                                                 const juce::String& presetId,
                                                 const juce::String& genre,
                                                 const juce::String& title,
                                                 const juce::String& feel,
                                                 int bpm,
                                                 std::initializer_list<std::initializer_list<int>> rows,
                                                 bool useFullPresetRows = false)
    {
        PatternsPanel::PatternDefinition p;
        p.id = id;
        p.presetId = presetId;
        p.genre = genre;
        p.title = title;
        p.feel = feel;
        p.bpm = bpm;
        p.useFullPresetRows = useFullPresetRows || p.presetId.isNotEmpty();

        int r = 0;
        for (const auto& row : rows)
        {
            int c = 0;
            for (int v : row)
            {
                if (r < 4 && c < 16)
                    p.rows[(size_t)r][(size_t)c] = v;
                ++c;
            }
            ++r;
        }

        return p;
    }
}

std::vector<PatternsPanel::PatternDefinition> PatternsPanel::getPatternLibrary()
{
    return {
    makePattern("boom_bap_default", "boom_bap", "Boom Bap", "Default Boom Bap", "current AI default", 90,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("boom_bap_dilla", "boom_bap", "Boom Bap", "Dusty Swing", "lazy kick, late hats", 90,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,0},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePattern("boom_bap_premo", "boom_bap", "Boom Bap", "NY Headnod", "hard snare, simple pocket", 92,
        {{1,0,0,0,0,0,0,1,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("boom_bap_sparse", "boom_bap", "Boom Bap", "Sparse Vinyl", "space for chops", 86,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0}}),

    makePattern("hiphop_default", "hiphop", "Hip Hop", "Default Hip Hop", "current AI default", 92,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("hiphop_modern", "hiphop", "Hip Hop", "Modern Knock", "tight hats and bounce", 94,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0}}),
    makePattern("hiphop_west", "hiphop", "Hip Hop", "West Coast", "clap layer and open hats", 96,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,1,0,1,0,0,0}}),

    makePattern("trap_default", "trap", "Trap", "Default Trap", "current AI default", 140,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("trap_atl", "trap", "Trap", "ATL Rolls", "rolling hats, simple 808", 140,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePattern("trap_dark", "trap", "Trap", "Dark Half-Time", "wide gaps, heavy bounce", 130,
        {{1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1},
         {0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0}}),

    makePattern("drill_default", "drill", "Drill", "Default Drill", "current AI default", 142,
        {{1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,1,0,0,0,0,1,0,1,0},
         {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("drill_slide", "drill", "Drill", "Slide Drill", "syncopated kick and snares", 142,
        {{1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,1,0,0,0,0,1,0,1,0},
         {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0}}),
    makePattern("house_default", "house", "House", "Default House", "current AI default", 124,
        {{1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("rnb_default", "rnb", "R&B", "Default R&B", "current AI default", 75,
        {{1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
         {1,0,1,0,1,0,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("rnb_late", "rnb", "R&B", "Late Pocket", "soft snare and rim", 75,
        {{1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
         {1,0,1,0,1,0,1,0,1,0,1,1,1,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePattern("lofi_default", "lofi", "Lo-Fi", "Default Lo-Fi", "current AI default", 75,
        {{1,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1},
         {0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("rock_default", "rock", "Rock", "Default Rock", "current AI default", 120,
        {{1,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}),
    makePattern("detroit_default", "detroit", "Detroit Flint", "Default Detroit", "current AI default", 70,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,1,1,0,0,0},
         {1,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("afrobeat_default", "afrobeat", "Afrobeat", "Default Afrobeat", "current AI default", 104,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePattern("house_classic", "house", "House", "Four On Floor", "club foundation", 124,
        {{1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("reggaeton_default", "reggaeton", "Reggaeton", "Default Reggaeton", "current AI default", 96,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},
         {0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("jersey_default", "jersey", "Jersey Club", "Default Jersey", "current AI default", 135,
        {{1,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,1,0,1,1,0,1,1,1,1,0,1,1,0,1,1},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("ukg_default", "ukg", "UK Garage", "Default UK Garage", "current AI default", 132,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("dnb_default", "dnb", "Drum & Bass", "Default D&B", "current AI default", 174,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
         {0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0}}),
    makePattern("techno_default", "techno", "Techno", "Default Techno", "current AI default", 128,
        {{1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0},
         {0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("phonk_default", "phonk", "Phonk", "Default Phonk", "current AI default", 130,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("memphis_default", "memphis", "Memphis", "Default Memphis", "current AI default", 150,
        {{1,0,0,0,0,1,0,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("funk_default", "funk", "Funk", "Default Funk", "current AI default", 102,
        {{1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,1,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,0,1,1,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makePattern("ukg_shuffle", "ukg", "UK Garage", "Shuffle", "skippy kick and hats", 132,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0},
         {0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1}}),
    makePattern("reggaeton_dembow", "reggaeton", "Reggaeton", "Dembow", "classic dembow pulse", 96,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},
         {0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}})
    };
}

std::vector<PatternsPanel::PatternDefinition> PatternsPanel::getPatternsForPreset(const juce::String& presetId)
{
    std::vector<PatternDefinition> matches;
    for (const auto& p : getPatternLibrary())
        if (p.presetId.equalsIgnoreCase(presetId))
            matches.push_back(p);
    return matches;
}

PatternsPanel::PatternsPanel()
{
    for (const auto& def : getPatternLibrary())
    {
        patterns_.push_back({ def, {} });
        if (!genres_.contains(def.genre))
            genres_.add(def.genre);
    }
}

std::vector<int> PatternsPanel::visiblePatternIndices() const
{
    std::vector<int> ids;
    for (int i = 0; i < (int)patterns_.size(); ++i)
        if (patterns_[(size_t)i].def.genre == selectedGenre_)
            ids.push_back(i);
    return ids;
}

const PatternsPanel::PatternEntry* PatternsPanel::selectedPattern() const
{
    if (selectedPattern_ >= 0 && selectedPattern_ < (int)patterns_.size())
        return &patterns_[(size_t)selectedPattern_];
    return nullptr;
}

void PatternsPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::DropShadow(juce::Colours::black.withAlpha(0.65f), 22, { 0, 8 })
        .drawForRectangle(g, getLocalBounds().reduced(2));

    juce::ColourGradient chassis(juce::Colour(0xff17171a), 0.0f, 0.0f,
                                 juce::Colour(0xff08080a), 0.0f, bounds.getBottom(), false);
    g.setGradientFill(chassis);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colour(0xff2a2a2e));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    auto header = juce::Rectangle<int>(0, 0, getWidth(), HEADER_H);
    juce::ColourGradient hg(Theme::orange1, 0.0f, 0.0f, Theme::orange3, 0.0f, (float)HEADER_H, false);
    g.setGradientFill(hg);
    g.fillRect(header);
    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(HEADER_H - 1, 0.0f, (float)getWidth());
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(13.0f).withStyle("Bold"));
    g.drawText("PATTERNS", 14, 0, getWidth() - 60, HEADER_H, juce::Justification::centredLeft);

    closeBtnRect_ = { getWidth() - HEADER_H, 0, HEADER_H, HEADER_H };
    g.drawText("X", closeBtnRect_, juce::Justification::centred);

    auto body = getLocalBounds().withTrimmedTop(HEADER_H).withTrimmedBottom(FOOTER_H).reduced(10);
    auto left = body.removeFromLeft(GENRE_W);
    auto right = body.withTrimmedLeft(10);

    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("GENRES", left.removeFromTop(20), juce::Justification::centredLeft);

    for (int i = 0; i < genres_.size() && i < 32; ++i)
    {
        auto r = left.removeFromTop(28).reduced(0, 3);
        genreRects_[i] = r;
        const bool selected = genres_[i] == selectedGenre_;
        g.setColour(selected ? Theme::orange1.withAlpha(0.95f) : juce::Colour(0xff1c1c20));
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
        g.setColour(selected ? juce::Colours::black.withAlpha(0.65f) : juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 4.0f, 1.0f);
        g.setColour(selected ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(genres_[i], r.reduced(9, 0), juce::Justification::centredLeft);
    }

    auto top = right.removeFromTop(24);
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("MIDI PATTERNS", top, juce::Justification::centredLeft);

    auto preview = right.removeFromBottom(160);
    auto listArea = right.reduced(0, 4);
    g.setColour(juce::Colour(0xff050507));
    g.fillRoundedRectangle(listArea.toFloat(), 5.0f);
    g.setColour(juce::Colour(0xff222226));
    g.drawRoundedRectangle(listArea.toFloat(), 5.0f, 1.0f);

    g.saveState();
    g.reduceClipRegion(listArea);
    int y = listArea.getY() - listScrollY_;
    auto visible = visiblePatternIndices();
    for (int n = 0; n < (int)visible.size(); ++n)
    {
        auto& p = patterns_[(size_t)visible[(size_t)n]];
        auto r = juce::Rectangle<int>(listArea.getX() + 6, y + 6, listArea.getWidth() - 12, ROW_H - 6);
        p.rect = r;
        const bool selected = visible[(size_t)n] == selectedPattern_;
        g.setColour(selected ? juce::Colour(0xff2b1a10) : juce::Colour(0xff19191d));
        g.fillRoundedRectangle(r.toFloat(), 5.0f);
        g.setColour(selected ? Theme::orange1 : juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 5.0f, selected ? 1.5f : 1.0f);

        g.setColour(Theme::zinc100);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(11.0f).withStyle("Bold"));
        g.drawText(p.def.title, r.getX() + 10, r.getY() + 7, r.getWidth() - 110, 16, juce::Justification::centredLeft);
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
        g.drawText(p.def.feel, r.getX() + 10, r.getY() + 28, r.getWidth() - 110, 15, juce::Justification::centredLeft);
        g.setColour(Theme::orange2);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(juce::String(p.def.bpm) + " BPM", r.getRight() - 82, r.getY() + 17, 70, 18, juce::Justification::centredRight);
        y += ROW_H;
    }
    g.restoreState();

    if (auto* p = selectedPattern())
        drawPreview(g, preview.reduced(0, 10), *p);

    auto footer = getLocalBounds().withTrimmedTop(getHeight() - FOOTER_H).reduced(10, 7);
    applyBtnRect_ = footer.removeFromRight(150);
    g.setColour(Theme::orange1);
    g.fillRoundedRectangle(applyBtnRect_.toFloat(), 4.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(applyBtnRect_.toFloat(), 4.0f, 1.0f);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.5f).withStyle("Bold"));
    g.drawText("APPLY TO RACK", applyBtnRect_, juce::Justification::centred);

    g.setColour(Theme::zinc600);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
    g.drawText("Pattern dashboard for AI drum MIDI presets.", footer, juce::Justification::centredLeft);
}

void PatternsPanel::drawPreview(juce::Graphics& g, juce::Rectangle<int> area, const PatternEntry& p)
{
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText("PREVIEW: " + p.def.genre + " / " + p.def.title, area.removeFromTop(18), juce::Justification::centredLeft);

    static const char* rowNames[] = { "Kick", "Snare", "Hat", "Perc" };
    const int labelW = 46;
    const int stepW = juce::jmax(8, (area.getWidth() - labelW - 16 * 4) / 16);
    int y = area.getY() + 6;
    for (int r = 0; r < 4; ++r)
    {
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.0f).withStyle("Bold"));
        g.drawText(rowNames[r], area.getX(), y, labelW - 6, 18, juce::Justification::centredRight);

        int x = area.getX() + labelW;
        for (int s = 0; s < 16; ++s)
        {
            auto cell = juce::Rectangle<float>((float)x, (float)y, (float)stepW, 18.0f);
            g.setColour(p.def.rows[(size_t)r][(size_t)s] ? Theme::orange1 : juce::Colour(0xff121215));
            g.fillRoundedRectangle(cell, 3.0f);
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(cell, 3.0f, 1.0f);
            x += stepW + 4;
        }
        y += 24;
    }
}

void PatternsPanel::resized() {}

void PatternsPanel::mouseDown(const juce::MouseEvent& e)
{
    isDraggingPanel_ = false;
    if (closeBtnRect_.contains(e.x, e.y))
    {
        if (onClose) onClose();
        return;
    }

    if (e.y < HEADER_H)
    {
        dragger_.startDraggingComponent(this, e);
        isDraggingPanel_ = true;
        return;
    }

    for (int i = 0; i < genres_.size() && i < 32; ++i)
    {
        if (genreRects_[i].contains(e.x, e.y))
        {
            selectedGenre_ = genres_[i];
            listScrollY_ = 0;
            auto visible = visiblePatternIndices();
            if (!visible.empty())
                selectedPattern_ = visible.front();
            repaint();
            return;
        }
    }

    for (int i : visiblePatternIndices())
    {
        if (patterns_[(size_t)i].rect.contains(e.x, e.y))
        {
            selectedPattern_ = i;
            repaint();
            return;
        }
    }

    if (applyBtnRect_.contains(e.x, e.y))
        if (auto* p = selectedPattern())
            if (onApplyPattern)
                onApplyPattern(p->def);
}

void PatternsPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingPanel_)
        dragger_.dragComponent(this, e, nullptr);
}

void PatternsPanel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    listScrollY_ = juce::jmax(0, listScrollY_ - (int)(wheel.deltaY * 80.0f));
    repaint();
}
