#include "PluginHost.h"
#include <algorithm>

PluginHost::PluginHost()
{
    masterReverb_ = std::make_unique<ReverbEffect>();
    
    // Initialize synth DSP nodes
    synthOscillator_ = std::make_unique<juce::dsp::Oscillator<float>>();
    synthGain_ = std::make_unique<juce::dsp::Gain<float>>();
    synthEnvelope_ = std::make_unique<juce::dsp::Gain<float>>();
    
#if JUCE_PLUGINHOST_VST3
    formatManager_.addFormat(std::make_unique<juce::VST3PluginFormat>());
#endif

    // Audio format manager for sample preview (wav, mp3, flac, ogg, aiff)
    sampleFormatManager_.registerBasicFormats();
}

PluginHost::~PluginHost()
{
    juce::ScopedLock sl(slotsLock_);
    slots_.clear();
}

void PluginHost::scanDefaultLocations()
{
    juce::StringArray candidates;

   #if JUCE_WINDOWS
    auto pf  = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory);
    auto cfd = juce::File("C:/Program Files/Common Files/VST3");
    auto vp2 = juce::File("C:/Program Files/VstPlugins");
    auto vp2x86 = juce::File("C:/Program Files/Steinberg/VSTPlugins");
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto userVst3 = appData.getChildFile("VST3");

    candidates.add(cfd.getFullPathName());
    candidates.add(pf.getChildFile("VST3").getFullPathName());
    candidates.add(vp2.getFullPathName());
    candidates.add(vp2x86.getFullPathName());
    candidates.add(userVst3.getFullPathName());
   #endif
   #if JUCE_MAC
    auto sysVst3  = juce::File("/Library/Audio/Plug-Ins/VST3");
    auto userVst3 = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                        .getChildFile("Library/Audio/Plug-Ins/VST3");
    candidates.add(sysVst3.getFullPathName());
    candidates.add(userVst3.getFullPathName());
   #endif

    for (const auto& p : candidates)
    {
        juce::File dir(p);
        if (dir.isDirectory()) scanDirectory(p);
    }
}

juce::var PluginHost::scanDirectory(const juce::String& path)
{
    juce::OwnedArray<juce::PluginDescription> results;
    juce::File dir(path);
    juce::FileSearchPath searchPath;
    searchPath.add(dir);

    juce::KnownPluginList tempList;
    for (auto* fmt : formatManager_.getFormats())
    {
        juce::PluginDirectoryScanner scanner(tempList, *fmt, searchPath, true, juce::File(), false);
        juce::String pluginName;
        while (scanner.scanNextFile(true, pluginName)) {}
    }

    juce::Array<juce::var> arr;
    for (const auto& desc : tempList.getTypes())
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("name",          desc.name);
        obj->setProperty("pluginFormatName", desc.pluginFormatName);
        obj->setProperty("fileOrIdentifier", desc.fileOrIdentifier);
        obj->setProperty("manufacturer", desc.manufacturerName);
        obj->setProperty("version",      desc.version);
        obj->setProperty("isInstrument", desc.isInstrument);
        obj->setProperty("numInputChannels",  desc.numInputChannels);
        obj->setProperty("numOutputChannels", desc.numOutputChannels);
        arr.add(juce::var(obj.get()));
        pluginList_.addType(desc);
    }
    return juce::var(arr);
}

int PluginHost::loadPlugin(const juce::String& fileOrIdentifier, juce::String& errorOut)
{
    // Find description in known list or create one from file
    juce::PluginDescription desc;
    bool found = false;
    for (const auto& d : pluginList_.getTypes())
    {
        if (d.fileOrIdentifier == fileOrIdentifier || d.name == fileOrIdentifier)
        {
            desc = d;
            found = true;
            break;
        }
    }
    if (!found)
    {
        // Try loading directly from file path (VST3 folder or .dll)
        juce::OwnedArray<juce::PluginDescription> descs;
        for (auto* fmt : formatManager_.getFormats())
        {
            fmt->findAllTypesForFile(descs, fileOrIdentifier);
            if (!descs.isEmpty()) { desc = *descs[0]; found = true; break; }
        }
    }
    if (!found)
    {
        errorOut = "Plugin not found: " + fileOrIdentifier;
        return -1;
    }

    auto instance = formatManager_.createPluginInstance(desc, sampleRate_, blockSize_, errorOut);
    if (!instance)
        return -1;

    instance->prepareToPlay(sampleRate_, blockSize_);
    instance->setPlayConfigDetails(0, 2, sampleRate_, blockSize_);

    auto slot = std::make_unique<PluginSlot>();
    int id = nextSlotId_++;
    slot->id = id;
    slot->instance = std::move(instance);
    slot->buffer.setSize(2, blockSize_);

    juce::ScopedLock sl(slotsLock_);
    slots_[id] = std::move(slot);
    return id;
}

void PluginHost::unloadPlugin(int slotId)
{
    juce::ScopedLock sl(slotsLock_);
    slots_.erase(slotId);
}

void PluginHost::sendMidiNote(int slotId, int channel, int note, int velocity, bool on)
{
    juce::ScopedLock sl(slotsLock_);
    auto it = slots_.find(slotId);
    if (it == slots_.end()) return;
    juce::ScopedLock sl2(it->second->lock);
    auto msg = on
        ? juce::MidiMessage::noteOn(channel, note, (juce::uint8)velocity)
        : juce::MidiMessage::noteOff(channel, note);
    it->second->pendingMidi.addEvent(msg, 0);
}

void PluginHost::sendMidiCC(int slotId, int channel, int cc, int value)
{
    juce::ScopedLock sl(slotsLock_);
    auto it = slots_.find(slotId);
    if (it == slots_.end()) return;
    juce::ScopedLock sl2(it->second->lock);
    it->second->pendingMidi.addEvent(
        juce::MidiMessage::controllerEvent(channel, cc, value), 0);
}

void PluginHost::setParameter(int slotId, int paramIndex, float value)
{
    juce::ScopedLock sl(slotsLock_);
    auto it = slots_.find(slotId);
    if (it == slots_.end()) return;
    auto& params = it->second->instance->getParameters();
    if (paramIndex < params.size())
        params[paramIndex]->setValueNotifyingHost(value);
}

juce::var PluginHost::getParameters(int slotId)
{
    juce::ScopedLock sl(slotsLock_);
    auto it = slots_.find(slotId);
    if (it == slots_.end()) return {};
    juce::Array<juce::var> arr;
    for (auto* p : it->second->instance->getParameters())
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("index", p->getParameterIndex());
        obj->setProperty("name",  p->getName(64));
        obj->setProperty("value", p->getValue());
        obj->setProperty("text",  p->getText(p->getValue(), 32));
        obj->setProperty("defaultValue", p->getDefaultValue());
        arr.add(juce::var(obj.get()));
    }
    return juce::var(arr);
}

juce::var PluginHost::getLoadedPlugins() const
{
    juce::ScopedLock sl(slotsLock_);
    juce::Array<juce::var> arr;
    for (const auto& [id, slot] : slots_)
    {
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("slotId", id);
        obj->setProperty("name",   slot->instance->getName());
        obj->setProperty("format", slot->instance->getPluginDescription().pluginFormatName);
        arr.add(juce::var(obj.get()));
    }
    return juce::var(arr);
}

void PluginHost::showEditor(int slotId, bool show)
{
    juce::MessageManager::callAsync([this, slotId, show]
    {
        juce::ScopedLock sl(slotsLock_);
        auto it = slots_.find(slotId);
        if (it == slots_.end()) return;
        auto& slot = *it->second;

        if (show)
        {
            if (slot.instance->hasEditor())
            {
                slot.editor.reset(slot.instance->createEditor());
                auto* w = new juce::DocumentWindow(
                    slot.instance->getName(), juce::Colours::black,
                    juce::DocumentWindow::allButtons);
                w->setUsingNativeTitleBar(true);
                w->setContentNonOwned(slot.editor.get(), true);
                w->setResizable(true, false);
                w->setVisible(true);
                slot.editorWindow.reset(w);
            }
        }
        else
        {
            slot.editorWindow.reset();
            slot.editor.reset();
        }
    });
}

void PluginHost::audioDeviceIOCallbackWithContext(
    const float* const* /*in*/, int /*numIn*/,
    float* const* out, int numOut,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& /*ctx*/)
{
    // Clear output first
    for (int ch = 0; ch < numOut; ++ch)
        juce::FloatVectorOperations::clear(out[ch], numSamples);

    juce::AudioBuffer<float> buffer(out, numOut, numSamples);
    juce::dsp::AudioBlock<float> block(buffer);
    
    // Update time
    currentTime_ += numSamples / sampleRate_;
    
    // Process synth voices
    {
        juce::ScopedLock sl(synthLock_);
        
        synthVoices_.erase(std::remove_if(synthVoices_.begin(), synthVoices_.end(),
            [this](const auto& voice) {
                return !voice.active || (currentTime_ - voice.startTime) >= voice.duration;
            }), synthVoices_.end());
        
        if (!synthVoices_.empty())
        {
            for (auto& voice : synthVoices_)
            {
                if (!voice.active) continue;
                
                double age = currentTime_ - voice.startTime;
                if (age >= voice.duration) { voice.active = false; continue; }
                
                float env = 1.0f;
                if (age < 0.01) env = age / 0.01f;
                else if (age > voice.duration - 0.05) env = (voice.duration - age) / 0.05f;
                if (env < 0) env = 0;
                
                float freq = voice.frequency;
                float vel = voice.velocity;
                
                for (int i = 0; i < numSamples; ++i)
                {
                    double t = (currentTime_ - numSamples / sampleRate_) + i / sampleRate_;
                    float sample = std::sin(freq * t * 6.283185) * env * vel * 0.3f;
                    for (int ch = 0; ch < std::min(numOut, 2); ++ch)
                        out[ch][i] += sample;
                }
            }
        }
    }
    
    // Mix sample voices (polyphonic, one-shot, linear-interpolated resampling)
    {
        juce::ScopedTryLock tl(sampleLock_);
        if (tl.isLocked())
        {
            for (auto& v : sampleVoices_)
            {
                if (!v.active || !v.buffer) continue;

                const int total = v.buffer->getNumSamples();
                if (v.position >= (double)total) { v.active = false; continue; }

                const int srcCh = v.buffer->getNumChannels();
                if (srcCh <= 0) { v.active = false; continue; }

                for (int i = 0; i < numSamples; ++i)
                {
                    const int    idx  = (int)v.position;
                    if (idx >= total) { v.active = false; break; }
                    const int    idx1 = juce::jmin(idx + 1, total - 1);
                    const float  frac = (float)(v.position - (double)idx);

                    for (int ch = 0; ch < numOut; ++ch)
                    {
                        const int sch = juce::jmin(ch, srcCh - 1);
                        if (sch < 0) break;
                        const auto* src = v.buffer->getReadPointer(sch);
                        const float a = src[idx];
                        const float b = src[idx1];
                        out[ch][i] += (a + (b - a) * frac) * 0.7f;
                    }

                    v.position += v.step;
                }
            }

            sampleVoices_.erase(
                std::remove_if(sampleVoices_.begin(), sampleVoices_.end(),
                               [](const SampleVoice& v){ return !v.active; }),
                sampleVoices_.end());
        }
    }

    // ── Master FX chain ──────────────────────────────────────────
    // Each loaded plugin processes the current master bus IN PLACE,
    // turning every loaded plugin into a series effect on the mix.
    // (Full per-track routing comes later; this at least makes
    //  effects like EQs, compressors, delays etc. actually audible.)
    {
        juce::ScopedLock sl(slotsLock_);
        for (auto& [id, slot] : slots_)
        {
            juce::ScopedLock sl2(slot->lock);
            slot->buffer.setSize(2, numSamples, false, false, true);

            // Copy current master output INTO the plugin's working buffer
            for (int ch = 0; ch < 2; ++ch)
            {
                const int srcCh = juce::jmin(ch, numOut - 1);
                if (srcCh < 0) { slot->buffer.clear(ch, 0, numSamples); continue; }
                slot->buffer.copyFrom(ch, 0, out[srcCh], numSamples);
            }

            slot->instance->processBlock(slot->buffer, slot->pendingMidi);
            slot->pendingMidi.clear();

            // Replace master output with the plugin's processed signal
            for (int ch = 0; ch < std::min(numOut, 2); ++ch)
                juce::FloatVectorOperations::copy(
                    out[ch], slot->buffer.getReadPointer(ch), numSamples);
        }
    }

    // Apply master reverb LAST (after FX chain, before going out)
    if (reverbEnabled_ && masterReverb_)
        masterReverb_->process(buffer);
}

void PluginHost::playSampleFile(const juce::File& file)
{
    if (!file.existsAsFile()) return;
    juce::String key = file.getFullPathName();
    std::shared_ptr<juce::AudioBuffer<float>> buf;
    double srcSampleRate = sampleRate_;

    // Look up in cache first
    {
        juce::ScopedLock sl(sampleLock_);
        auto it = sampleCache_.find(key);
        if (it != sampleCache_.end())
        {
            buf           = it->second.buffer;
            srcSampleRate = it->second.sampleRate;
        }
    }

    // Not cached — decode now
    if (!buf)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(
            sampleFormatManager_.createReaderFor(file));
        if (!reader) return;

        srcSampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : sampleRate_;
        buf = std::make_shared<juce::AudioBuffer<float>>(
            (int)reader->numChannels, (int)reader->lengthInSamples);
        reader->read(buf.get(), 0, (int)reader->lengthInSamples, 0, true, true);

        juce::ScopedLock sl(sampleLock_);
        sampleCache_[key] = { buf, srcSampleRate };
    }

    // Spawn a new voice — step ratio resamples on the fly so the sample
    // plays at its intended pitch regardless of the device sample rate.
    SampleVoice v;
    v.buffer   = buf;
    v.position = 0.0;
    v.step     = (sampleRate_ > 0.0) ? (srcSampleRate / sampleRate_) : 1.0;
    v.active   = true;

    juce::ScopedLock sl(sampleLock_);
    sampleVoices_.push_back(std::move(v));

    constexpr int kMaxVoices = 32;
    if ((int)sampleVoices_.size() > kMaxVoices)
        sampleVoices_.erase(sampleVoices_.begin(),
                            sampleVoices_.begin() + (sampleVoices_.size() - kMaxVoices));
}

void PluginHost::stopSamplePlayback()
{
    juce::ScopedLock sl(sampleLock_);
    sampleVoices_.clear();
}

void PluginHost::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate_ = device->getCurrentSampleRate();
    blockSize_  = device->getCurrentBufferSizeSamples();
    juce::ScopedLock sl(slotsLock_);
    for (auto& [id, slot] : slots_)
        slot->instance->prepareToPlay(sampleRate_, blockSize_);
    
    // Prepare reverb effect
    if (masterReverb_)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate_;
        spec.maximumBlockSize = blockSize_;
        spec.numChannels = 2;
        masterReverb_->prepare(spec);
    }
    
    // Prepare synth DSP nodes
    juce::dsp::ProcessSpec synthSpec;
    synthSpec.sampleRate = sampleRate_;
    synthSpec.maximumBlockSize = blockSize_;
    synthSpec.numChannels = 2;
    synthOscillator_->prepare(synthSpec);
    synthGain_->prepare(synthSpec);
    synthEnvelope_->prepare(synthSpec);
    synthOscillator_->initialise([](float x) { return std::sin(x); });
    synthOscillator_->reset();
    synthGain_->setGainLinear(0.8f);
    synthEnvelope_->setGainLinear(1.0f);
    
    // Reverb stays OFF by default. Users opt-in via per-track reverb sends
    // (Mixer) or the master reverb controls.
    reverbEnabled_ = false;
    if (masterReverb_)
    {
        masterReverb_->setRoomSize(0.6f);
        masterReverb_->setDamping(0.5f);
        masterReverb_->setWetLevel(0.0f);  // dry mix on startup
        masterReverb_->setDryLevel(1.0f);
        masterReverb_->setWidth(1.0f);
    }
}

void PluginHost::audioDeviceStopped()
{
    juce::ScopedLock sl(slotsLock_);
    for (auto& [id, slot] : slots_)
        slot->instance->releaseResources();
    
    if (masterReverb_)
        masterReverb_->reset();
}

void PluginHost::setMasterReverbRoomSize(float size)
{
    if (masterReverb_)
        masterReverb_->setRoomSize(size);
}

void PluginHost::setMasterReverbDamping(float damping)
{
    if (masterReverb_)
        masterReverb_->setDamping(damping);
}

void PluginHost::setMasterReverbWetLevel(float wet)
{
    if (masterReverb_)
        masterReverb_->setWetLevel(wet);
}

void PluginHost::setMasterReverbDryLevel(float dry)
{
    if (masterReverb_)
        masterReverb_->setDryLevel(dry);
}

void PluginHost::setMasterReverbWidth(float width)
{
    if (masterReverb_)
        masterReverb_->setWidth(width);
}

void PluginHost::setMasterReverbFreezeMode(bool freeze)
{
    if (masterReverb_)
        masterReverb_->setFreezeMode(freeze);
}

juce::var PluginHost::getMasterReverbParams() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    if (masterReverb_)
    {
        obj->setProperty("roomSize", masterReverb_->getRoomSize());
        obj->setProperty("damping", masterReverb_->getDamping());
        obj->setProperty("wetLevel", masterReverb_->getWetLevel());
        obj->setProperty("dryLevel", masterReverb_->getDryLevel());
        obj->setProperty("width", masterReverb_->getWidth());
        obj->setProperty("freezeMode", masterReverb_->getFreezeMode());
    }
    return juce::var(obj.get());
}

void PluginHost::playSynthKick(double time)
{
    juce::ScopedLock sl(synthLock_);
    synthVoices_.push_back({ time, 0.2, 60.0f, 1.0f, true });
}

void PluginHost::playSynthSnare(double time)
{
    juce::ScopedLock sl(synthLock_);
    synthVoices_.push_back({ time, 0.15, 200.0f, 0.8f, true });
}

void PluginHost::playSynthHihat(double time, bool open)
{
    juce::ScopedLock sl(synthLock_);
    synthVoices_.push_back({ time, open ? 0.3 : 0.05, 800.0f, 0.4f, true });
}

void PluginHost::playSynthClap(double time)
{
    juce::ScopedLock sl(synthLock_);
    synthVoices_.push_back({ time, 0.1, 1000.0f, 0.6f, true });
}

void PluginHost::playSynthTone(double frequency, double time, double duration, float velocity)
{
    juce::ScopedLock sl(synthLock_);
    synthVoices_.push_back({ time, duration, static_cast<float>(frequency), velocity, true });
}

void PluginHost::setSynthReverbWetLevel(float wetLevel)
{
    if (masterReverb_)
        masterReverb_->setWetLevel(wetLevel);
}

void PluginHost::setSynthReverbEnabled(bool enabled)
{
    reverbEnabled_ = enabled;
}
