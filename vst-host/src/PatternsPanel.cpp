#include "PatternsPanel.h"
#include "DrumMidiParser.h"
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
                                                 bool useFullPresetRows = false,
                                                 bool artistPattern = false)
    {
        PatternsPanel::PatternDefinition p;
        p.id = id;
        p.presetId = presetId;
        p.genre = genre;
        p.title = title;
        p.feel = feel;
        p.bpm = bpm;
        p.useFullPresetRows = useFullPresetRows || p.presetId.isNotEmpty();
        p.artistPattern = artistPattern;

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

    PatternsPanel::PatternDefinition makeArtistPattern(const juce::String& id,
                                                       const juce::String& presetId,
                                                       const juce::String& artist,
                                                       const juce::String& title,
                                                       const juce::String& feel,
                                                       int bpm,
                                                       std::initializer_list<std::initializer_list<int>> rows)
    {
        return makePattern(id, presetId, artist, title, feel, bpm, rows, true, true);
    }

    PatternsPanel::PatternDefinition makePopularSongPattern(const juce::String& id,
                                                            const juce::String& presetId,
                                                            const juce::String& artist,
                                                            const juce::String& title,
                                                            const juce::String& feel,
                                                            int bpm,
                                                            std::initializer_list<std::initializer_list<int>> rows,
                                                            const juce::String& exactMidiRelativePath = {})
    {
        auto p = makePattern(id, presetId, artist, title, feel, bpm, rows, true, false);
        p.popularSongPattern = true;
        p.exactMidiRelativePath = exactMidiRelativePath;
        return p;
    }

    void hydrateExactMidiPatterns(std::vector<PatternsPanel::PatternDefinition>& patterns)
    {
        for (auto& p : patterns)
        {
            const auto midiFile = PatternsPanel::resolveExactMidiFile(p);
            if (!midiFile.existsAsFile())
                continue;

            const auto parsed = DrumMidiParser::parseFile(midiFile);
            if (!parsed.ok)
                continue;

            p.rows = parsed.previewGrid;
            if (parsed.bpm > 0)
                p.bpm = parsed.bpm;

            if (!p.feel.containsIgnoreCase("exact MIDI"))
                p.feel = "exact MIDI · " + p.feel;

            if (!p.title.containsIgnoreCase("exact MIDI"))
                p.title = p.title.replace(" - inspired drums", " - exact MIDI");
        }
    }

    juce::String patternPresetLabel(const juce::String& presetId)
    {
        if (presetId.equalsIgnoreCase("boom_bap")) return "Boom Bap";
        if (presetId.equalsIgnoreCase("hiphop")) return "Hip Hop";
        if (presetId.equalsIgnoreCase("trap")) return "Trap";
        if (presetId.equalsIgnoreCase("drill")) return "Drill";
        if (presetId.equalsIgnoreCase("house")) return "House";
        if (presetId.equalsIgnoreCase("rnb")) return "R&B";
        if (presetId.equalsIgnoreCase("lofi")) return "Lo-Fi";
        if (presetId.equalsIgnoreCase("rock")) return "Rock";
        if (presetId.equalsIgnoreCase("detroit")) return "Detroit Flint";
        if (presetId.equalsIgnoreCase("afrobeat")) return "Afrobeat";
        if (presetId.equalsIgnoreCase("reggaeton")) return "Reggaeton";
        if (presetId.equalsIgnoreCase("jersey")) return "Jersey Club";
        if (presetId.equalsIgnoreCase("ukg")) return "UK Garage";
        if (presetId.equalsIgnoreCase("dnb")) return "Drum & Bass";
        if (presetId.equalsIgnoreCase("techno")) return "Techno";
        if (presetId.equalsIgnoreCase("phonk")) return "Phonk";
        if (presetId.equalsIgnoreCase("memphis")) return "Memphis";
        if (presetId.equalsIgnoreCase("funk")) return "Funk";
        return presetId;
    }

    const char* midiLaneName(int lane)
    {
        static const char* names[] = { "Kick", "Snare", "Hat", "Perc / Open Hat" };
        return names[juce::jlimit(0, 3, lane)];
    }

    juce::File popularSongMidiFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Stratum DAW");
        dir.createDirectory();
        return dir.getChildFile("popular-song-drum-midi.json");
    }

    int drumLaneForMidiPitch(int pitch)
    {
        return DrumMidiParser::drumLaneForPitch(pitch);
    }

    bool readMidiDrumGrid(const juce::File& file, PatternsPanel::PatternGrid& grid)
    {
        const auto parsed = DrumMidiParser::parseFile(file);
        if (!parsed.ok)
            return false;

        grid = parsed.previewGrid;
        return true;
    }

    std::vector<PatternsPanel::PatternDefinition> loadUserPopularSongPatterns()
    {
        std::vector<PatternsPanel::PatternDefinition> result;
        const auto file = popularSongMidiFile();
        if (!file.existsAsFile())
            return result;

        const auto parsed = juce::JSON::parse(file);
        if (auto* arr = parsed.getArray())
        {
            for (const auto& item : *arr)
            {
                if (!item.isObject())
                    continue;

                PatternsPanel::PatternDefinition p;
                p.id = item.getProperty("id", {}).toString();
                p.presetId = item.getProperty("presetId", "trap").toString();
                p.genre = item.getProperty("artist", "Imported").toString();
                p.title = item.getProperty("title", "Imported MIDI").toString();
                p.feel = item.getProperty("feel", "imported exact MIDI").toString();
                p.bpm = (int)item.getProperty("bpm", 90);
                p.exactMidiRelativePath = item.getProperty("exactMidiRelativePath", {}).toString();
                p.exactMidiStoredPath = item.getProperty("exactMidiStoredPath", {}).toString();
                p.useFullPresetRows = true;
                p.popularSongPattern = true;

                if (auto* rows = item.getProperty("rows", {}).getArray())
                {
                    for (int r = 0; r < rows->size() && r < 4; ++r)
                        if (auto* steps = rows->getReference(r).getArray())
                            for (int s = 0; s < steps->size() && s < 16; ++s)
                                p.rows[(size_t)r][(size_t)s] = (int)steps->getReference(s);
                }

                if (p.id.isEmpty())
                    p.id = "user_popular_" + p.genre + "_" + p.title;
                result.push_back(p);
            }
        }

        return result;
    }

    void saveUserPopularSongPattern(const PatternsPanel::PatternDefinition& pattern)
    {
        auto patterns = loadUserPopularSongPatterns();
        bool replaced = false;
        for (auto& existing : patterns)
        {
            if (existing.id == pattern.id)
            {
                existing = pattern;
                replaced = true;
                break;
            }
        }
        if (!replaced)
            patterns.push_back(pattern);

        juce::Array<juce::var> arr;
        for (const auto& p : patterns)
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("id", p.id);
            obj->setProperty("presetId", p.presetId);
            obj->setProperty("artist", p.genre);
            obj->setProperty("title", p.title);
            obj->setProperty("feel", p.feel);
            obj->setProperty("bpm", p.bpm);
            obj->setProperty("exactMidiRelativePath", p.exactMidiRelativePath);
            obj->setProperty("exactMidiStoredPath", p.exactMidiStoredPath);

            juce::Array<juce::var> rows;
            for (int r = 0; r < 4; ++r)
            {
                juce::Array<juce::var> steps;
                for (int s = 0; s < 16; ++s)
                    steps.add(p.rows[(size_t)r][(size_t)s]);
                rows.add(juce::var(steps));
            }
            obj->setProperty("rows", juce::var(rows));
            arr.add(juce::var(obj));
        }

        popularSongMidiFile().replaceWithText(juce::JSON::toString(juce::var(arr), false));
    }
}

juce::File PatternsPanel::resolveExactMidiFile(const PatternDefinition& pattern)
{
    if (pattern.exactMidiStoredPath.isNotEmpty())
    {
        juce::File stored(pattern.exactMidiStoredPath);
        if (stored.existsAsFile())
            return stored;
    }

    if (pattern.exactMidiRelativePath.isNotEmpty())
        return DrumMidiParser::resolveBundledMidi(pattern.exactMidiRelativePath);

    return {};
}

bool PatternsPanel::hasExactMidiFile(const PatternDefinition& pattern)
{
    return resolveExactMidiFile(pattern).existsAsFile();
}

std::vector<PatternsPanel::PatternDefinition> PatternsPanel::getPatternLibrary()
{
    std::vector<PatternDefinition> library {
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
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0}}),
    makePattern("trap_atl", "trap", "Trap", "ATL Rolls", "rolling hats, simple 808", 140,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,1},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0}}),
    makePattern("trap_dark", "trap", "Trap", "Dark Half-Time", "wide gaps, heavy bounce", 130,
        {{1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,1},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0}}),

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
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),

    makeArtistPattern("artist_isaiah_rashad_suns_tirade", "boom_bap", "Isaiah Rashad", "Laidback Soul Bounce", "loose kick, dusty rim pocket", 88,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,1,1,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makeArtistPattern("artist_isaiah_rashad_cilvia", "boom_bap", "Isaiah Rashad", "Cilvia Pocket", "behind-the-grid hats, warm snare", 86,
        {{1,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,0,1,1,0,1,0,1,0,0,1,1,0,1,0},
         {0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0}}),
    makeArtistPattern("artist_isaiah_rashad_watery", "boom_bap", "Isaiah Rashad", "Watery Pocket", "soft kick answers, lazy closed hats", 88,
        {{1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,0,1,1,0,1,0,1,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,0}}),
    makeArtistPattern("artist_isaiah_rashad_parked", "boom_bap", "Isaiah Rashad", "Parked Car Swing", "rim ghost notes with headnod kick", 84,
        {{1,0,0,0,0,0,0,1,0,0,1,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,0,1,1,0,1,0,1,0,0,1,1,0,1,0},
         {0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0}}),
    makeArtistPattern("artist_isaiah_rashad_knoxville", "boom_bap", "Isaiah Rashad", "Knoxville Crawl", "slow bounce, late perc lift", 82,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,1,1,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,0,1,1,0,1,0},
         {0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makeArtistPattern("artist_isaiah_rashad_heavenly", "rnb", "Isaiah Rashad", "Heavenly R&B Dust", "soft snare, R&B-leaning hats", 76,
        {{1,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
         {1,0,1,0,1,0,1,0,1,0,0,1,1,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makeArtistPattern("artist_isaiah_rashad_smoke", "boom_bap", "Isaiah Rashad", "Smoky Backroom", "minimal kick, roomy snare", 86,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,0,1,0,0,1,0,1,0,0,1,1,0,1,0},
         {0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0}}),
    makeArtistPattern("artist_isaiah_rashad_bounce_low", "hiphop", "Isaiah Rashad", "Low Bounce", "tight kick doubles, relaxed hat lane", 90,
        {{1,0,0,0,0,1,1,0,0,0,1,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0}}),
    makeArtistPattern("artist_isaiah_rashad_tape_warm", "boom_bap", "Isaiah Rashad", "Tape Warmth", "vinyl swing and short open hat", 88,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,0,1,1,0,1,0,1,0,0,1},
         {0,0,1,0,0,0,0,0,0,1,0,0,1,0,0,0}}),
    makeArtistPattern("artist_isaiah_rashad_lazy_river", "rnb", "Isaiah Rashad", "Lazy River", "half-sung pocket, rim movement", 74,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
         {1,0,0,1,1,0,1,0,1,0,1,1,1,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0}}),
    makeArtistPattern("artist_isaiah_rashad_front_porch", "boom_bap", "Isaiah Rashad", "Front Porch Knock", "loose kick before the two", 89,
        {{1,0,0,0,0,1,0,0,1,0,0,1,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0}}),
    makeArtistPattern("artist_isaiah_rashad_after_hours", "boom_bap", "Isaiah Rashad", "After Hours Dust", "deep swing, sparse percussion", 85,
        {{1,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,1,1,0,0,0},
         {1,0,0,1,1,0,1,0,1,0,0,1,0,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,0}}),
    makeArtistPattern("artist_kendrick_live_knock", "hiphop", "Kendrick Lamar", "West Coast Live Knock", "dry clap, tight kick answers", 94,
        {{1,0,0,0,0,0,1,0,1,0,0,1,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,0,1,1,1,0},
         {0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makeArtistPattern("artist_drake_40_space", "rnb", "Drake / 40", "Sparse R&B Swing", "soft kick, late snare, open space", 72,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
         {1,0,0,1,1,0,1,0,1,0,0,1,1,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makeArtistPattern("artist_travis_atmospheric", "trap", "Travis Scott", "Atmospheric Trap", "half-time snare, rolling hat details", 140,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,1},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0}}),
    makeArtistPattern("artist_metro_minimal", "trap", "Metro Boomin", "Minimal Menace", "simple drums, heavy negative space", 140,
        {{1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makeArtistPattern("artist_tyler_dusty", "funk", "Tyler, The Creator", "Dusty Funk Loop", "chunky kick and clap pocket", 98,
        {{1,0,0,1,0,0,1,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,1,0,1,0,0,0},
         {1,0,1,0,1,1,0,1,1,0,1,0,1,1,0,1},
         {0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0}}),
    makeArtistPattern("artist_j_dilla_late", "boom_bap", "J Dilla", "Late Hat Swing", "lazy timing feel, snare stays solid", 90,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,1,0,1,0,0,1},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makeArtistPattern("artist_premo_ny", "boom_bap", "DJ Premier", "NY Chop Drums", "hard snare, no wasted hits", 92,
        {{1,0,0,0,0,0,0,1,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0}}),
    makeArtistPattern("artist_alchemist_sparse", "boom_bap", "The Alchemist", "Sparse Drum Break", "dry vinyl pocket, lots of room", 84,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,0}}),
    makeArtistPattern("artist_timbaland_bounce", "rnb", "Timbaland", "Syncopated R&B Bounce", "offset percussion and clipped hat rhythm", 100,
        {{1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,1,0,1,0,0,0},
         {1,0,1,1,1,0,1,0,1,0,1,1,1,0,1,0},
         {0,0,1,0,0,1,0,0,0,0,1,0,0,1,0,0}}),

    makePopularSongPattern("popular_drake_nonstop", "trap", "Drake", "Nonstop - inspired drums", "minimal bounce, clipped snare space", 78,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,1},
         {0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0}}),
    makePopularSongPattern("popular_drake_gods_plan", "rnb", "Drake", "God's Plan - inspired drums", "soft kick answers, clean hat pocket", 77,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
         {1,0,1,0,1,0,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePopularSongPattern("popular_drake_jimmy_cooks", "trap", "Drake", "Jimmy Cooks - inspired drums", "late kick, 16th hat lift", 82,
        {{1,0,0,0,0,0,0,0,1,0,0,1,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,1},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePopularSongPattern("popular_drake_one_dance", "afrobeat", "Drake", "One Dance - inspired drums", "Afrobeats sway, syncopated kick, offbeat hats", 96,
        {{1,0,0,0,0,0,1,0,0,1,0,0,0,0,1,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1},
         {1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,0}}),
    makePopularSongPattern("popular_drake_hotline_bling", "rnb", "Drake", "Hotline Bling - inspired drums", "sparse kick-snare, near-empty hat lane", 86,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}),
    makePopularSongPattern("popular_drake_in_my_feelings", "trap", "Drake", "In My Feelings - inspired drums", "New Orleans bounce kick stomp, rolling groove", 75,
        {{1,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePopularSongPattern("popular_drake_started_from_bottom", "trap", "Drake", "Started From The Bottom - inspired drums", "early trap punch, doubled kick accent", 100,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePopularSongPattern("popular_drake_money_in_the_grave", "trap", "Drake", "Money In The Grave - inspired drums", "dark heavy kick, rolling hat triplets", 97,
        {{1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0},
         {0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,0}}),
    makePopularSongPattern("popular_drake_rich_flex", "trap", "Drake", "Rich Flex - inspired drums", "Metro dark trap, punchy double kick", 91,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,0,1,0,1,1,1,0,1,1},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}}),

    // Drake — ICEMAN (2026) — drop exact .mid into data/popular-song-midi/Drake/ICEMAN/
    makePopularSongPattern("popular_drake_iceman_make_them_cry", "rnb", "Drake", "Make Them Cry (ICEMAN) - inspired drums", "soul opener, 40 sparse kick-snare pocket", 73,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
         {0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0}},
        "Drake/ICEMAN/make-them-cry.mid"),
    makePopularSongPattern("popular_drake_iceman_dust", "rnb", "Drake", "Dust (ICEMAN) - inspired drums", "dusty vinyl feel, late ghost snare", 76,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
         {1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}},
        "Drake/ICEMAN/dust.mid"),
    makePopularSongPattern("popular_drake_iceman_whisper_my_name", "rnb", "Drake", "Whisper My Name (ICEMAN) - inspired drums", "intimate R&B sway, soft hat whispers", 82,
        {{1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
        "Drake/ICEMAN/whisper-my-name.mid"),
    makePopularSongPattern("popular_drake_iceman_janice_stfu", "trap", "Drake", "Janice STFU (ICEMAN) - inspired drums", "Billboard #1 bounce, clipped snare lift", 126,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0}},
        "Drake/ICEMAN/janice-stfu.mid"),
    makePopularSongPattern("popular_drake_iceman_ran_to_atlanta", "trap", "Drake", "Ran To Atlanta (ICEMAN) - inspired drums", "Future collab, sliding 808 kick pocket", 142,
        {{1,0,0,0,0,0,0,0,1,0,0,1,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0}},
        "Drake/ICEMAN/ran-to-atlanta.mid"),
    makePopularSongPattern("popular_drake_iceman_shabang", "trap", "Drake", "Shabang (ICEMAN) - inspired drums", "Tay Keith hard trap, triplet hat rush", 148,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,1},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}},
        "Drake/ICEMAN/shabang.mid"),
    makePopularSongPattern("popular_drake_iceman_make_them_pay", "rnb", "Drake", "Make Them Pay (ICEMAN) - inspired drums", "cinematic drama, wide kick gaps", 84,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0}},
        "Drake/ICEMAN/make-them-pay.mid"),
    makePopularSongPattern("popular_drake_iceman_burning_bridges", "trap", "Drake", "Burning Bridges (ICEMAN) - inspired drums", "midtempo trap, syncopated kick answers", 88,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}},
        "Drake/ICEMAN/burning-bridges.mid"),
    makePopularSongPattern("popular_drake_iceman_national_treasures", "trap", "Drake", "National Treasures (ICEMAN) - inspired drums", "Oz bounce, hot-cold kick flip", 92,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0}},
        "Drake/ICEMAN/national-treasures.mid"),
    makePopularSongPattern("popular_drake_iceman_bs_on_table", "trap", "Drake", "B's On The Table (ICEMAN) - inspired drums", "21 Savage dark Metro, snare on 3", 130,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
         {1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
        "Drake/ICEMAN/bs-on-the-table.mid"),
    makePopularSongPattern("popular_drake_iceman_what_did_i_miss", "trap", "Drake", "What Did I Miss? (ICEMAN) - inspired drums", "comeback single, tight 126 bounce", 126,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}},
        "Drake/ICEMAN/what-did-i-miss.mid"),
    makePopularSongPattern("popular_drake_iceman_plot_twist", "trap", "Drake", "Plot Twist (ICEMAN) - inspired drums", "offset kick twist, rolling 16th hats", 94,
        {{1,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,1},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0}},
        "Drake/ICEMAN/plot-twist.mid"),
    makePopularSongPattern("popular_drake_iceman_2_hard_4_radio", "trap", "Drake", "2 Hard 4 The Radio (ICEMAN) - inspired drums", "radio-hard 105 trap, punchy kick stack", 105,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,1,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0}},
        "Drake/ICEMAN/2-hard-4-the-radio.mid"),
    makePopularSongPattern("popular_drake_iceman_make_them_remember", "trap", "Drake", "Make Them Remember (ICEMAN) - inspired drums", "diss-track energy, heavy downbeat kick", 98,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0}},
        "Drake/ICEMAN/make-them-remember.mid"),
    makePopularSongPattern("popular_drake_iceman_little_birdie", "trap", "Drake", "Little Birdie (ICEMAN) - inspired drums", "light skip groove, airy hat flutter", 110,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
        "Drake/ICEMAN/little-birdie.mid"),
    makePopularSongPattern("popular_drake_iceman_dont_worry", "rnb", "Drake", "Don't Worry (ICEMAN) - inspired drums", "relaxed 40 pocket, warm kick-snare space", 80,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
        "Drake/ICEMAN/dont-worry.mid"),
    makePopularSongPattern("popular_drake_iceman_firm_friends", "rnb", "Drake", "Firm Friends (ICEMAN) - inspired drums", "groove-forward R&B, syncopated perc", 86,
        {{1,0,0,1,0,0,0,0,1,0,0,1,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,0},
         {0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0}},
        "Drake/ICEMAN/firm-friends.mid"),
    makePopularSongPattern("popular_drake_iceman_make_them_know", "rnb", "Drake", "Make Them Know (ICEMAN) - inspired drums", "album closer, wide open soul drums", 78,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
         {0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0}},
        "Drake/ICEMAN/make-them-know.mid"),

    makePopularSongPattern("popular_gunna_pushin_p", "trap", "Gunna", "pushin P - inspired drums", "slick ATL hat roll, relaxed kick", 78,
        {{1,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1}}),
    makePopularSongPattern("popular_gunna_drip_too_hard", "trap", "Gunna", "Drip Too Hard - inspired drums", "bright hats, simple snare pocket", 112,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0}}),

    makePopularSongPattern("popular_young_thug_hot", "trap", "Young Thug", "Hot - inspired drums", "anthem snare, rolling hat energy", 112,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0}}),
    makePopularSongPattern("popular_young_thug_pick_up_phone", "trap", "Young Thug", "Pick Up The Phone - inspired drums", "bouncy snare, airy hat lane", 136,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1}}),

    makePopularSongPattern("popular_travis_sicko_mode", "trap", "Travis Scott", "Sicko Mode - inspired drums", "hard halftime section, busy hats", 155,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,1,0,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,1},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0}}),
    makePopularSongPattern("popular_future_mask_off", "trap", "Future", "Mask Off - inspired drums", "wide open flute pocket, sparse hits", 75,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePopularSongPattern("popular_lil_baby_freestyle", "trap", "Lil Baby", "Freestyle - inspired drums", "urgent hats, clean ATL bounce", 88,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePopularSongPattern("popular_21_savage_bank_account", "trap", "21 Savage", "Bank Account - inspired drums", "dry snare, dark simple bounce", 75,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0}}),
    makePopularSongPattern("popular_21_savage_rockstar", "trap", "21 Savage", "Rockstar - inspired drums", "half-time snare on 3, wide open pocket", 160,
        {{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}),
    makePopularSongPattern("popular_21_savage_a_lot", "trap", "21 Savage", "A Lot - inspired drums", "melancholy Metro trap, subtle hat rolls", 135,
        {{1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,0,1,1,1,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}),
    makePopularSongPattern("popular_21_savage_no_heart", "trap", "21 Savage", "No Heart - inspired drums", "Southside darkness, ultra-sparse grid", 130,
        {{1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,1,1,0,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}),
    makePopularSongPattern("popular_21_savage_redrum", "trap", "21 Savage", "redrum - inspired drums", "haunting sample pocket, snare ghost on 4-e", 130,
        {{1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
         {1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0},
         {0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0}}),
    makePopularSongPattern("popular_21_savage_knife_talk", "trap", "21 Savage", "Knife Talk - inspired drums", "Memphis-flipped dark grid, hard kick drop", 100,
        {{1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,1},
         {0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0}}),
    makePopularSongPattern("popular_21_savage_immortal", "trap", "21 Savage", "Immortal - inspired drums", "hard late kick, tight triplet hat runs", 140,
        {{1,0,0,0,0,0,0,0,1,0,0,1,0,0,0,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,0,1,1,1,0},
         {0,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0}}),
    makePopularSongPattern("popular_carti_magnolia", "trap", "Playboi Carti", "Magnolia - inspired drums", "playful hats, light snare bounce", 163,
        {{1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0},
         {0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0},
         {1,0,1,0,1,1,1,0,1,0,1,1,1,0,1,0},
         {0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0}})
    };

    hydrateExactMidiPatterns(library);
    return library;
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
        if (def.popularSongPattern)
        {
            if (!popularArtists_.contains(def.genre))
                popularArtists_.add(def.genre);
        }
        else if (def.artistPattern)
        {
            if (!artists_.contains(def.genre))
                artists_.add(def.genre);
        }
        else if (!genres_.contains(def.genre))
        {
            genres_.add(def.genre);
        }

        const auto midiGenre = patternPresetLabel(def.presetId);
        if (midiGenre.isNotEmpty() && !midiGenres_.contains(midiGenre))
            midiGenres_.add(midiGenre);
    }

    auto userLoaded = loadUserPopularSongPatterns();
    hydrateExactMidiPatterns(userLoaded);
    for (const auto& def : userLoaded)
    {
        patterns_.push_back({ def, {} });
        if (!popularArtists_.contains(def.genre))
            popularArtists_.add(def.genre);
    }
}

std::vector<int> PatternsPanel::visiblePatternIndices() const
{
    std::vector<int> ids;
    for (int i = 0; i < (int)patterns_.size(); ++i)
    {
        const auto& def = patterns_[(size_t)i].def;
        if (activeLibraryTab_ == 3)
        {
            if (def.popularSongPattern && def.genre == selectedPopularArtist_)
                ids.push_back(i);
            continue;
        }

        if (activeLibraryTab_ == 2)
        {
            if (!def.popularSongPattern && patternPresetLabel(def.presetId) == selectedMidiGenre_)
                ids.push_back(i);
            continue;
        }

        const juce::String selected = activeLibraryTab_ == 1 ? selectedArtist_ : selectedGenre_;
        if (!def.popularSongPattern && def.artistPattern == (activeLibraryTab_ == 1) && def.genre == selected)
            ids.push_back(i);
    }

    if (activeLibraryTab_ == 3)
    {
        std::stable_sort(ids.begin(), ids.end(), [this](int a, int b)
        {
            const auto& ta = patterns_[(size_t)a].def.title;
            const auto& tb = patterns_[(size_t)b].def.title;
            const bool aIceman = ta.contains("(ICEMAN)");
            const bool bIceman = tb.contains("(ICEMAN)");
            if (aIceman != bIceman)
                return aIceman;
            return ta.compareIgnoreCase(tb) < 0;
        });
    }

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
    auto tabs = body.removeFromTop(34);
    genresTabRect_ = tabs.removeFromLeft(96).reduced(0, 4);
    artistTabRect_ = tabs.removeFromLeft(96).reduced(6, 4);
    midiTabRect_ = tabs.removeFromLeft(96).reduced(6, 4);
    popularSongsTabRect_ = tabs.removeFromLeft(132).reduced(6, 4);
    auto drawTab = [&](juce::Rectangle<int> r, const juce::String& label, bool selected)
    {
        g.setColour(selected ? Theme::orange1.withAlpha(0.95f) : juce::Colour(0xff1c1c20));
        g.fillRoundedRectangle(r.toFloat(), 5.0f);
        g.setColour(selected ? juce::Colours::black.withAlpha(0.7f) : juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 5.0f, 1.0f);
        g.setColour(selected ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(label, r, juce::Justification::centred);
    };
    drawTab(genresTabRect_, "GENRES", activeLibraryTab_ == 0);
    drawTab(artistTabRect_, "ARTIST", activeLibraryTab_ == 1);
    drawTab(midiTabRect_, "MIDI", activeLibraryTab_ == 2);
    drawTab(popularSongsTabRect_, "POPULAR SONGS", activeLibraryTab_ == 3);
    importMidiBtnRect_ = {};

    auto left = body.removeFromLeft(GENRE_W);
    auto right = body.withTrimmedLeft(10);

    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    g.drawText(activeLibraryTab_ == 1 ? "ARTISTS"
                                      : (activeLibraryTab_ == 3 ? "POPULAR ARTISTS" : "GENRES"),
               left.removeFromTop(20), juce::Justification::centredLeft);

    const auto& leftItems = activeLibraryTab_ == 1 ? artists_
                            : (activeLibraryTab_ == 2 ? midiGenres_
                               : (activeLibraryTab_ == 3 ? popularArtists_ : genres_));
    const auto selectedLeftItem = activeLibraryTab_ == 1 ? selectedArtist_
                                  : (activeLibraryTab_ == 2 ? selectedMidiGenre_
                                     : (activeLibraryTab_ == 3 ? selectedPopularArtist_ : selectedGenre_));
    for (int i = 0; i < leftItems.size() && i < 32; ++i)
    {
        auto r = left.removeFromTop(28).reduced(0, 3);
        genreRects_[i] = r;
        const bool selected = leftItems[i] == selectedLeftItem;
        g.setColour(selected ? Theme::orange1.withAlpha(0.95f) : juce::Colour(0xff1c1c20));
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
        g.setColour(selected ? juce::Colours::black.withAlpha(0.65f) : juce::Colours::black);
        g.drawRoundedRectangle(r.toFloat(), 4.0f, 1.0f);
        g.setColour(selected ? juce::Colours::black : Theme::zinc200);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(leftItems[i], r.reduced(9, 0), juce::Justification::centredLeft);
    }

    if (activeLibraryTab_ == 2)
    {
        left.removeFromTop(10);
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText("CHANNEL", left.removeFromTop(20), juce::Justification::centredLeft);

        for (int i = 0; i < 4; ++i)
        {
            auto r = left.removeFromTop(28).reduced(0, 3);
            midiLaneRects_[i] = r;
            const bool selected = i == selectedMidiLane_;
            g.setColour(selected ? Theme::orange1.withAlpha(0.95f) : juce::Colour(0xff1c1c20));
            g.fillRoundedRectangle(r.toFloat(), 4.0f);
            g.setColour(selected ? juce::Colours::black.withAlpha(0.65f) : juce::Colours::black);
            g.drawRoundedRectangle(r.toFloat(), 4.0f, 1.0f);
            g.setColour(selected ? juce::Colours::black : Theme::zinc200);
            g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
            g.drawText(midiLaneName(i), r.reduced(9, 0), juce::Justification::centredLeft);
        }
    }

    auto top = right.removeFromTop(24);
    g.setColour(Theme::zinc500);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    if (activeLibraryTab_ == 3)
    {
        importMidiBtnRect_ = top.removeFromRight(112).reduced(0, 2);
        g.setColour(Theme::orange1.withAlpha(0.95f));
        g.fillRoundedRectangle(importMidiBtnRect_.toFloat(), 4.0f);
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.drawRoundedRectangle(importMidiBtnRect_.toFloat(), 4.0f, 1.0f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f).withStyle("Bold"));
        g.drawText("IMPORT MIDI", importMidiBtnRect_, juce::Justification::centred);
        g.setColour(Theme::zinc500);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
    }
    g.drawText(activeLibraryTab_ == 1 ? "ARTIST DRUM PATTERNS"
                                      : (activeLibraryTab_ == 2 ? (selectedMidiGenre_ + " " + midiLaneName(selectedMidiLane_) + " MIDI")
                                         : (activeLibraryTab_ == 3 ? "POPULAR SONG DRUM MIDI" : "MIDI PATTERNS")),
               top, juce::Justification::centredLeft);

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
    g.drawText(activeLibraryTab_ == 2 ? "APPLY LANE" : "APPLY TO RACK", applyBtnRect_, juce::Justification::centred);

    g.setColour(Theme::zinc600);
    g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(9.5f));
    g.drawText(activeLibraryTab_ == 1
                   ? "Artist tab uses original artist-inspired patterns and matching kit presets."
                   : (activeLibraryTab_ == 2
                          ? "MIDI tab is the shared source used by Channel Rack right-click menus."
                          : (activeLibraryTab_ == 3
                                ? "Drop .mid files in popular-song-midi/Drake/ICEMAN/ or use IMPORT MIDI for exact drums."
                                : "Pattern dashboard for AI drum MIDI presets.")),
               footer, juce::Justification::centredLeft);
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

    if (activeLibraryTab_ == 2)
    {
        const int markerY = area.getY() + 6 + selectedMidiLane_ * 24;
        g.setColour(Theme::orange1);
        g.drawRoundedRectangle((float)area.getX(), (float)markerY, (float)area.getWidth(), 18.0f, 3.0f, 1.5f);
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

    if (activeLibraryTab_ == 3 && importMidiBtnRect_.contains(e.x, e.y))
    {
        importPopularSongMidi();
        return;
    }

    if (genresTabRect_.contains(e.x, e.y)
        || artistTabRect_.contains(e.x, e.y)
        || midiTabRect_.contains(e.x, e.y)
        || popularSongsTabRect_.contains(e.x, e.y))
    {
        activeLibraryTab_ = popularSongsTabRect_.contains(e.x, e.y) ? 3
                          : (midiTabRect_.contains(e.x, e.y) ? 2
                             : (artistTabRect_.contains(e.x, e.y) ? 1 : 0));
        listScrollY_ = 0;
        auto visible = visiblePatternIndices();
        if (!visible.empty())
            selectedPattern_ = visible.front();
        repaint();
        return;
    }

    const auto& leftItems = activeLibraryTab_ == 1 ? artists_
                            : (activeLibraryTab_ == 2 ? midiGenres_
                               : (activeLibraryTab_ == 3 ? popularArtists_ : genres_));
    for (int i = 0; i < leftItems.size() && i < 32; ++i)
    {
        if (genreRects_[i].contains(e.x, e.y))
        {
            if (activeLibraryTab_ == 1)
                selectedArtist_ = leftItems[i];
            else if (activeLibraryTab_ == 2)
                selectedMidiGenre_ = leftItems[i];
            else if (activeLibraryTab_ == 3)
                selectedPopularArtist_ = leftItems[i];
            else
                selectedGenre_ = leftItems[i];
            listScrollY_ = 0;
            auto visible = visiblePatternIndices();
            if (!visible.empty())
                selectedPattern_ = visible.front();
            repaint();
            return;
        }
    }

    if (activeLibraryTab_ == 2)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (midiLaneRects_[i].contains(e.x, e.y))
            {
                selectedMidiLane_ = i;
                repaint();
                return;
            }
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
            if (activeLibraryTab_ == 2 && onApplyChannelPattern)
                onApplyChannelPattern(p->def, selectedMidiLane_);
            else if (onApplyPattern)
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

void PatternsPanel::importPopularSongMidi()
{
    midiChooser_ = std::make_unique<juce::FileChooser>(
        "Import exact drum MIDI",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.mid;*.midi");

    midiChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (!file.existsAsFile())
                return;

            PatternGrid grid {};
            const auto parsed = DrumMidiParser::parseFile(file);
            if (!parsed.ok)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                    "Import MIDI",
                    "Could not read drum notes from this MIDI file.");
                return;
            }
            grid = parsed.previewGrid;

            const juce::String artistFolder = selectedPopularArtist_.isNotEmpty() ? selectedPopularArtist_ : juce::String("Imported");
            const auto destDir = DrumMidiParser::userPopularSongMidiRoot().getChildFile(artistFolder);
            destDir.createDirectory();
            const auto destFile = destDir.getChildFile(file.getFileName());
            file.copyFileTo(destFile);

            PatternDefinition p;
            p.id = "user_popular_" + selectedPopularArtist_ + "_" + file.getFileNameWithoutExtension();
            p.presetId = "trap";
            p.genre = artistFolder;
            p.title = file.getFileNameWithoutExtension() + " - exact MIDI";
            p.feel = "imported exact MIDI";
            p.bpm = parsed.bpm > 0 ? parsed.bpm : 90;
            p.rows = grid;
            p.useFullPresetRows = true;
            p.popularSongPattern = true;
            p.exactMidiStoredPath = destFile.getFullPathName();
            p.exactMidiRelativePath = artistFolder + "/" + file.getFileName();

            saveUserPopularSongPattern(p);
            if (!popularArtists_.contains(p.genre))
                popularArtists_.add(p.genre);

            for (int i = 0; i < (int)patterns_.size(); ++i)
            {
                if (patterns_[(size_t)i].def.id == p.id)
                {
                    patterns_[(size_t)i].def = p;
                    selectedPattern_ = i;
                    listScrollY_ = 0;
                    repaint();
                    return;
                }
            }

            patterns_.push_back({ p, {} });
            selectedPattern_ = (int)patterns_.size() - 1;
            listScrollY_ = 0;
            repaint();
        });
}
