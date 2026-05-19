#include "PluginProcessor.h"
#include "PluginEditor.h"

StratumGuitarAudioProcessor::StratumGuitarAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager_.registerBasicFormats();
    loadSamples();
}

juce::AudioProcessorValueTreeState::ParameterLayout StratumGuitarAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("gain", "Gain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.78f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tone", "Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.62f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("decay", "Decay",
        juce::NormalisableRange<float>(0.15f, 4.0f, 0.001f), 1.6f));
    return { params.begin(), params.end() };
}

void StratumGuitarAudioProcessor::prepareToPlay(double sampleRate, int)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (auto& voice : voices_)
        voice = {};
}

void StratumGuitarAudioProcessor::releaseResources()
{
}

bool StratumGuitarAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

double StratumGuitarAudioProcessor::midiToHz(int midiNote)
{
    return 440.0 * std::pow(2.0, ((double)midiNote - 69.0) / 12.0);
}

int StratumGuitarAudioProcessor::noteNameToMidi(const juce::String& name)
{
    auto s = name.trim().toUpperCase();
    if (s.isEmpty())
        return -1;

    if (s.containsOnly("0123456789"))
    {
        const int note = s.getIntValue();
        return (note >= 0 && note <= 127) ? note : -1;
    }

    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int noteName = 0; noteName < 12; ++noteName)
    {
        juce::String prefix(names[noteName]);
        if (!s.startsWith(prefix))
            continue;

        auto tail = s.substring(prefix.length());
        juce::String octaveText;
        for (int i = 0; i < tail.length(); ++i)
        {
            const auto c = tail[i];
            if ((c == '-' && octaveText.isEmpty()) || (c >= '0' && c <= '9'))
                octaveText << juce::String::charToString(c);
            else
                break;
        }
        if (octaveText.isEmpty())
            continue;

        const int octave = octaveText.getIntValue();
        const int midi = (octave + 1) * 12 + noteName;
        return (midi >= 0 && midi <= 127) ? midi : -1;
    }

    return -1;
}

juce::File StratumGuitarAudioProcessor::findSampleFolder() const
{
    juce::Array<juce::File> candidates;
    const auto moduleFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    candidates.add(moduleFile.getParentDirectory().getChildFile("Samples").getChildFile("Guitar"));
    candidates.add(moduleFile.getParentDirectory().getParentDirectory().getChildFile("Samples").getChildFile("Guitar"));
    candidates.add(moduleFile.getParentDirectory().getParentDirectory().getParentDirectory().getChildFile("Samples").getChildFile("Guitar"));
    candidates.add(juce::File("F:/PlaygroundTest/flstudioclonee/vst-plugins/Samples/Guitar"));

    for (const auto& candidate : candidates)
        if (candidate.isDirectory())
            return candidate;

    return candidates.getLast();
}

void StratumGuitarAudioProcessor::loadSamples()
{
    samples_.clear();
    sampleFolder_ = findSampleFolder();
    if (!sampleFolder_.isDirectory())
    {
        loadedSampleCount_ = 0;
        return;
    }

    juce::Array<juce::File> files;
    sampleFolder_.findChildFiles(files, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.mp3;*.flac");

    for (const auto& file : files)
    {
        const int root = noteNameToMidi(file.getFileNameWithoutExtension());
        if (root < 0)
            continue;

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(file));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
            continue;

        GuitarSample sample;
        sample.rootNote = root;
        sample.sampleRate = reader->sampleRate;
        sample.buffer = std::make_shared<juce::AudioBuffer<float>>((int)reader->numChannels, (int)reader->lengthInSamples);
        reader->read(sample.buffer.get(), 0, (int)reader->lengthInSamples, 0, true, true);
        samples_.push_back(std::move(sample));
    }

    std::sort(samples_.begin(), samples_.end(), [](const auto& a, const auto& b) { return a.rootNote < b.rootNote; });
    loadedSampleCount_ = (int)samples_.size();
}

int StratumGuitarAudioProcessor::findClosestSampleIndex(int midiNote) const
{
    if (samples_.empty())
        return -1;

    int best = 0;
    int bestDistance = std::abs(samples_[0].rootNote - midiNote);
    for (int i = 1; i < (int)samples_.size(); ++i)
    {
        const int distance = std::abs(samples_[(size_t)i].rootNote - midiNote);
        if (distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

void StratumGuitarAudioProcessor::startNote(int midiNote, float velocity)
{
    auto* target = &voices_.front();
    for (auto& voice : voices_)
    {
        if (!voice.active)
        {
            target = &voice;
            break;
        }
    }

    *target = {};
    target->active = true;
    target->note = juce::jlimit(0, 127, midiNote);
    target->frequency = midiToHz(target->note);
    target->velocity = juce::jlimit(0.0f, 1.0f, velocity);
    target->sampleIndex = findClosestSampleIndex(target->note);
    target->usingSample = target->sampleIndex >= 0;
    if (target->usingSample)
    {
        const auto& sample = samples_[(size_t)target->sampleIndex];
        const double pitchRatio = std::pow(2.0, ((double)target->note - (double)sample.rootNote) / 12.0);
        target->sampleStep = pitchRatio * sample.sampleRate / juce::jmax(1.0, sampleRate_);
    }
}

void StratumGuitarAudioProcessor::stopNote(int midiNote)
{
    for (auto& voice : voices_)
    {
        if (voice.active && voice.note == midiNote && !voice.releasing)
        {
            voice.releasing = true;
            voice.releaseAge = 0.0;
            voice.releaseLevel = juce::jlimit(0.0f, 1.0f, voice.velocity * std::exp((float)(-voice.age * 1.8)));
        }
    }
}

void StratumGuitarAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            startNote(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            stopNote(msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            for (auto& voice : voices_)
                voice.releasing = true;
    }

    const auto gain = parameters.getRawParameterValue("gain")->load();
    const auto tone = parameters.getRawParameterValue("tone")->load();
    const auto decay = parameters.getRawParameterValue("decay")->load();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float out = 0.0f;

        for (auto& voice : voices_)
        {
            if (!voice.active)
                continue;

            float voiceOut = voice.usingSample ? renderSampleVoice(voice)
                                               : renderSynthVoice(voice, tone, decay);

            if (voice.releasing)
            {
                const float rel = std::exp((float)(-voice.releaseAge / 0.16));
                voiceOut *= rel;
                voice.releaseAge += 1.0 / sampleRate_;
                if (std::abs(voiceOut) < 0.0002f && voice.releaseAge > 0.28)
                {
                    voice.active = false;
                    continue;
                }
            }

            out += voiceOut;
        }

        out = std::tanh(out * gain * 0.65f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, sample, out);
    }
}

float StratumGuitarAudioProcessor::renderSampleVoice(Voice& voice)
{
    if (voice.sampleIndex < 0 || voice.sampleIndex >= (int)samples_.size())
        return 0.0f;

    const auto& sample = samples_[(size_t)voice.sampleIndex];
    if (!sample.buffer || sample.buffer->getNumSamples() <= 1)
        return 0.0f;

    const int total = sample.buffer->getNumSamples();
    const int idx = (int)voice.samplePosition;
    if (idx >= total - 1)
    {
        voice.active = false;
        return 0.0f;
    }

    const int idx1 = juce::jmin(idx + 1, total - 1);
    const float frac = (float)(voice.samplePosition - (double)idx);
    float value = 0.0f;
    const int channels = sample.buffer->getNumChannels();
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* data = sample.buffer->getReadPointer(ch);
        value += data[idx] + (data[idx1] - data[idx]) * frac;
    }
    value /= (float)juce::jmax(1, channels);

    const float attack = voice.age < 0.004 ? (float)(voice.age / 0.004) : 1.0f;
    voice.samplePosition += voice.sampleStep;
    voice.age += 1.0 / sampleRate_;
    return value * attack * voice.velocity;
}

float StratumGuitarAudioProcessor::renderSynthVoice(Voice& voice, float tone, float decay)
{
    const double twoPi = juce::MathConstants<double>::twoPi;
    const double dt = 1.0 / sampleRate_;
    const float attack = voice.age < 0.0025 ? (float)(voice.age / 0.0025) : 1.0f;
    const float pluck = std::exp((float)(-voice.age / 0.035));
    const float bodyEnv = std::exp((float)(-voice.age / juce::jmax(0.04f, decay)));
    const float env = attack * bodyEnv * voice.velocity;

    voice.phase += twoPi * voice.frequency / sampleRate_;
    voice.phase2 += twoPi * voice.frequency * 2.003 / sampleRate_;
    voice.phase3 += twoPi * voice.frequency * 3.01 / sampleRate_;
    if (voice.phase > twoPi) voice.phase -= twoPi;
    if (voice.phase2 > twoPi) voice.phase2 -= twoPi;
    if (voice.phase3 > twoPi) voice.phase3 -= twoPi;

    const float bright = 0.14f + tone * 0.36f;
    const float stringBuzz = std::sin((float)(voice.phase * 0.5 + voice.phase2 * 0.015)) > 0.0f ? 1.0f : -1.0f;
    const float value = std::sin((float)voice.phase) * 0.58f
                      + std::sin((float)voice.phase2) * bright * 0.62f
                      + std::sin((float)voice.phase3) * bright * 0.35f
                      + stringBuzz * bright * 0.16f * pluck;
    voice.age += dt;
    return value * env;
}

juce::AudioProcessorEditor* StratumGuitarAudioProcessor::createEditor()
{
    return new StratumGuitarAudioProcessorEditor(*this);
}

void StratumGuitarAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void StratumGuitarAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StratumGuitarAudioProcessor();
}
