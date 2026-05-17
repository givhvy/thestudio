#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <unordered_map>
#include "ReverbEffect.h"

// Manages loading, scanning, and running VST3/VST2 plugins.
// Each loaded plugin lives in a PluginInstance slot (indexed by integer id).
class PluginHost : public juce::AudioIODeviceCallback
{
public:
    PluginHost();
    ~PluginHost() override;

    // Scan a directory for plugins and return JSON array of descriptors
    juce::var scanDirectory(const juce::String& path);

    // Scan the OS's default plugin folders (VST3 + VST2). Cheap to call
    // multiple times — each path is only deep-scanned once.
    void scanDefaultLocations();

    // The cumulative list of plugins discovered by previous scan calls.
    const juce::KnownPluginList& getKnownPluginList() const { return pluginList_; }

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

    // Show/hide plugin editor.
    // If onEditorReady / onEditorClosed are set, the editor is created
    // detached and handed to the UI layer for embedding (no native
    // DocumentWindow). Otherwise a native window is created (legacy).
    void showEditor(int slotId, bool show);

    // UI layer hooks — set by MainComponent so plugin editors can be embedded
    // inside the main app instead of opening as separate native windows.
    // editor pointer is non-owning; PluginHost retains lifetime ownership.
    std::function<void(int slotId, juce::AudioProcessorEditor* editor, const juce::String& name)> onEditorReady;
    std::function<void(int slotId)> onEditorClosed;

    // AudioIODeviceCallback — renders all active plugin slots into output
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData, int numInputChannels,
        float* const* outputChannelData, int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // Reverb effect controls
    void setMasterReverbRoomSize(float size);
    void setMasterReverbDamping(float damping);
    void setMasterReverbWetLevel(float wet);
    void setMasterReverbDryLevel(float dry);
    void setMasterReverbWidth(float width);
    void setMasterReverbFreezeMode(bool freeze);
    juce::var getMasterReverbParams() const;

    // Synth controls
    void playSynthKick(double time);
    void playSynthSnare(double time);
    void playSynthHihat(double time, bool open);
    void playSynthClap(double time);
    void playSynthTone(double frequency, double time, double duration, float velocity);
    void setSynthReverbWetLevel(float wetLevel);
    void setSynthReverbEnabled(bool enabled);

    // Sample preview (one-shot playback of any audio file). trackIdx routes
    // the voice through the corresponding mixer track's plugin chain
    // (-1 = master bus).
    void playSampleFile(const juce::File& file, int trackIdx = -1);
    void stopSamplePlayback();

    // ── Per-track plugin routing ─────────────────────────────────
    // Called by Mixer whenever a track's FX list changes. slotIds are the
    // plugin slot ids (from loadPlugin) in the order they should be applied.
    void setTrackChain(int trackIdx, std::vector<int> slotIds);
    // Designate which mixer track is the master bus (its chain runs LAST,
    // after all per-track chains have been summed).
    void setMasterTrackIdx(int idx);

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
    
    std::unique_ptr<ReverbEffect> masterReverb_;
    juce::AudioBuffer<float> reverbBuffer_;
    bool reverbEnabled_ = false;
    
    // Built-in synth voices
    struct SynthVoice {
        double startTime;
        double duration;
        float frequency;
        float velocity;
        bool active;
    };
    std::vector<SynthVoice> synthVoices_;
    juce::CriticalSection synthLock_;
    double currentTime_ = 0;
    std::unique_ptr<juce::dsp::Oscillator<float>> synthOscillator_;
    std::unique_ptr<juce::dsp::Gain<float>> synthGain_;
    std::unique_ptr<juce::dsp::Gain<float>> synthEnvelope_;
    
    // Sample playback (polyphonic voice pool + decoded-buffer cache)
    juce::AudioFormatManager sampleFormatManager_;
    
    struct SampleVoice
    {
        std::shared_ptr<juce::AudioBuffer<float>> buffer;
        double position   = 0.0;   // fractional source-sample index
        double step       = 1.0;   // sourceSR / deviceSR — advance per output sample
        bool   active     = true;
        int    trackIdx   = -1;    // -1 = goes straight to master bus
    };
    struct CachedSample
    {
        std::shared_ptr<juce::AudioBuffer<float>> buffer;
        double sampleRate = 44100.0;   // native sample rate of the file
    };
    std::vector<SampleVoice> sampleVoices_;
    std::unordered_map<juce::String, CachedSample> sampleCache_;
    juce::CriticalSection sampleLock_;

    // Per-track plugin chain routing (track idx → ordered slot ids).
    std::unordered_map<int, std::vector<int>> trackChains_;
    int  masterTrackIdx_ = -1;
    juce::CriticalSection routingLock_;
};
