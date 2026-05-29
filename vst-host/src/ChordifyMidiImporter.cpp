#include "ChordifyMidiImporter.h"
#include "Midi808ImportSettings.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>

namespace
{
struct RawNote
{
    int pitch = 0;
    double startSec = 0.0;
    double endSec = 0.0;
    int velocity = 100;
};

int foldPitchToC4C6(int pitch, int previous)
{
    pitch = juce::jlimit(0, 127, pitch);
    while (pitch < 60)
        pitch += 12;
    while (pitch > 84)
        pitch -= 12;
    pitch = juce::jlimit(60, 84, pitch);

    if (previous >= 60 && previous <= 84)
    {
        int best = pitch;
        int bestDist = std::abs(pitch - previous);
        for (int candidate : { pitch - 12, pitch, pitch + 12 })
        {
            if (candidate < 60 || candidate > 84)
                continue;
            const int dist = std::abs(candidate - previous);
            if (dist < bestDist)
            {
                best = candidate;
                bestDist = dist;
            }
        }
        return best;
    }

    return pitch;
}

int secondsToStep(double seconds, double bpm)
{
    if (bpm <= 0.0)
        bpm = 120.0;
    return (int) std::lround((seconds / 60.0) * bpm * 4.0);
}

double readMidiBpm(const juce::MidiFile& midi)
{
    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        for (int i = 0; i < midi.getTrack(t)->getNumEvents(); ++i)
        {
            const auto msg = midi.getTrack(t)->getEventPointer(i)->message;
            if (msg.isTempoMetaEvent())
                return msg.getTempoSecondsPerQuarterNote() > 0.0
                           ? 60.0 / msg.getTempoSecondsPerQuarterNote()
                           : 0.0;
        }
    }
    return 0.0;
}

std::vector<RawNote> collectRawNotes(const juce::MidiFile& midi)
{
    std::vector<std::vector<RawNote>> perTrack;
    juce::MidiMessageSequence merged;

    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence trackOnly;
        trackOnly.addSequence(*midi.getTrack(t), 0.0, 0.0, midi.getLastTimestamp());
        trackOnly.updateMatchedPairs();

        std::vector<RawNote> trackNotes;
        for (int i = 0; i < trackOnly.getNumEvents(); ++i)
        {
            auto* holder = trackOnly.getEventPointer(i);
            const auto& msg = holder->message;
            if (! msg.isNoteOn(true))
                continue;

            const double startSec = holder->message.getTimeStamp();
            const double endSec = holder->noteOffObject != nullptr
                                      ? holder->noteOffObject->message.getTimeStamp()
                                      : startSec + 0.25;

            trackNotes.push_back({
                msg.getNoteNumber(),
                startSec,
                juce::jmax(startSec + 0.05, endSec),
                msg.getVelocity(),
            });
        }

        if (! trackNotes.empty())
            perTrack.push_back(std::move(trackNotes));
    }

    if (perTrack.empty())
        return {};

    if (perTrack.size() == 1)
    {
        auto notes = perTrack.front();
        std::sort(notes.begin(), notes.end(), [](const RawNote& a, const RawNote& b)
        {
            if (a.startSec != b.startSec)
                return a.startSec < b.startSec;
            return a.pitch < b.pitch;
        });
        return notes;
    }

    // Chordify exports chord + bass tracks — pick lowest average pitch (bass line).
    size_t bestIdx = 0;
    double bestAvg = 127.0;
    for (size_t i = 0; i < perTrack.size(); ++i)
    {
        double sum = 0.0;
        for (const auto& n : perTrack[i])
            sum += n.pitch;
        const double avg = sum / (double) perTrack[i].size();
        if (avg < bestAvg)
        {
            bestAvg = avg;
            bestIdx = i;
        }
    }

    auto notes = perTrack[bestIdx];
    std::sort(notes.begin(), notes.end(), [](const RawNote& a, const RawNote& b)
    {
        if (a.startSec != b.startSec)
            return a.startSec < b.startSec;
        return a.pitch < b.pitch;
    });
    return notes;
}

std::vector<RawNote> clusterToBassRoots(const std::vector<RawNote>& notes, double clusterSec = 0.09)
{
    std::vector<RawNote> roots;
    if (notes.empty())
        return roots;

    RawNote clusterStart = notes.front();
    int lowestPitch = notes.front().pitch;
    double clusterEnd = notes.front().endSec;
    int clusterVel = notes.front().velocity;

    auto flush = [&]()
    {
        RawNote root;
        root.pitch = lowestPitch;
        root.startSec = clusterStart.startSec;
        root.endSec = clusterEnd;
        root.velocity = clusterVel;
        roots.push_back(root);
    };

    for (size_t i = 1; i < notes.size(); ++i)
    {
        if (notes[i].startSec - clusterStart.startSec <= clusterSec)
        {
            lowestPitch = juce::jmin(lowestPitch, notes[i].pitch);
            clusterEnd = juce::jmax(clusterEnd, notes[i].endSec);
            clusterVel = juce::jmax(clusterVel, notes[i].velocity);
            continue;
        }

        flush();
        clusterStart = notes[i];
        lowestPitch = notes[i].pitch;
        clusterEnd = notes[i].endSec;
        clusterVel = notes[i].velocity;
    }

    flush();
    return roots;
}

std::vector<RawNote> collectAllNotesFromTracks(const juce::MidiFile& midi)
{
    std::vector<RawNote> all;
    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence trackOnly;
        trackOnly.addSequence(*midi.getTrack(t), 0.0, 0.0, midi.getLastTimestamp());
        trackOnly.updateMatchedPairs();

        for (int i = 0; i < trackOnly.getNumEvents(); ++i)
        {
            auto* holder = trackOnly.getEventPointer(i);
            const auto& msg = holder->message;
            if (! msg.isNoteOn(true))
                continue;

            const double startSec = holder->message.getTimeStamp();
            const double endSec = holder->noteOffObject != nullptr
                                      ? holder->noteOffObject->message.getTimeStamp()
                                      : startSec + 0.25;

            all.push_back({
                msg.getNoteNumber(),
                startSec,
                juce::jmax(startSec + 0.05, endSec),
                msg.getVelocity(),
            });
        }
    }

    std::sort(all.begin(), all.end(), [](const RawNote& a, const RawNote& b)
    {
        if (a.startSec != b.startSec)
            return a.startSec < b.startSec;
        return a.pitch < b.pitch;
    });
    return all;
}
}

std::vector<ChordifyMidiImporter::BassNote> ChordifyMidiImporter::import(const juce::File& midiFile,
                                                                          double bpmHint,
                                                                          int maxSteps)
{
    std::vector<BassNote> out;
    if (! midiFile.existsAsFile())
        return out;

    juce::FileInputStream stream(midiFile);
    if (! stream.openedOk())
        return out;

    juce::MidiFile midi;
    if (! midi.readFrom(stream))
        return out;

    midi.convertTimestampTicksToSeconds();

    double bpm = readMidiBpm(midi);
    if (bpm <= 0.0 || bpm < 40.0 || bpm > 240.0)
        bpm = bpmHint > 0.0 ? bpmHint : 120.0;
    else if (bpmHint > 0.0 && std::abs(bpm - bpmHint) / bpmHint <= 0.06)
        bpm = bpmHint;

    const auto& settings = Midi808ImportSettings::get();
    std::vector<RawNote> sourceNotes;
    if (settings.lowestNotesOnly)
    {
        const auto rawNotes = collectRawNotes(midi);
        if (rawNotes.empty())
            return out;
        sourceNotes = clusterToBassRoots(rawNotes);
    }
    else
    {
        sourceNotes = collectAllNotesFromTracks(midi);
        if (sourceNotes.empty())
            return out;
    }

    int previousPitch = -1;
    const int minSteps = settings.lowestNotesOnly ? 2 : 1;

    for (const auto& root : sourceNotes)
    {
        int startStep = juce::jmax(0, secondsToStep(root.startSec, bpm));
        int endStep = juce::jmax(startStep + 1, secondsToStep(root.endSec, bpm));
        int lengthSteps = juce::jmax(minSteps, endStep - startStep);

        if (maxSteps > 0 && startStep >= maxSteps)
            break;
        if (maxSteps > 0 && startStep + lengthSteps > maxSteps)
            lengthSteps = juce::jmax(minSteps, maxSteps - startStep);

        const int pitch = settings.applyPitch(root.pitch, previousPitch);
        previousPitch = pitch;

        if (settings.lowestNotesOnly && ! out.empty() && out.back().pitch == pitch)
        {
            out.back().lengthSteps += lengthSteps;
            continue;
        }

        BassNote note;
        note.pitch = pitch;
        note.startStep = startStep;
        note.lengthSteps = lengthSteps;
        note.velocity = juce::jlimit(70, 115, root.velocity);
        out.push_back(note);
    }

    return out;
}

std::vector<ChordifyMidiImporter::BassNote> ChordifyMidiImporter::importAllTracks(const juce::File& midiFile,
                                                                                   double bpmHint,
                                                                                   int maxSteps)
{
    std::vector<BassNote> out;
    if (! midiFile.existsAsFile())
        return out;

    juce::FileInputStream stream(midiFile);
    if (! stream.openedOk())
        return out;

    juce::MidiFile midi;
    if (! midi.readFrom(stream))
        return out;

    midi.convertTimestampTicksToSeconds();

    double bpm = readMidiBpm(midi);
    if (bpm <= 0.0 || bpm < 40.0 || bpm > 240.0)
        bpm = bpmHint > 0.0 ? bpmHint : 120.0;
    else if (bpmHint > 0.0 && std::abs(bpm - bpmHint) / bpmHint <= 0.06)
        bpm = bpmHint;

    const auto rawNotes = collectAllNotesFromTracks(midi);
    const int minSteps = 1;

    for (const auto& raw : rawNotes)
    {
        int startStep = juce::jmax(0, secondsToStep(raw.startSec, bpm));
        int endStep = juce::jmax(startStep + 1, secondsToStep(raw.endSec, bpm));
        int lengthSteps = juce::jmax(minSteps, endStep - startStep);

        if (maxSteps > 0 && startStep >= maxSteps)
            break;
        if (maxSteps > 0 && startStep + lengthSteps > maxSteps)
            lengthSteps = juce::jmax(minSteps, maxSteps - startStep);

        const auto& settings = Midi808ImportSettings::get();
        BassNote note;
        note.pitch = settings.applyPitch(raw.pitch);
        note.startStep = startStep;
        note.lengthSteps = lengthSteps;
        note.velocity = juce::jlimit(60, 127, raw.velocity);
        out.push_back(note);
    }

    return out;
}
