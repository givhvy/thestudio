#include "ReverbEffect.h"

ReverbEffect::ReverbEffect()
{
    params.roomSize = roomSize;
    params.damping = damping;
    params.wetLevel = wetLevel;
    params.dryLevel = dryLevel;
    params.width = width;
    params.freezeMode = freezeMode ? 1.0f : 0.0f;
    reverb.setParameters (params);
}

ReverbEffect::~ReverbEffect()
{
}

void ReverbEffect::prepare (const juce::dsp::ProcessSpec& spec)
{
    reverb.prepare (spec);
    isPrepared = true;
}

void ReverbEffect::process (juce::AudioBuffer<float>& buffer)
{
    if (!isPrepared)
        return;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    reverb.process (context);
}

void ReverbEffect::reset()
{
    reverb.reset();
}

void ReverbEffect::setRoomSize (float size)
{
    roomSize = juce::jlimit (0.0f, 1.0f, size);
    updateParameters();
}

void ReverbEffect::setDamping (float dampingValue)
{
    damping = juce::jlimit (0.0f, 1.0f, dampingValue);
    updateParameters();
}

void ReverbEffect::setWetLevel (float wet)
{
    wetLevel = juce::jlimit (0.0f, 1.0f, wet);
    updateParameters();
}

void ReverbEffect::setDryLevel (float dry)
{
    dryLevel = juce::jlimit (0.0f, 1.0f, dry);
    updateParameters();
}

void ReverbEffect::setWidth (float widthValue)
{
    width = juce::jlimit (0.0f, 1.0f, widthValue);
    updateParameters();
}

void ReverbEffect::setFreezeMode (bool freeze)
{
    freezeMode = freeze;
    updateParameters();
}

void ReverbEffect::updateParameters()
{
    params.roomSize = roomSize;
    params.damping = damping;
    params.wetLevel = wetLevel;
    params.dryLevel = dryLevel;
    params.width = width;
    params.freezeMode = freezeMode ? 1.0f : 0.0f;
    reverb.setParameters (params);
}
