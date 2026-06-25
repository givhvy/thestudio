#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <array>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
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
    void sendMidiNote(int slotId, int channel, int note, int velocity, bool on, int sampleOffset = 0);

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
    juce::String getNativeEffectName(int effectId) const;
    juce::String getNativeEffectType(int effectId) const;
    float getNativeEffectParam(int effectId, const juce::String& param) const;
    void setNativeEffectParam(int effectId, const juce::String& param, float value);
    void showNativeEffectEditor(int effectId);

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
    std::function<void(int effectId, const juce::String& name)> onNativeEffectEditorRequested;

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

    // ── Headphone Flat EQ (monitoring-only correction) ──────────
    // Applies a correction curve to flatten the Sony WH-1000XM5
    // frequency response for studio-quality mixing. Runs as the
    // very last processing step, after master reverb.
    void setHeadphoneFlatEnabled(bool enabled);
    bool isHeadphoneFlatEnabled() const;

    // Synth controls
    // trackIdx = -1 routes to master; >= 0 routes through the corresponding mixer track.
    void playSynthKick(double time, int trackIdx = -1);
    void playSynthSnare(double time, int trackIdx = -1);
    void playSynthHihat(double time, bool open, int trackIdx = -1);
    void playSynthClap(double time, int trackIdx = -1);
    void playSynthTone(double frequency, double time, double duration, float velocity, int trackIdx = -1);
    void playSynthPiano(int midiNote, double time, double duration, float velocity, int trackIdx = -1);
    void playSynthBass(int midiNote, double time, double duration, float velocity, int trackIdx = -1);
    void setSynthReverbWetLevel(float wetLevel);
    void setSynthReverbEnabled(bool enabled);

    // Sample preview (one-shot playback of any audio file). trackIdx routes
    // the voice through the corresponding mixer track's plugin chain
    // (-1 = master bus).
    void playSampleFile(const juce::File& file, int trackIdx = -1, double startOffsetSeconds = 0.0, float gain = 1.0f,
                        double playbackRate = 1.0, double maxTimelineSeconds = -1.0,
                        int outputDelaySamples = 0);

    // FL-style sample channel processing baked into a cached buffer (reverse,
    // normalize, edge fades, ping-pong). Pitch stays a resample ratio applied
    // via playbackRate so the processed buffer is pitch-independent.
    struct SampleRenderOptions
    {
        bool  reverse   = false;
        bool  normalize = false;
        bool  pingPong  = false;
        bool  declick   = true;
        float fadeInMs  = 0.0f;
        float fadeOutMs = 0.0f;
        juce::String hash() const
        {
            const int flags = (reverse ? 1 : 0) | (normalize ? 2 : 0)
                            | (pingPong ? 4 : 0) | (declick ? 8 : 0);
            return juce::String(flags) + "_" + juce::String(fadeInMs, 1)
                 + "_" + juce::String(fadeOutMs, 1);
        }
    };
    void playSampleFileOpt(const juce::File& file, int trackIdx, float gain,
                           double playbackRate, const SampleRenderOptions& opt);
    void playSamplePreview(const juce::File& file);
    void stopSamplePlayback();
    void stopSamplePlaybackImmediate();
    void stopSampleFileVoices(const juce::File& file, bool immediate = true);
    // Live-update the gain of any currently-playing voices for this file
    // (optionally restricted to one mixer track). Lets the UI change a clip's
    // volume in real time without re-triggering playback.
    void setSampleVoiceGain(const juce::File& file, float gain, int trackIdx = -1);
    void stopSampleVoicesOnTrack(const juce::File& file, int trackIdx, bool immediate = true);
    void sendAllNotesOff(int slotId, int channel = 1);
    void flushAllPluginNotesOff();
    void clearTransientPlayback();

    // ── Per-track plugin routing ─────────────────────────────────
    // Called by Mixer whenever a track's FX list changes. slotIds are the
    // plugin slot ids (from loadPlugin) in the order they should be applied.
    void setTrackChain(int trackIdx, std::vector<int> slotIds);
    void setFxSlotBypassed(int slotId, bool bypassed);
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

    // ── Perf instrumentation ─────────────────────────────────────────
    // Worst-case audio render time (ms) since the last read. As a fraction of
    // the block budget (numSamples/sampleRate) this is the audio CPU headroom.
    double readAndResetAudioPeakMs() noexcept
    {
        return audioPeakMs_.exchange(0.0, std::memory_order_relaxed);
    }
    double getAudioBlockBudgetMs() const noexcept
    {
        return (sampleRate_ > 0.0 && lastBlockSamples_ > 0)
                   ? (1000.0 * lastBlockSamples_ / sampleRate_) : 0.0;
    }
    int getActiveSampleVoiceCount() const noexcept
    {
        return activeVoiceCount_.load(std::memory_order_relaxed);
    }

    // ── Master-output spectrum analyzer (FL-style) ───────────────────
    // The audio thread runs an FFT on the final master mix and stores
    // log-spaced, smoothed magnitude bins (0..1). The UI reads them
    // lock-free for the transport-bar visualizer.
    static constexpr int kSpectrumBins = 48;
    void readSpectrum(float* outBins) const noexcept
    {
        for (int i = 0; i < kSpectrumBins; ++i)
            outBins[i] = spectrumBins_[(size_t)i].load(std::memory_order_relaxed);
    }
    float getMasterOutputLevel() const noexcept
    {
        return masterLevel_.load(std::memory_order_relaxed);
    }

    // Decode a sample into the cache ahead of time (off the message thread) so
    // the first playback never has to decode mid-stream — which would stall the
    // message thread that also runs the sequencer clock. Safe to call repeatedly.
    void prewarmSampleCache(const juce::File& file);

    // ── Auto-sidechain (kick ducks 808/bass) ─────────────────────────
    // Real envelope-follower ducking: the source track's amplitude drives a
    // gain reduction on the target track each audio block.
    // depth 0..1 = how much the target ducks; attack/release in ms.
    void setSidechain(bool enabled, int sourceTrack, int targetTrack,
                       float depth = 0.7f, float attackMs = 5.0f, float releaseMs = 180.0f);
    bool isSidechainEnabled() const { return sidechainEnabled_.load(std::memory_order_relaxed); }

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
        enum class Type { Reverb, Delay, ParametricEq, SoftClipper };

        struct Biquad
        {
            double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
            std::array<double, 2> z1 {};
            std::array<double, 2> z2 {};

            void reset();
            void setIdentity();
            void setCoefficients(double nb0, double nb1, double nb2, double na0, double na1, double na2);
            void setHighPass(double sr, double freq, double q);
            void setLowPass(double sr, double freq, double q);
            void setPeak(double sr, double freq, double q, double gainDb);
            void setLowShelf(double sr, double freq, double gainDb);
            void setHighShelf(double sr, double freq, double gainDb);
            void process(juce::AudioBuffer<float>& buffer);
        };

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

        std::array<Biquad, 7> eqBands;
        std::array<float, 7> eqFreq { 28.0f, 95.0f, 280.0f, 850.0f, 2500.0f, 9200.0f, 19500.0f };
        std::array<float, 7> eqGain { 0.0f, 1.5f, -1.5f, 0.0f, 1.2f, 1.0f, 0.0f };
        std::array<float, 7> eqQ    { 0.72f, 0.72f, 1.0f, 1.0f, 1.1f, 0.72f, 0.72f };
        float clipThreshold = 0.78f;
        float clipPreGain = 1.45f;
        float clipPostGain = 0.82f;

        void prepare(double sr, int maxBlockSize);
        void updateEqCoefficients();
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
        bool bass = false;
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
        double endPosition = 0.0;  // stop at this source-sample index (exclusive)
        double step       = 1.0;   // sourceSR / deviceSR — advance per output sample
        bool   active     = true;
        int    trackIdx   = -1;    // -1 = goes straight to master bus
        juce::String sourcePath;
        int    outputSamplesRemaining = -1; // wall-clock cap (-1 = none)
        int    ageSamples = 0;
        int    attackSamples = 1;
        int    releaseSamples = 1;
        int    releaseRemaining = 0;
        int    startDelaySamples = 0;
        float  gain = 1.0f;
    };
    struct CachedSample
    {
        std::shared_ptr<juce::AudioBuffer<float>> buffer;
        double sampleRate = 44100.0;   // native sample rate of the file
    };
    std::vector<SampleVoice> sampleVoices_;
    std::unordered_map<juce::String, CachedSample> sampleCache_;
    // Cache of FL-processed (reverse/normalize/fade/ping-pong) buffers, keyed
    // by "<path>|<optionsHash>". Source sample rate matches the base sample.
    std::unordered_map<juce::String, CachedSample> processedSampleCache_;
    juce::CriticalSection sampleLock_;

    // Per-track plugin chain routing (track idx → ordered slot ids).
    std::unordered_map<int, std::vector<int>> trackChains_;
    std::unordered_set<int> bypassedFxSlots_;
    int  masterTrackIdx_ = -1;
    juce::CriticalSection routingLock_;

    // Map plugin slot id → mixer track index for VST instrument routing.
    std::unordered_map<int, int> slotTrackMap_;
    juce::CriticalSection slotTrackLock_;

    std::vector<TrackControl> trackControls_;
    juce::CriticalSection trackControlLock_;
    static constexpr int maxMeterTracks_ = 128;
    std::array<std::atomic<float>, maxMeterTracks_> trackLevels_ {};

    // ── Reusable audio-render scratch (audio thread only; no per-block heap
    //    allocation after warm-up) ─────────────────────────────────────────
    std::unordered_map<int, juce::AudioBuffer<float>> trackBuffers_;
    juce::AudioBuffer<float> masterBuf_;
    std::unordered_set<int>  touchedSet_;        // tracks that got audio this block
    std::vector<int>         touchedTracks_;     // same, ordered, for iteration
    std::vector<TrackControl> controlsSnapshot_;
    std::unordered_set<int>  bypassedSnapshot_;
    std::vector<int>         masterChainSnapshot_;

    // ── Sidechain state ─────────────────────────────────────────
    std::atomic<bool> sidechainEnabled_ { false };
    std::atomic<int>  sidechainSource_ { -1 };  // kick track index
    std::atomic<int>  sidechainTarget_ { -1 };  // 808/bass track index
    std::atomic<float> sidechainDepth_ { 0.7f };
    std::atomic<float> sidechainAttackMs_ { 5.0f };
    std::atomic<float> sidechainReleaseMs_ { 180.0f };
    float sidechainEnv_ = 0.0f;                 // envelope follower (audio thread only)

    // Perf counters (written on audio thread, read on message thread).
    std::atomic<double> audioPeakMs_ { 0.0 };
    std::atomic<int>    lastBlockSamples_ { 0 };
    std::atomic<int>    activeVoiceCount_ { 0 };

    // ── Spectrum-analyzer state ──────────────────────────────────
    // FFT runs on the audio thread; bins read lock-free by the UI.
    static constexpr int kFftOrder = 10;            // 1024-point FFT
    static constexpr int kFftSize  = 1 << kFftOrder;
    juce::dsp::FFT spectrumFft_ { kFftOrder };
    std::array<float, (size_t)kFftSize * 2> fftWorkspace_ {};  // audio thread scratch
    std::array<float, (size_t)kFftSize>     fftInput_ {};      // accumulating frame
    int fftFill_ = 0;
    std::array<std::atomic<float>, (size_t)kSpectrumBins> spectrumBins_ {};
    std::atomic<float> masterLevel_ { 0.0f };
    void pushSamplesToSpectrum(const float* const* out, int numOut, int numSamples);
    // Ducks the target track buffer using the source track as trigger. Operates
    // on the reusable trackBuffers_/masterBuf_ members; only touched tracks duck.
    void applySidechainDucking(int numSamples);

    // ── Headphone Flat EQ state ─────────────────────────────────
    std::atomic<bool> headphoneFlatEnabled_ { false };
    static constexpr int kHpCorrectionBands = 8;
    std::array<NativeEffectSlot::Biquad, kHpCorrectionBands> hpCorrectionBands_;
    float hpCorrectionPreamp_ = 1.0f;   // linear gain
    bool  hpCorrectionPrepared_ = false;
    void  prepareHeadphoneCorrection(double sampleRate);

    juce::File getPluginCacheFile() const;
    void loadPluginCache();
    void savePluginCache() const;
};
