#include "Midi808ImportSettings.h"

namespace
{
juce::File settingsFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Stratum DAW");
    dir.createDirectory();
    return dir.getChildFile("midi-808-import-settings.json");
}
} // namespace

Midi808ImportSettings::Midi808ImportSettings()
{
    load();
}

Midi808ImportSettings& Midi808ImportSettings::get()
{
    static Midi808ImportSettings instance;
    return instance;
}

void Midi808ImportSettings::load()
{
    const auto file = settingsFile();
    if (! file.existsAsFile())
        return;

    const auto json = juce::JSON::parse(file);
    if (! json.isObject())
        return;

    lowestNotesOnly = (bool) json.getProperty("lowestNotesOnly", true);
    foldToC4C6 = (bool) json.getProperty("foldToC4C6", true);
}

void Midi808ImportSettings::save() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("lowestNotesOnly", lowestNotesOnly);
    obj->setProperty("foldToC4C6", foldToC4C6);
    settingsFile().replaceWithText(juce::JSON::toString(juce::var(obj), true));
}

int Midi808ImportSettings::applyPitch(int pitch, int previousPitch) const
{
    pitch = juce::jlimit(0, 127, pitch);
    if (! foldToC4C6)
        return pitch;

    while (pitch < 60)
        pitch += 12;
    while (pitch > 84)
        pitch -= 12;
    pitch = juce::jlimit(60, 84, pitch);

    if (previousPitch >= 60 && previousPitch <= 84)
    {
        int best = pitch;
        int bestDist = std::abs(pitch - previousPitch);
        for (int candidate : { pitch - 12, pitch, pitch + 12 })
        {
            if (candidate < 60 || candidate > 84)
                continue;
            const int dist = std::abs(candidate - previousPitch);
            if (dist < bestDist)
            {
                best = candidate;
                bestDist = dist;
            }
        }
        pitch = best;
    }

    return pitch;
}
