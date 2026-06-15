#include "AudioEngine.h"

AudioEngine::AudioEngine(juce::AudioIODeviceCallback& callback)
    : callback_(callback)
{}

AudioEngine::~AudioEngine()
{
    stop();
}

bool AudioEngine::start(double sampleRate, int bufferSize)
{
    // Open the device once with the desired setup, instead of opening with
    // defaults and immediately re-opening via setAudioDeviceSetup (the
    // double-open roughly doubled audio startup time on Windows).
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.sampleRate = sampleRate;
    setup.bufferSize = bufferSize;
    juce::String err = deviceManager_.initialise(0, 2, nullptr, true, {}, &setup);
    if (err.isNotEmpty())
    {
        juce::Logger::writeToLog("AudioEngine: device init error: " + err);
        return false;
    }

    deviceManager_.addAudioCallback(&callback_);
    juce::Logger::writeToLog("AudioEngine: started — SR=" + juce::String(getSampleRate())
                              + " buf=" + juce::String(getBufferSize()));
    return true;
}

void AudioEngine::stop()
{
    deviceManager_.removeAudioCallback(&callback_);
    deviceManager_.closeAudioDevice();
}

double AudioEngine::getSampleRate() const
{
    if (auto* dev = deviceManager_.getCurrentAudioDevice())
        return dev->getCurrentSampleRate();
    return 44100.0;
}

int AudioEngine::getBufferSize() const
{
    if (auto* dev = deviceManager_.getCurrentAudioDevice())
        return dev->getCurrentBufferSizeSamples();
    return 512;
}
