#include "SynthEngine.h"

SynthEngine::SynthEngine()
{
    masterGain = std::make_unique<juce::dsp::Gain<float>>();
    masterGain->setGainLinear(0.8f);
    
    reverb = std::make_unique<juce::dsp::Reverb>();
    reverbParams.roomSize = 0.5f;
    reverbParams.damping = 0.5f;
    reverbParams.wetLevel = 0.0f;
    reverbParams.dryLevel = 1.0f;
    reverbParams.width = 1.0f;
    reverb->setParameters(reverbParams);
}

SynthEngine::~SynthEngine()
{
}

void SynthEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    spec.sampleRate = device->getCurrentSampleRate();
    spec.maximumBlockSize = device->getCurrentBufferSizeSamples();
    spec.numChannels = 2;
    
    masterGain->prepare(spec);
    reverb->prepare(spec);
    
    isPrepared = true;
}

void SynthEngine::audioDeviceStopped()
{
    masterGain->reset();
    reverb->reset();
    isPrepared = false;
}

void SynthEngine::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData, int numInputChannels,
    float* const* outputChannelData, int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& context)
{
    if (!isPrepared) return;
    
    // Clear output
    for (int ch = 0; ch < numOutputChannels; ++ch)
        juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    
    juce::AudioBuffer<float> buffer(outputChannelData, numOutputChannels, numSamples);
    juce::dsp::AudioBlock<float> block(buffer);
    
    // Process active voices
    juce::ScopedLock sl(voiceLock);
    currentTime += numSamples / spec.sampleRate;
    
    // Remove finished voices
    voices.erase(std::remove_if(voices.begin(), voices.end(),
        [this](const auto& voice) {
            return !voice->active || (currentTime - voice->startTime) >= voice->duration;
        }), voices.end());
    
    // Process each voice
    for (auto& voice : voices)
    {
        if (!voice->active) continue;
        
        double age = currentTime - voice->startTime;
        if (age >= voice->duration)
        {
            voice->active = false;
            continue;
        }
        
        // Simple envelope
        float env = 1.0f;
        if (age < 0.01) env = age / 0.01f; // Attack
        else if (age > voice->duration - 0.05) env = (voice->duration - age) / 0.05f; // Release
        if (env < 0) env = 0;
        
        voice->envelope->setGainLinear(env);
    }
    
    // Apply master gain
    masterGain->process(juce::dsp::ProcessContextReplacing<float>(block));
    
    // Apply reverb if enabled
    if (reverbEnabled)
        reverb->process(juce::dsp::ProcessContextReplacing<float>(block));
}

void SynthEngine::playKick(double time)
{
    juce::ScopedLock sl(voiceLock);
    auto voice = std::make_unique<Voice>();
    voice->oscillator = std::make_unique<juce::dsp::Oscillator<float>>();
    voice->oscillator->setFrequency(60.0f);
    voice->oscillator->initialise([](float x) { return std::sin(x); });
    voice->gain = std::make_unique<juce::dsp::Gain<float>>();
    voice->gain->setGainLinear(1.0f);
    voice->envelope = std::make_unique<juce::dsp::Gain<float>>();
    voice->startTime = time;
    voice->duration = 0.2;
    voice->active = true;
    
    voice->oscillator->prepare(spec);
    voice->gain->prepare(spec);
    voice->envelope->prepare(spec);
    voice->oscillator->reset();
    voice->gain->reset();
    voice->envelope->reset();
    
    voices.push_back(std::move(voice));
}

void SynthEngine::playSnare(double time)
{
    juce::ScopedLock sl(voiceLock);
    auto voice = std::make_unique<Voice>();
    voice->oscillator = std::make_unique<juce::dsp::Oscillator<float>>();
    voice->oscillator->setFrequency(200.0f);
    voice->oscillator->initialise([](float x) { return std::sin(x); });
    voice->gain = std::make_unique<juce::dsp::Gain<float>>();
    voice->gain->setGainLinear(0.8f);
    voice->envelope = std::make_unique<juce::dsp::Gain<float>>();
    voice->startTime = time;
    voice->duration = 0.15;
    voice->active = true;
    
    voice->oscillator->prepare(spec);
    voice->gain->prepare(spec);
    voice->envelope->prepare(spec);
    voice->oscillator->reset();
    voice->gain->reset();
    voice->envelope->reset();
    
    voices.push_back(std::move(voice));
}

void SynthEngine::playHihat(double time, bool open)
{
    juce::ScopedLock sl(voiceLock);
    auto voice = std::make_unique<Voice>();
    voice->oscillator = std::make_unique<juce::dsp::Oscillator<float>>();
    voice->oscillator->setFrequency(800.0f);
    voice->oscillator->initialise([](float x) { return std::sin(x); });
    voice->gain = std::make_unique<juce::dsp::Gain<float>>();
    voice->gain->setGainLinear(0.4f);
    voice->envelope = std::make_unique<juce::dsp::Gain<float>>();
    voice->startTime = time;
    voice->duration = open ? 0.3 : 0.05;
    voice->active = true;
    
    voice->oscillator->prepare(spec);
    voice->gain->prepare(spec);
    voice->envelope->prepare(spec);
    voice->oscillator->reset();
    voice->gain->reset();
    voice->envelope->reset();
    
    voices.push_back(std::move(voice));
}

void SynthEngine::playClap(double time)
{
    juce::ScopedLock sl(voiceLock);
    auto voice = std::make_unique<Voice>();
    voice->oscillator = std::make_unique<juce::dsp::Oscillator<float>>();
    voice->oscillator->setFrequency(1000.0f);
    voice->oscillator->initialise([](float x) { return std::sin(x); });
    voice->gain = std::make_unique<juce::dsp::Gain<float>>();
    voice->gain->setGainLinear(0.6f);
    voice->envelope = std::make_unique<juce::dsp::Gain<float>>();
    voice->startTime = time;
    voice->duration = 0.1;
    voice->active = true;
    
    voice->oscillator->prepare(spec);
    voice->gain->prepare(spec);
    voice->envelope->prepare(spec);
    voice->oscillator->reset();
    voice->gain->reset();
    voice->envelope->reset();
    
    voices.push_back(std::move(voice));
}

void SynthEngine::playTone(double frequency, double time, double duration, float velocity)
{
    juce::ScopedLock sl(voiceLock);
    auto voice = std::make_unique<Voice>();
    voice->oscillator = std::make_unique<juce::dsp::Oscillator<float>>();
    voice->oscillator->setFrequency(frequency);
    voice->oscillator->initialise([](float x) { return std::sin(x); });
    voice->gain = std::make_unique<juce::dsp::Gain<float>>();
    voice->gain->setGainLinear(velocity);
    voice->envelope = std::make_unique<juce::dsp::Gain<float>>();
    voice->startTime = time;
    voice->duration = duration;
    voice->active = true;
    
    voice->oscillator->prepare(spec);
    voice->gain->prepare(spec);
    voice->envelope->prepare(spec);
    voice->oscillator->reset();
    voice->gain->reset();
    voice->envelope->reset();
    
    voices.push_back(std::move(voice));
}

void SynthEngine::setReverbWetLevel(float wetLevel)
{
    reverbParams.wetLevel = juce::jlimit(0.0f, 1.0f, wetLevel);
    reverbParams.dryLevel = 1.0f - wetLevel;
    reverb->setParameters(reverbParams);
}

void SynthEngine::setReverbEnabled(bool enabled)
{
    reverbEnabled = enabled;
}
