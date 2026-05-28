#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

struct LoopBpmRange
{
    double minBpm = 0.0;
    double maxBpm = 0.0;
    bool valid = false;
};

inline LoopBpmRange parseLoopBpmRangeFromFileName(const juce::String& name)
{
    LoopBpmRange result;
    const juce::String lower = name.toLowerCase();
    const int bpmPos = lower.indexOf("bpm");
    if (bpmPos < 0)
        return result;

    const juce::String before = lower.substring(0, bpmPos).trimEnd();
    const int dash = before.lastIndexOfChar('-');
    if (dash > 0)
    {
        const juce::String left = before.substring(0, dash).trimEnd();
        const juce::String right = before.substring(dash + 1).trim();

        int leftEnd = left.length();
        int leftStart = leftEnd;
        while (leftStart > 0 && juce::CharacterFunctions::isDigit(left[leftStart - 1]))
            --leftStart;

        int rightStart = 0;
        while (rightStart < right.length() && juce::CharacterFunctions::isDigit(right[rightStart]))
            ++rightStart;

        if (leftStart < leftEnd && rightStart > 0)
        {
            const int a = left.substring(leftStart, leftEnd).getIntValue();
            const int b = right.substring(0, rightStart).getIntValue();
            if (a >= 40 && a <= 240 && b >= 40 && b <= 240)
            {
                result.minBpm = (double)juce::jmin(a, b);
                result.maxBpm = (double)juce::jmax(a, b);
                result.valid = true;
                return result;
            }
        }
    }

    int end = bpmPos - 1;
    while (end >= 0 && lower[end] == ' ')
        --end;

    int start = end;
    while (start >= 0 && juce::CharacterFunctions::isDigit(lower[start]))
        --start;

    if (end >= start + 1)
    {
        const int bpm = lower.substring(start + 1, end + 1).getIntValue();
        if (bpm >= 40 && bpm <= 240)
        {
            result.minBpm = (double)bpm;
            result.maxBpm = (double)bpm;
            result.valid = true;
        }
    }

    return result;
}

inline double parseBpmFromFileName(const juce::String& name)
{
    const auto range = parseLoopBpmRangeFromFileName(name);
    if (!range.valid)
        return 0.0;
    if (range.minBpm == range.maxBpm)
        return range.minBpm;
    return (range.minBpm + range.maxBpm) * 0.5;
}

inline bool loopFileMatchesTargetBpm(const juce::File& file, double targetBpm, double tolerance = 5.0)
{
    const auto range = parseLoopBpmRangeFromFileName(file.getFileNameWithoutExtension());
    if (!range.valid)
        return false;

    if (range.minBpm == range.maxBpm)
        return std::abs(range.minBpm - targetBpm) <= tolerance;

    return targetBpm >= range.minBpm && targetBpm <= range.maxBpm;
}
