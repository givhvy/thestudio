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
    juce::String err = deviceManager_.initialiseWithDefaultDevices(0, 2);
    if (err.isNotEmpty())
    {
        juce::Logger::writeToLog("AudioEngine: device init error: " + err);
        return false;
    }

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager_.getAudioDeviceSetup(setup);
    setup.sampleRate = sampleRate;
    setup.bufferSize = bufferSize;
    err = deviceManager_.setAudioDeviceSetup(setup, true);
    if (err.isNotEmpty())
        juce::Logger::writeToLog("AudioEngine: setup warning: " + err);

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
