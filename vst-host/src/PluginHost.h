#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <array>
#include <atomic>
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

    // Add a user-supplied folder to the persistent scan list and immediately
    // scan it for plugins. The list is reloaded on every scanDefaultLocations()
    // call. Used by the "Scan a folder..." picker so things like Kontakt
    // Portable that live outside the standard VST3 paths get picked up.
    void addPluginScanPath(const juce::String& folderPath);

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

    // Native app effects. These are internal processors, not VST plugins.
    // IDs are negative so they can live in the same mixer FX chain as VST slots.
    int createNativeEffect(const juce::String& type);
    void unloadNativeEffect(int effectId);
    bool isNativeEffectId(int effectId) const { return effectId < 0; }

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

    // Offline bounce: caller must hold getRenderLock() (export path).
    void renderAudioBlock(float* const* out, int numOut, int numSamples);

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
    // trackIdx = -1 routes to master; >= 0 routes through the corresponding mixer track.
    void playSynthKick(double time, int trackIdx = -1);
    void playSynthSnare(double time, int trackIdx = -1);
    void playSynthHihat(double time, bool open, int trackIdx = -1);
    void playSynthClap(double time, int trackIdx = -1);
    void playSynthTone(double frequency, double time, double duration, float velocity, int trackIdx = -1);
    void playSynthPiano(int midiNote, double time, double duration, float velocity, int trackIdx = -1);
    void setSynthReverbWetLevel(float wetLevel);
    void setSynthReverbEnabled(bool enabled);

    // Sample preview (one-shot playback of any audio file). trackIdx routes
    // the voice through the corresponding mixer track's plugin chain
    // (-1 = master bus).
    void playSampleFile(const juce::File& file, int trackIdx = -1, double startOffsetSeconds = 0.0, float gain = 1.0f, double playbackRate = 1.0);
    void playSamplePreview(const juce::File& file);
    void stopSamplePlayback();
    void clearTransientPlayback();

    // ── Per-track plugin routing ─────────────────────────────────
    // Called by Mixer whenever a track's FX list changes. slotIds are the
    // plugin slot ids (from loadPlugin) in the order they should be applied.
    void setTrackChain(int trackIdx, std::vector<int> slotIds);
    // Designate which mixer track is the master bus (its chain runs LAST,
    // after all per-track chains have been summed).
    void setMasterTrackIdx(int idx);

    // Map a loaded plugin slot to a mixer track so its rendered audio is
    // routed through that track's FX chain instead of the master bus.
    void setSlotTrack(int slotId, int trackIdx);
    void clearSlotTrack(int slotId);

    struct TrackControl
    {
        float volume = 0.8f;
        float pan = 0.0f;
        bool muted = false;
        bool solo = false;
    };
    void setTrackControls(std::vector<TrackControl> controls);
    float getTrackLevel(int trackIdx) const;
    juce::CriticalSection& getRenderLock() noexcept { return renderLock_; }

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

    struct NativeEffectSlot
    {
        enum class Type { Reverb, Delay };

        int id = -1;
        Type type = Type::Reverb;
        juce::String name;
        bool prepared = false;
        double sampleRate = 44100.0;

        juce::dsp::Reverb reverb;
        juce::dsp::Reverb::Parameters reverbParams;

        juce::AudioBuffer<float> delayBuffer;
        int delayWrite = 0;
        float delayMs = 320.0f;
        float feedback = 0.34f;
        float wet = 0.30f;
        float dry = 1.0f;

        void prepare(double sr, int maxBlockSize);
        void reset();
        void process(juce::AudioBuffer<float>& buffer);
    };

    juce::AudioPluginFormatManager formatManager_;
    juce::KnownPluginList pluginList_;
    double sampleRate_ = 44100.0;
    int blockSize_ = 512;
    int nextSlotId_ = 1;
    std::unordered_map<int, std::unique_ptr<PluginSlot>> slots_;
    mutable juce::CriticalSection slotsLock_;
    juce::CriticalSection renderLock_;

    int nextNativeEffectId_ = -1;
    std::unordered_map<int, std::unique_ptr<NativeEffectSlot>> nativeEffects_;
    mutable juce::CriticalSection nativeEffectsLock_;
    
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
        bool piano = false;
        int trackIdx = -1;  // -1 = master bus, >= 0 = mixer track
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
        int    ageSamples = 0;
        int    attackSamples = 1;
        int    releaseSamples = 1;
        int    releaseRemaining = 0;
        float  gain = 1.0f;
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

    // Map plugin slot id → mixer track index for VST instrument routing.
    std::unordered_map<int, int> slotTrackMap_;
    juce::CriticalSection slotTrackLock_;

    std::vector<TrackControl> trackControls_;
    juce::CriticalSection trackControlLock_;
    static constexpr int maxMeterTracks_ = 128;
    std::array<std::atomic<float>, maxMeterTracks_> trackLevels_ {};
};
