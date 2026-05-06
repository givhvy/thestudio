#pragma once
#include <juce_audio_devices/juce_audio_devices.h>

// AudioEngine: wraps AudioDeviceManager setup.
// PluginHost implements the audio callback directly;
// this class just handles device lifecycle.
class AudioEngine
{
public:
    explicit AudioEngine(juce::AudioIODeviceCallback& callback);
    ~AudioEngine();

    bool start(double sampleRate = 44100.0, int bufferSize = 512);
    void stop();
    double getSampleRate() const;
    int getBufferSize() const;
    juce::AudioIODeviceCallback& getCallback() { return callback_; }

private:
    juce::AudioDeviceManager deviceManager_;
    juce::AudioIODeviceCallback& callback_;
};
