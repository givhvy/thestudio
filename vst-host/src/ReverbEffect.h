#pragma once

#include <juce_dsp/juce_dsp.h>

class ReverbEffect
{
public:
    ReverbEffect();
    ~ReverbEffect();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer);
    void reset();

    void setRoomSize (float size);    // 0.0 to 1.0
    void setDamping (float damping);  // 0.0 to 1.0
    void setWetLevel (float wet);     // 0.0 to 1.0
    void setDryLevel (float dry);     // 0.0 to 1.0
    void setWidth (float width);      // 0.0 to 1.0
    void setFreezeMode (bool freeze);

    float getRoomSize() const { return roomSize; }
    float getDamping() const { return damping; }
    float getWetLevel() const { return wetLevel; }
    float getDryLevel() const { return dryLevel; }
    float getWidth() const { return width; }
    bool getFreezeMode() const { return freezeMode; }

private:
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters params;
    bool isPrepared = false;

    float roomSize = 0.5f;
    float damping = 0.5f;
    float wetLevel = 0.3f;
    float dryLevel = 0.7f;
    float width = 1.0f;
    bool freezeMode = false;

    void updateParameters();
};
