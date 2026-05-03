#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <memory>
#include <unordered_map>

// Manages loading, scanning, and running VST3/VST2 plugins.
// Each loaded plugin lives in a PluginInstance slot (indexed by integer id).
class PluginHost : public juce::AudioIODeviceCallback
{
public:
    PluginHost();
    ~PluginHost() override;

    // Scan a directory for plugins and return JSON array of descriptors
    juce::var scanDirectory(const juce::String& path);

    // Load a plugin by its identifier string (from scan results)
    // Returns slot id on success, -1 on failure
    int loadPlugin(const juce::String& fileOrIdentifier, juce::String& errorOut);

    // Unload a plugin slot
    void unloadPlugin(int slotId);

    // Send a MIDI note on/off to a slot
    void sendMidiNote(int slotId, int channel, int note, int velocity, bool on);

    // Send a block of MIDI CC to a slot
    void sendMidiCC(int slotId, int channel, int cc, int value);

    // Set a parameter value (0.0 - 1.0)
    void setParameter(int slotId, int paramIndex, float value);

    // Get all parameter info for a slot
    juce::var getParameters(int slotId);

    // Get a JSON list of all loaded slots
    juce::var getLoadedPlugins() const;

    // Show/hide plugin editor window
    void showEditor(int slotId, bool show);

    // AudioIODeviceCallback — renders all active plugin slots into output
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData, int numInputChannels,
        float* const* outputChannelData, int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    struct PluginSlot
    {
        int id;
        std::unique_ptr<juce::AudioPluginInstance> instance;
        juce::MidiBuffer pendingMidi;
        juce::AudioBuffer<float> buffer;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        std::unique_ptr<juce::DocumentWindow> editorWindow;
        juce::CriticalSection lock;
    };

    juce::AudioPluginFormatManager formatManager_;
    juce::KnownPluginList pluginList_;
    double sampleRate_ = 44100.0;
    int blockSize_ = 512;
    int nextSlotId_ = 1;
    std::unordered_map<int, std::unique_ptr<PluginSlot>> slots_;
    mutable juce::CriticalSection slotsLock_;
};
