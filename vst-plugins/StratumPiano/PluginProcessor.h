#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <memory>
#include <vector>

class StratumPianoAudioProcessor final : public juce::AudioProcessor
{
public:
    StratumPianoAudioProcessor();
    ~StratumPianoAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Stratum Piano"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    int getLoadedSampleCount() const { return loadedSampleCount_; }
    juce::String getSampleFolderPath() const { return sampleFolder_.getFullPathName(); }

private:
    struct PianoSample
    {
        int rootNote = 60;
        double sampleRate = 44100.0;
        std::shared_ptr<juce::AudioBuffer<float>> buffer;
    };

    struct Voice
    {
        bool active = false;
        bool releasing = false;
        bool usingSample = false;
        int note = 60;
        int sampleIndex = -1;
        double frequency = 261.625565;
        double samplePosition = 0.0;
        double sampleStep = 1.0;
        double phase = 0.0;
        double phase2 = 0.0;
        double phase3 = 0.0;
        double age = 0.0;
        double releaseAge = 0.0;
        float velocity = 1.0f;
        float releaseLevel = 0.0f;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static double midiToHz(int midiNote);
    static int noteNameToMidi(const juce::String& name);
    void startNote(int midiNote, float velocity);
    void stopNote(int midiNote);
    void loadSamples();
    juce::File findSampleFolder() const;
    int findClosestSampleIndex(int midiNote) const;
    float renderSynthVoice(Voice& voice, float tone, float decay);
    float renderSampleVoice(Voice& voice);

    std::array<Voice, 32> voices_;
    std::vector<PianoSample> samples_;
    juce::AudioFormatManager formatManager_;
    juce::File sampleFolder_;
    double sampleRate_ = 44100.0;
    int loadedSampleCount_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StratumPianoAudioProcessor)
};
