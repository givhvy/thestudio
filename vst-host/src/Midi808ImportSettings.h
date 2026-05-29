#pragma once

#include <juce_core/juce_core.h>
#include <functional>

class Midi808ImportSettings
{
public:
    bool lowestNotesOnly = true;
    bool foldToC4C6 = true;

    static Midi808ImportSettings& get();

    void load();
    void save() const;

    /** Fold into C4–C6 when enabled; otherwise clamp to 0–127. */
    int applyPitch(int pitch, int previousPitch = -1) const;

    std::function<void()> onChanged;

private:
    Midi808ImportSettings();
};
