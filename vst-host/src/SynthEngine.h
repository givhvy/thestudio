#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <memory>

class SynthEngine : public juce::AudioIODeviceCallback
{
public:
    SynthEngine();
    ~SynthEngine() override;

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData, int numInputChannels,
        float* const* outputChannelData, int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // Sound triggers
    void playKick(double time);
    void playSnare(double time);
    void playHihat(double time, bool open);
    void playClap(double time);
    void playTone(double frequency, double time, double duration, float velocity);

    // Reverb control
    void setReverbWetLevel(float wetLevel);
    void setReverbEnabled(bool enabled);

private:
    struct Voice
    {
        std::unique_ptr<juce::dsp::Oscillator<float>> oscillator;
        std::unique_ptr<juce::dsp::Gain<float>> gain;
        std::unique_ptr<juce::dsp::Gain<float>> envelope;
        double startTime = 0;
        double duration = 0;
        bool active = false;
    };

    juce::dsp::ProcessSpec spec;
    std::vector<std::unique_ptr<Voice>> voices;
    juce::CriticalSection voiceLock;
    
    // Master output
    std::unique_ptr<juce::dsp::Gain<float>> masterGain;
    std::unique_ptr<juce::dsp::Reverb> reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    bool reverbEnabled = false;
    
    // Current time
    double currentTime = 0;
    bool isPrepared = false;
};
