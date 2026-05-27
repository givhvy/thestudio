#include "PluginHost.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

PluginHost::PluginHost()
{
    for (auto& level : trackLevels_)
        level.store(0.0f, std::memory_order_relaxed);

    masterReverb_ = std::make_unique<ReverbEffect>();
    
    // Initialize synth DSP nodes
    synthOscillator_ = std::make_unique<juce::dsp::Oscillator<float>>();
    synthGain_ = std::make_unique<juce::dsp::Gain<float>>();
    synthEnvelope_ = std::make_unique<juce::dsp::Gain<float>>();
    
#if JUCE_PLUGINHOST_VST3
    formatManager_.addFormat(std::make_unique<juce::VST3PluginFormat>());
#endif
#if JUCE_PLUGINHOST_VST
    formatManager_.addFormat(std::make_unique<juce::VSTPluginFormat>());
#endif

    // Audio format manager for sample preview (wav, mp3, flac, ogg, aiff)
    sampleFormatManager_.registerBasicFormats();

    loadPluginCache();
}

PluginHost::~PluginHost()
{
    juce::ScopedLock sl(slotsLock_);
    slots_.clear();
}

juce::File PluginHost::getPluginCacheFile() const
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appData.getChildFile("Stratum DAW").getChildFile("known-plugins.xml");
}

void PluginHost::loadPluginCache()
{
    auto cacheFile = getPluginCacheFile();
    if (!cacheFile.existsAsFile())
        return;

    if (auto xml = juce::parseXML(cacheFile))
        pluginList_.recreateFromXml(*xml);
}

void PluginHost::savePluginCache() const
{
    auto cacheFile = getPluginCacheFile();
    cacheFile.getParentDirectory().createDirectory();

    if (auto xml = pluginList_.createXml())
        xml->writeTo(cacheFile);
}

void PluginHost::NativeEffectSlot::Biquad::reset()
{
    z1 = {};
    z2 = {};
}

void PluginHost::NativeEffectSlot::Biquad::setIdentity()
{
    b0 = 1.0;
    b1 = b2 = a1 = a2 = 0.0;
    reset();
}

void PluginHost::NativeEffectSlot::Biquad::setCoefficients(double nb0, double nb1, double nb2,
                                                           double na0, double na1, double na2)
{
    const double invA0 = na0 != 0.0 ? 1.0 / na0 : 1.0;
    b0 = nb0 * invA0;
    b1 = nb1 * invA0;
    b2 = nb2 * invA0;
    a1 = na1 * invA0;
    a2 = na2 * invA0;
    reset();
}

void PluginHost::NativeEffectSlot::Biquad::setHighPass(double sr, double freq, double q)
{
    const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit(10.0, sr * 0.45, freq) / sr;
    const double cosW = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    setCoefficients((1.0 + cosW) * 0.5, -(1.0 + cosW), (1.0 + cosW) * 0.5,
                    1.0 + alpha, -2.0 * cosW, 1.0 - alpha);
}

void PluginHost::NativeEffectSlot::Biquad::setLowPass(double sr, double freq, double q)
{
    const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit(10.0, sr * 0.45, freq) / sr;
    const double cosW = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    setCoefficients((1.0 - cosW) * 0.5, 1.0 - cosW, (1.0 - cosW) * 0.5,
                    1.0 + alpha, -2.0 * cosW, 1.0 - alpha);
}

void PluginHost::NativeEffectSlot::Biquad::setPeak(double sr, double freq, double q, double gainDb)
{
    const double a = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit(10.0, sr * 0.45, freq) / sr;
    const double cosW = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    setCoefficients(1.0 + alpha * a, -2.0 * cosW, 1.0 - alpha * a,
                    1.0 + alpha / a, -2.0 * cosW, 1.0 - alpha / a);
}

void PluginHost::NativeEffectSlot::Biquad::setLowShelf(double sr, double freq, double gainDb)
{
    const double a = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit(10.0, sr * 0.45, freq) / sr;
    const double cosW = std::cos(w0);
    const double beta = std::sqrt(a) / std::sqrt(2.0);
    setCoefficients(a * ((a + 1.0) - (a - 1.0) * cosW + 2.0 * beta * std::sin(w0)),
                    2.0 * a * ((a - 1.0) - (a + 1.0) * cosW),
                    a * ((a + 1.0) - (a - 1.0) * cosW - 2.0 * beta * std::sin(w0)),
                    (a + 1.0) + (a - 1.0) * cosW + 2.0 * beta * std::sin(w0),
                    -2.0 * ((a - 1.0) + (a + 1.0) * cosW),
                    (a + 1.0) + (a - 1.0) * cosW - 2.0 * beta * std::sin(w0));
}

void PluginHost::NativeEffectSlot::Biquad::setHighShelf(double sr, double freq, double gainDb)
{
    const double a = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit(10.0, sr * 0.45, freq) / sr;
    const double cosW = std::cos(w0);
    const double beta = std::sqrt(a) / std::sqrt(2.0);
    setCoefficients(a * ((a + 1.0) + (a - 1.0) * cosW + 2.0 * beta * std::sin(w0)),
                    -2.0 * a * ((a - 1.0) + (a + 1.0) * cosW),
                    a * ((a + 1.0) + (a - 1.0) * cosW - 2.0 * beta * std::sin(w0)),
                    (a + 1.0) - (a - 1.0) * cosW + 2.0 * beta * std::sin(w0),
                    2.0 * ((a - 1.0) - (a + 1.0) * cosW),
                    (a + 1.0) - (a - 1.0) * cosW - 2.0 * beta * std::sin(w0));
}

void PluginHost::NativeEffectSlot::Biquad::process(juce::AudioBuffer<float>& buffer)
{
    const int channels = juce::jmin(2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        double s1 = z1[(size_t)ch];
        double s2 = z2[(size_t)ch];
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const double x = data[i];
            const double y = b0 * x + s1;
            s1 = b1 * x - a1 * y + s2;
            s2 = b2 * x - a2 * y;
            data[i] = (float)juce::jlimit(-8.0, 8.0, y);
        }
        z1[(size_t)ch] = s1;
        z2[(size_t)ch] = s2;
    }
}

void PluginHost::NativeEffectSlot::prepare(double sr, int maxBlockSize)
{
    sampleRate = sr > 1.0 ? sr : 44100.0;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)juce::jmax(1, maxBlockSize);
    spec.numChannels = 2;

    if (type == Type::Reverb)
    {
        reverb.prepare(spec);
        reverbParams.roomSize = 0.62f;
        reverbParams.damping = 0.46f;
        reverbParams.wetLevel = 0.28f;
        reverbParams.dryLevel = 0.86f;
        reverbParams.width = 1.0f;
        reverbParams.freezeMode = 0.0f;
        reverb.setParameters(reverbParams);
    }
    else if (type == Type::Delay)
    {
        const int maxDelaySamples = (int)(sampleRate * 3.0);
        delayBuffer.setSize(2, juce::jmax(maxDelaySamples, maxBlockSize + 1), false, false, true);
        delayBuffer.clear();
        delayWrite = 0;
    }
    else if (type == Type::ParametricEq)
        updateEqCoefficients();

    prepared = true;
}

void PluginHost::NativeEffectSlot::updateEqCoefficients()
{
    if (sampleRate <= 1.0)
        sampleRate = 44100.0;

    eqBands[0].setHighPass(sampleRate, eqFreq[0], eqQ[0]);
    eqBands[1].setLowShelf(sampleRate, eqFreq[1], eqGain[1]);
    eqBands[2].setPeak(sampleRate, eqFreq[2], eqQ[2], eqGain[2]);
    eqBands[3].setPeak(sampleRate, eqFreq[3], eqQ[3], eqGain[3]);
    eqBands[4].setPeak(sampleRate, eqFreq[4], eqQ[4], eqGain[4]);
    eqBands[5].setHighShelf(sampleRate, eqFreq[5], eqGain[5]);
    eqBands[6].setLowPass(sampleRate, eqFreq[6], eqQ[6]);
}

void PluginHost::NativeEffectSlot::reset()
{
    if (type == Type::Reverb)
        reverb.reset();
    else if (type == Type::Delay)
        delayBuffer.clear();
    else
        for (auto& band : eqBands)
            band.reset();
    delayWrite = 0;
}

void PluginHost::NativeEffectSlot::process(juce::AudioBuffer<float>& buffer)
{
    if (!prepared)
        return;

    if (type == Type::Reverb)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        reverb.process(context);
        return;
    }

    if (type == Type::ParametricEq)
    {
        for (auto& band : eqBands)
            band.process(buffer);
        return;
    }

    if (type == Type::SoftClipper)
    {
        const int channels = juce::jmin(2, buffer.getNumChannels());
        const float threshold = juce::jlimit(0.1f, 0.98f, clipThreshold);
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float x = data[i] * clipPreGain;
                const float sign = x < 0.0f ? -1.0f : 1.0f;
                const float ax = std::abs(x);
                if (ax > threshold)
                {
                    const float over = (ax - threshold) / juce::jmax(0.001f, 1.0f - threshold);
                    x = sign * (threshold + (1.0f - threshold) * std::tanh(over));
                }
                data[i] = juce::jlimit(-0.98f, 0.98f, x * clipPostGain);
            }
        }
        return;
    }

    const int channels = juce::jmin(2, buffer.getNumChannels());
    const int delaySamples = juce::jlimit(1, juce::jmax(1, delayBuffer.getNumSamples() - 1),
                                         (int)std::round(sampleRate * delayMs / 1000.0));
    const int delaySize = delayBuffer.getNumSamples();

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const int readPos = (delayWrite - delaySamples + delaySize) % delaySize;
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* dst = buffer.getWritePointer(ch);
            auto* del = delayBuffer.getWritePointer(ch);
            const float input = dst[i];
            const float delayed = del[readPos];
            dst[i] = input * dry + delayed * wet;
            del[delayWrite] = juce::jlimit(-1.0f, 1.0f, input + delayed * feedback);
        }
        delayWrite = (delayWrite + 1) % delaySize;
    }
}

void PluginHost::scanDefaultLocations()
{
    juce::StringArray candidates;

   #if JUCE_WINDOWS
    auto pf  = juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory);
    auto cfd = juce::File("C:/Program Files/Common Files/VST3");
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto userVst3 = appData.getChildFile("VST3");
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto repoRoot = exeDir.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory();
    auto localStratumPlugins = repoRoot.getChildFile("vst-plugins").getChildFile("_installed").getChildFile("VST3");

    candidates.add(cfd.getFullPathName());
    candidates.add(pf.getChildFile("VST3").getFullPathName());
    candidates.add(userVst3.getFullPathName());
    candidates.add(localStratumPlugins.getFullPathName());

    auto appDir = appData.getChildFile("Stratum DAW");
    auto pathsFile = appDir.getChildFile("plugin-paths.txt");
    if (pathsFile.existsAsFile())
    {
        juce::StringArray persistedPaths;
        pathsFile.readLines(persistedPaths);
        for (const auto& path : persistedPaths)
        {
           #if JUCE_PLUGINHOST_VST
            candidates.add(path);
           #else
            const auto lower = path.toLowerCase();
            if (!lower.contains("vst2") && !lower.contains("vstplugins") && !lower.contains("ujam")
                && !lower.endsWith(".dll"))
                candidates.add(path);
           #endif
        }
    }

   #if JUCE_PLUGINHOST_VST
    candidates.add("C:/Program Files/Common Files/VST2");
    candidates.add("C:/Program Files/Steinberg/VstPlugins");
    candidates.add("D:/Vst");
    candidates.add("D:/Vst/Ujam");
    candidates.add("D:/VST");
    candidates.add("D:/VST/Ujam");
    candidates.add("D:/Vst/Ozone 11 Advanced");
    candidates.add("D:/FL studio/Plugins/VST");
   #endif
   #endif
   #if JUCE_MAC
    auto sysVst3  = juce::File("/Library/Audio/Plug-Ins/VST3");
    auto userVst3 = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                        .getChildFile("Library/Audio/Plug-Ins/VST3");
    candidates.add(sysVst3.getFullPathName());
    candidates.add(userVst3.getFullPathName());

   #endif

    candidates.removeDuplicates(false);
    for (const auto& p : candidates)
    {
        juce::File dir(p);
        if (dir.isDirectory()) scanDirectory(p);
    }

    savePluginCache();
}

void PluginHost::addPluginScanPath(const juce::String& folderPath)
{
    juce::File dir(folderPath);
    if (!dir.isDirectory()) return;

    // Persist to %AppData%/Stratum DAW/plugin-paths.txt
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto appDir = appData.getChildFile("Stratum DAW");
    if (!appDir.exists()) appDir.createDirectory();
    auto pathsFile = appDir.getChildFile("plugin-paths.txt");

    juce::StringArray lines;
    if (pathsFile.existsAsFile())
        pathsFile.readLines(lines);

    // Avoid duplicates
    if (!lines.contains(folderPath))
    {
        lines.add(folderPath);
        pathsFile.replaceWithText(lines.joinIntoString("\n"));
    }

    // Scan immediately
    scanDirectory(folderPath);
    savePluginCache();
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
    savePluginCache();
    return juce::var(arr);
}

int PluginHost::loadPlugin(const juce::String& fileOrIdentifier, juce::String& errorOut)
{
#if ! JUCE_PLUGINHOST_VST
    const juce::File requestedFile(fileOrIdentifier);
    if (requestedFile.hasFileExtension(".dll"))
    {
        errorOut = "VST2 .dll plugins are not enabled in this build. Use a .vst3 plugin, or add the VST2 SDK headers and rebuild with VST2 support.";
        return -1;
    }
#endif

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

int PluginHost::createNativeEffect(const juce::String& type)
{
    auto fx = std::make_unique<NativeEffectSlot>();
    fx->id = nextNativeEffectId_--;
    if (type.equalsIgnoreCase("delay"))
    {
        fx->type = NativeEffectSlot::Type::Delay;
        fx->name = "Stratum Delay";
    }
    else if (type.equalsIgnoreCase("eq") || type.equalsIgnoreCase("parametric-eq"))
    {
        fx->type = NativeEffectSlot::Type::ParametricEq;
        fx->name = "Stratum Parametric EQ";
    }
    else if (type.equalsIgnoreCase("soft-clipper") || type.equalsIgnoreCase("clipper"))
    {
        fx->type = NativeEffectSlot::Type::SoftClipper;
        fx->name = "Stratum Soft Clipper";
    }
    else
    {
        fx->type = NativeEffectSlot::Type::Reverb;
        fx->name = "Stratum Reverb";
    }
    fx->prepare(sampleRate_, blockSize_);

    const int id = fx->id;
    juce::ScopedLock sl(nativeEffectsLock_);
    nativeEffects_[id] = std::move(fx);
    return id;
}

void PluginHost::unloadNativeEffect(int effectId)
{
    juce::ScopedLock sl(nativeEffectsLock_);
    nativeEffects_.erase(effectId);
}

juce::String PluginHost::getNativeEffectName(int effectId) const
{
    juce::ScopedLock sl(nativeEffectsLock_);
    auto it = nativeEffects_.find(effectId);
    return it != nativeEffects_.end() ? it->second->name : juce::String();
}

juce::String PluginHost::getNativeEffectType(int effectId) const
{
    juce::ScopedLock sl(nativeEffectsLock_);
    auto it = nativeEffects_.find(effectId);
    if (it == nativeEffects_.end()) return {};
    switch (it->second->type)
    {
        case NativeEffectSlot::Type::ParametricEq: return "parametric-eq";
        case NativeEffectSlot::Type::SoftClipper: return "soft-clipper";
        case NativeEffectSlot::Type::Delay: return "delay";
        case NativeEffectSlot::Type::Reverb:
        default: return "reverb";
    }
}

float PluginHost::getNativeEffectParam(int effectId, const juce::String& param) const
{
    juce::ScopedLock sl(nativeEffectsLock_);
    auto it = nativeEffects_.find(effectId);
    if (it == nativeEffects_.end()) return 0.0f;
    const auto& fx = *it->second;

    for (int i = 0; i < 7; ++i)
    {
        const auto idx = juce::String(i);
        if (param == "eq" + idx + "Freq") return fx.eqFreq[(size_t)i];
        if (param == "eq" + idx + "Gain") return fx.eqGain[(size_t)i];
        if (param == "eq" + idx + "Q")    return fx.eqQ[(size_t)i];
    }

    if (param == "clipThreshold") return fx.clipThreshold;
    if (param == "clipPreGain") return fx.clipPreGain;
    if (param == "clipPostGain") return fx.clipPostGain;
    return 0.0f;
}

void PluginHost::setNativeEffectParam(int effectId, const juce::String& param, float value)
{
    juce::ScopedLock sl(nativeEffectsLock_);
    auto it = nativeEffects_.find(effectId);
    if (it == nativeEffects_.end()) return;
    auto& fx = *it->second;

    bool eqChanged = false;
    for (int i = 0; i < 7; ++i)
    {
        const auto idx = juce::String(i);
        if (param == "eq" + idx + "Freq")
        {
            fx.eqFreq[(size_t)i] = juce::jlimit(20.0f, 20000.0f, value);
            eqChanged = true;
        }
        else if (param == "eq" + idx + "Gain")
        {
            fx.eqGain[(size_t)i] = juce::jlimit(-18.0f, 18.0f, value);
            eqChanged = true;
        }
        else if (param == "eq" + idx + "Q")
        {
            fx.eqQ[(size_t)i] = juce::jlimit(0.15f, 12.0f, value);
            eqChanged = true;
        }
    }

    if (param == "clipThreshold") fx.clipThreshold = juce::jlimit(0.05f, 0.98f, value);
    if (param == "clipPreGain")   fx.clipPreGain = juce::jlimit(0.25f, 8.0f, value);
    if (param == "clipPostGain")  fx.clipPostGain = juce::jlimit(0.0f, 2.0f, value);

    if (eqChanged)
        fx.updateEqCoefficients();
}

void PluginHost::showNativeEffectEditor(int effectId)
{
    juce::MessageManager::callAsync([this, effectId]
    {
        juce::String name;
        {
            juce::ScopedLock sl(nativeEffectsLock_);
            auto it = nativeEffects_.find(effectId);
            if (it == nativeEffects_.end()) return;
            name = it->second->name;
        }

        if (onNativeEffectEditorRequested)
            onNativeEffectEditorRequested(effectId, name);
    });
}

void PluginHost::unloadPlugin(int slotId)
{
    {
        juce::ScopedLock routingSl(routingLock_);
        bypassedFxSlots_.erase(slotId);
    }

    if (slotId < 0)
    {
        unloadNativeEffect(slotId);
        return;
    }

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
            if (! slot.instance->hasEditor()) return;
            if (slot.editor) return; // already shown

            slot.editor.reset(slot.instance->createEditor());
            if (slot.editor == nullptr) return;

            // Preferred path: hand the editor to the UI layer for embedding.
            if (onEditorReady)
            {
                onEditorReady(slotId, slot.editor.get(), slot.instance->getName());
                return;
            }

            // Legacy fallback: open a native top-level window.
            auto* w = new juce::DocumentWindow(
                slot.instance->getName(), juce::Colours::black,
                juce::DocumentWindow::allButtons);
            w->setUsingNativeTitleBar(true);
            w->setContentNonOwned(slot.editor.get(), true);
            w->setResizable(true, false);
            w->setVisible(true);
            slot.editorWindow.reset(w);
        }
        else
        {
            // Tell UI layer to drop its embed wrapper FIRST so it un-parents
            // the editor before we delete it.
            if (onEditorClosed) onEditorClosed(slotId);
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
    juce::ScopedLock renderSl(renderLock_);
    renderAudioBlock(out, numOut, numSamples);
}

void PluginHost::renderAudioBlock(float* const* out, int numOut, int numSamples)
{
    // Clear output first
    for (int ch = 0; ch < numOut; ++ch)
        juce::FloatVectorOperations::clear(out[ch], numSamples);

    juce::AudioBuffer<float> buffer(out, numOut, numSamples);
    juce::dsp::AudioBlock<float> block(buffer);
    
    // Update time
    currentTime_ += numSamples / sampleRate_;
    
    // ── Per-track temp buffers ─────────────────────────────────
    std::unordered_map<int, juce::AudioBuffer<float>> trackBuffers;
    juce::AudioBuffer<float> masterBuf(2, numSamples);
    masterBuf.clear();

    // Helper to get the destination buffer for a given track index.
    auto getDstBuf = [&](int trackIdx, int ns) -> juce::AudioBuffer<float>*
    {
        if (trackIdx < 0 || trackIdx == masterTrackIdx_)
            return &masterBuf;
        auto& b = trackBuffers[trackIdx];
        if (b.getNumSamples() != ns)
        { b.setSize(2, ns, false, false, true); b.clear(); }
        return &b;
    };

    // Process synth voices — route into the appropriate track buffer.
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

                juce::AudioBuffer<float>* dst = getDstBuf(voice.trackIdx, numSamples);

                for (int i = 0; i < numSamples; ++i)
                {
                    double t = (currentTime_ - numSamples / sampleRate_) + i / sampleRate_;
                    float sample = 0.0f;
                    if (voice.bass)
                    {
                        const double localT = juce::jmax(0.0, t - voice.startTime);
                        const double noteEnd = juce::jmax(0.01, voice.duration);
                        const float attack = (float)juce::jlimit(0.0, 1.0, localT / 0.012);
                        const float sustain = (float)std::exp(-0.45 * localT / juce::jmax(0.18, noteEnd));
                        const float release = localT > noteEnd - 0.12
                            ? (float)juce::jlimit(0.0, 1.0, (noteEnd - localT) / 0.12)
                            : 1.0f;
                        const float bassEnv = attack * sustain * release * env;
                        const double phase = freq * t * 6.28318530718;
                        const float sub = std::sin((float)phase);
                        const float body = std::sin((float)(phase * 2.0)) * 0.28f;
                        const float growl = std::tanh(sub * 1.35f + body * 0.9f) * 0.35f;
                        sample = (sub * 0.78f + body + growl) * bassEnv * vel * 0.44f;
                    }
                    else if (voice.piano)
                    {
                        const double localT = juce::jmax(0.0, t - voice.startTime);
                        const float decay = std::exp((float)(-3.8 * localT / juce::jmax(0.08, voice.duration)));
                        const float body =
                            std::sin((float)(freq * t * 6.28318530718)) * 0.72f
                          + std::sin((float)(freq * 2.01 * t * 6.28318530718)) * 0.22f
                          + std::sin((float)(freq * 3.02 * t * 6.28318530718)) * 0.10f;
                        const float hammer = std::sin((float)(freq * 7.0 * t * 6.28318530718))
                                           * std::exp((float)(-60.0 * localT)) * 0.16f;
                        sample = (body * decay + hammer) * env * vel * 0.34f;
                    }
                    else
                    {
                        sample = std::sin(freq * t * 6.283185) * env * vel * 0.3f;
                    }
                    for (int ch = 0; ch < std::min(dst->getNumChannels(), 2); ++ch)
                        dst->getWritePointer(ch)[i] += sample;
                }
            }
        }
    }

    // ── Per-track sample voice rendering ─────────────────────────
    {
        juce::ScopedTryLock tl(sampleLock_);
        if (tl.isLocked())
        {
            for (auto& v : sampleVoices_)
            {
                if (!v.active || !v.buffer) continue;

                const int total = v.buffer->getNumSamples();
                const double endPos = juce::jlimit(0.0, (double)total,
                                                   v.endPosition > 0.0 ? v.endPosition : (double)total);
                if (v.position >= endPos) { v.active = false; continue; }

                const int srcCh = v.buffer->getNumChannels();
                if (srcCh <= 0) { v.active = false; continue; }

                juce::AudioBuffer<float>* dst = getDstBuf(v.trackIdx, numSamples);

                for (int i = 0; i < numSamples; ++i)
                {
                    if (v.outputSamplesRemaining == 0) { v.active = false; break; }

                    const int    idx  = (int)v.position;
                    if (v.position >= endPos || idx >= total) { v.active = false; break; }
                    const int    idx1 = juce::jmin(idx + 1, (int)endPos - 1, total - 1);
                    const float  frac = (float)(v.position - (double)idx);
                    float env = 1.0f;

                    if (v.attackSamples > 1 && v.ageSamples < v.attackSamples)
                        env *= (float)v.ageSamples / (float)v.attackSamples;

                    if (v.releaseRemaining > 0)
                    {
                        env *= (float)v.releaseRemaining / (float)juce::jmax(1, v.releaseSamples);
                        --v.releaseRemaining;
                        if (v.releaseRemaining <= 0)
                        {
                            v.active = false;
                            break;
                        }
                    }
                    else
                    {
                        const double samplesLeft = (endPos - v.position) / juce::jmax(0.000001, v.step);
                        if (samplesLeft < (double)v.releaseSamples)
                            env *= (float)(samplesLeft / (double)juce::jmax(1, v.releaseSamples));
                    }

                    for (int ch = 0; ch < dst->getNumChannels(); ++ch)
                    {
                        const int sch = juce::jmin(ch, srcCh - 1);
                        if (sch < 0) break;
                        const auto* src = v.buffer->getReadPointer(sch);
                        const float a = src[idx];
                        const float b = src[idx1];
                        dst->getWritePointer(ch)[i] += (a + (b - a) * frac) * env * 0.7f * v.gain;
                    }

                    v.position += v.step;
                    ++v.ageSamples;

                    if (v.outputSamplesRemaining > 0 && --v.outputSamplesRemaining == 0)
                    {
                        v.active = false;
                        break;
                    }
                }
            }

            sampleVoices_.erase(
                std::remove_if(sampleVoices_.begin(), sampleVoices_.end(),
                               [](const SampleVoice& v){ return !v.active; }),
                sampleVoices_.end());
        }
    }

    // Clear output — we'll write the final mix back at the end.
    for (int ch = 0; ch < numOut; ++ch)
        juce::FloatVectorOperations::clear(out[ch], numSamples);

    // ── Per-track plugin chains ──────────────────────────────────
    std::unordered_set<int> bypassedFxSlots;
    {
        juce::ScopedLock sl(routingLock_);
        bypassedFxSlots = bypassedFxSlots_;
    }

    auto runChain = [this, numSamples, &bypassedFxSlots](juce::AudioBuffer<float>& buf,
                                                         const std::vector<int>& chain)
    {
        for (int slotId : chain)
        {
            if (bypassedFxSlots.count(slotId) != 0)
                continue;

            if (slotId < 0)
            {
                juce::ScopedLock nativeSl(nativeEffectsLock_);
                auto it = nativeEffects_.find(slotId);
                if (it != nativeEffects_.end())
                    it->second->process(buf);
                continue;
            }

            juce::ScopedLock sl(slotsLock_);
            auto it = slots_.find(slotId);
            if (it == slots_.end()) continue;
            auto& slot = it->second;
            juce::ScopedLock sl2(slot->lock);
            slot->buffer.setSize(2, numSamples, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                slot->buffer.copyFrom(ch, 0, buf, juce::jmin(ch, buf.getNumChannels() - 1), 0, numSamples);
            slot->instance->processBlock(slot->buffer, slot->pendingMidi);
            slot->pendingMidi.clear();
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                buf.copyFrom(ch, 0, slot->buffer, juce::jmin(ch, 1), 0, numSamples);
        }
    };

    std::unordered_set<int> routedSlots;
    {
        juce::ScopedLock sl(routingLock_);
        for (const auto& [trackIdx, chain] : trackChains_)
            for (int slotId : chain)
                routedSlots.insert(slotId);
    }

    // Channel-rack instrument slots are not mixer FX. They still need to be
    // processed every block so MIDI-driven VST instruments render into audio.
    // Route their output to the track buffer assigned via setSlotTrack().
    {
        juce::ScopedLock sl(slotsLock_);
        juce::ScopedLock stl(slotTrackLock_);
        for (auto& [slotId, slot] : slots_)
        {
            if (routedSlots.count(slotId) != 0)
                continue;

            juce::ScopedLock sl2(slot->lock);
            slot->buffer.setSize(2, numSamples, false, false, true);
            slot->buffer.clear();
            slot->instance->processBlock(slot->buffer, slot->pendingMidi);
            slot->pendingMidi.clear();

            int slotTrack = -1;
            auto sit = slotTrackMap_.find(slotId);
            if (sit != slotTrackMap_.end()) slotTrack = sit->second;

            juce::AudioBuffer<float>* dst = getDstBuf(slotTrack, numSamples);
            for (int ch = 0; ch < 2; ++ch)
                dst->addFrom(ch, 0, slot->buffer, juce::jmin(ch, slot->buffer.getNumChannels() - 1), 0, numSamples);
        }
    }

    std::vector<TrackControl> controls;
    {
        juce::ScopedLock ctl(trackControlLock_);
        controls = trackControls_;
    }

    bool anySolo = false;
    for (int i = 0; i < (int)controls.size(); ++i)
        if (i != masterTrackIdx_ && controls[(size_t)i].solo)
            anySolo = true;

    for (auto& level : trackLevels_)
        level.store(level.load(std::memory_order_relaxed) * 0.82f, std::memory_order_relaxed);

    auto publishLevel = [this](int trackIdx, const juce::AudioBuffer<float>& buf)
    {
        if (trackIdx < 0 || trackIdx >= maxMeterTracks_)
            return;

        float peak = 0.0f;
        const int channels = juce::jmin(2, buf.getNumChannels());
        for (int ch = 0; ch < channels; ++ch)
            peak = juce::jmax(peak, buf.getMagnitude(ch, 0, buf.getNumSamples()));

        peak = juce::jlimit(0.0f, 1.0f, peak * 1.7f);
        const float old = trackLevels_[(size_t)trackIdx].load(std::memory_order_relaxed);
        trackLevels_[(size_t)trackIdx].store(juce::jmax(peak, old * 0.72f), std::memory_order_relaxed);
    };

    auto applyTrackControl = [&](int trackIdx, juce::AudioBuffer<float>& buf)
    {
        TrackControl ctl;
        if (trackIdx >= 0 && trackIdx < (int)controls.size())
            ctl = controls[(size_t)trackIdx];

        const bool mutedBySolo = anySolo && trackIdx != masterTrackIdx_ && !ctl.solo;
        if (ctl.muted || mutedBySolo)
        {
            buf.clear();
            publishLevel(trackIdx, buf);
            return;
        }

        const float volume = juce::jlimit(0.0f, 1.0f, ctl.volume);
        const float pan = juce::jlimit(-1.0f, 1.0f, ctl.pan);
        const float leftGain = volume * (pan <= 0.0f ? 1.0f : 1.0f - pan);
        const float rightGain = volume * (pan >= 0.0f ? 1.0f : 1.0f + pan);

        if (buf.getNumChannels() > 0)
            buf.applyGain(0, 0, buf.getNumSamples(), leftGain);
        if (buf.getNumChannels() > 1)
            buf.applyGain(1, 0, buf.getNumSamples(), rightGain);

        publishLevel(trackIdx, buf);
    };

    std::vector<int> masterChain;
    {
        juce::ScopedLock sl(routingLock_);
        for (auto& [trackIdx, chain] : trackChains_)
        {
            if (trackIdx == masterTrackIdx_) { masterChain = chain; continue; }
            auto it = trackBuffers.find(trackIdx);
            if (it == trackBuffers.end()) continue; // no audio on this track this block
            runChain(it->second, chain);
        }
    }

    // Sum every per-track buffer into the master bus.
    for (auto& [idx, b] : trackBuffers)
    {
        applyTrackControl(idx, b);
        for (int ch = 0; ch < 2; ++ch)
            masterBuf.addFrom(ch, 0, b, ch, 0, numSamples);
    }

    // Master chain runs on the summed bus.
    if (!masterChain.empty()) runChain(masterBuf, masterChain);
    applyTrackControl(masterTrackIdx_, masterBuf);

    // Write final mix back into the device output.
    for (int ch = 0; ch < std::min(numOut, 2); ++ch)
        juce::FloatVectorOperations::copy(out[ch], masterBuf.getReadPointer(ch), numSamples);

    // Apply master reverb LAST (after FX chain, before going out)
    if (reverbEnabled_ && masterReverb_)
        masterReverb_->process(buffer);

    // ── Headphone Flat EQ (monitoring-only, post-master) ────────
    if (headphoneFlatEnabled_.load(std::memory_order_relaxed) && hpCorrectionPrepared_)
    {
        buffer.applyGain(hpCorrectionPreamp_);
        for (auto& band : hpCorrectionBands_)
            band.process(buffer);
    }
}

void PluginHost::playSampleFile(const juce::File& file, int trackIdx, double startOffsetSeconds, float gain,
                                double playbackRate, double maxTimelineSeconds)
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
    v.buffer      = buf;
    v.sourcePath  = key;
    const int totalSamples = buf->getNumSamples();
    const double rate = juce::jlimit(0.25, 4.0, playbackRate);
    const double startSample = juce::jmax(0.0, startOffsetSeconds) * srcSampleRate;
    v.position = juce::jlimit(0.0, (double)juce::jmax(0, totalSamples - 1), startSample);
    if (maxTimelineSeconds > 0.0)
    {
        v.outputSamplesRemaining = juce::jmax(1, (int)std::ceil(maxTimelineSeconds * sampleRate_));
        const double maxSourceSeconds = maxTimelineSeconds * rate;
        v.endPosition = juce::jmin((double)totalSamples,
                                   v.position + maxSourceSeconds * srcSampleRate);
    }
    else
    {
        v.outputSamplesRemaining = -1;
        v.endPosition = (double)totalSamples;
    }
    v.step     = (sampleRate_ > 0.0) ? (srcSampleRate / sampleRate_) : 1.0;
    v.step    *= rate;
    v.active   = true;
    v.trackIdx = trackIdx;
    v.gain     = juce::jlimit(0.0f, 2.0f, gain);
    v.attackSamples = juce::jmax(1, (int)(sampleRate_ * 0.003));
    v.releaseSamples = juce::jmax(1, (int)(sampleRate_ * 0.012));

    juce::ScopedLock sl(sampleLock_);
    sampleVoices_.push_back(std::move(v));

    constexpr int kMaxVoices = 32;
    if ((int)sampleVoices_.size() > kMaxVoices)
    {
        const int voicesToRelease = (int)sampleVoices_.size() - kMaxVoices;
        for (int i = 0; i < voicesToRelease; ++i)
            if (sampleVoices_[(size_t)i].releaseRemaining <= 0)
                sampleVoices_[(size_t)i].releaseRemaining = sampleVoices_[(size_t)i].releaseSamples;
    }
}

void PluginHost::playSamplePreview(const juce::File& file)
{
    {
        juce::ScopedLock sl(sampleLock_);
        for (auto& v : sampleVoices_)
            if (v.releaseRemaining <= 0)
                v.releaseRemaining = v.releaseSamples;
    }

    playSampleFile(file);
}

void PluginHost::sendAllNotesOff(int slotId, int channel)
{
    juce::ScopedLock sl(slotsLock_);
    auto it = slots_.find(slotId);
    if (it == slots_.end())
        return;

    juce::ScopedLock sl2(it->second->lock);
    const int ch = juce::jlimit(1, 16, channel);
    it->second->pendingMidi.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
    it->second->pendingMidi.addEvent(juce::MidiMessage::controllerEvent(ch, 123, 0), 0);
}

void PluginHost::flushAllPluginNotesOff()
{
    juce::ScopedLock sl(slotsLock_);
    for (auto& [id, slot] : slots_)
    {
        juce::ignoreUnused(id);
        juce::ScopedLock sl2(slot->lock);
        slot->pendingMidi.clear();
        for (int ch = 1; ch <= 16; ++ch)
        {
            slot->pendingMidi.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
            slot->pendingMidi.addEvent(juce::MidiMessage::controllerEvent(ch, 123, 0), 0);
        }
    }
}

void PluginHost::stopSampleVoicesOnTrack(const juce::File& file, int trackIdx, bool immediate)
{
    if (!file.existsAsFile())
        return;

    const juce::String key = file.getFullPathName();
    juce::ScopedLock sl(sampleLock_);
    for (auto& v : sampleVoices_)
    {
        if (v.sourcePath != key)
            continue;
        if (trackIdx >= 0 && v.trackIdx != trackIdx)
            continue;

        if (immediate)
        {
            v.active = false;
            v.outputSamplesRemaining = 0;
        }
        else if (v.releaseRemaining <= 0)
        {
            v.releaseRemaining = v.releaseSamples;
        }
    }
}

void PluginHost::stopSamplePlaybackImmediate()
{
    juce::ScopedLock sl(sampleLock_);
    sampleVoices_.clear();
}

void PluginHost::stopSamplePlayback()
{
    juce::ScopedLock sl(sampleLock_);
    for (auto& v : sampleVoices_)
        if (v.releaseRemaining <= 0)
            v.releaseRemaining = v.releaseSamples;
}

void PluginHost::stopSampleFileVoices(const juce::File& file, bool immediate)
{
    if (!file.existsAsFile())
        return;

    const juce::String key = file.getFullPathName();
    juce::ScopedLock sl(sampleLock_);
    for (auto& v : sampleVoices_)
    {
        if (v.sourcePath != key)
            continue;

        if (immediate)
        {
            v.active = false;
            v.outputSamplesRemaining = 0;
        }
        else if (v.releaseRemaining <= 0)
        {
            v.releaseRemaining = v.releaseSamples;
        }
    }
}

void PluginHost::clearTransientPlayback()
{
    juce::ScopedLock renderSl(renderLock_);
    {
        juce::ScopedLock sl(sampleLock_);
        sampleVoices_.clear();
    }
    {
        juce::ScopedLock sl(synthLock_);
        synthVoices_.clear();
        currentTime_ = 0.0;
    }
    flushAllPluginNotesOff();

    if (masterReverb_)
        masterReverb_->reset();

    for (auto& level : trackLevels_)
        level.store(0.0f, std::memory_order_relaxed);
}

void PluginHost::setTrackChain(int trackIdx, std::vector<int> slotIds)
{
    juce::ScopedLock sl(routingLock_);
    if (slotIds.empty())
        trackChains_.erase(trackIdx);
    else
        trackChains_[trackIdx] = std::move(slotIds);
}

void PluginHost::setFxSlotBypassed(int slotId, bool bypassed)
{
    juce::ScopedLock sl(routingLock_);
    if (bypassed)
        bypassedFxSlots_.insert(slotId);
    else
        bypassedFxSlots_.erase(slotId);
}

void PluginHost::setMasterTrackIdx(int idx)
{
    juce::ScopedLock sl(routingLock_);
    masterTrackIdx_ = idx;
}

void PluginHost::setSlotTrack(int slotId, int trackIdx)
{
    juce::ScopedLock sl(slotTrackLock_);
    slotTrackMap_[slotId] = trackIdx;
}

void PluginHost::clearSlotTrack(int slotId)
{
    juce::ScopedLock sl(slotTrackLock_);
    slotTrackMap_.erase(slotId);
}

void PluginHost::setTrackControls(std::vector<TrackControl> controls)
{
    juce::ScopedLock sl(trackControlLock_);
    trackControls_ = std::move(controls);
}

float PluginHost::getTrackLevel(int trackIdx) const
{
    if (trackIdx < 0 || trackIdx >= maxMeterTracks_)
        return 0.0f;

    return trackLevels_[(size_t)trackIdx].load(std::memory_order_relaxed);
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

    {
        juce::ScopedLock nativeSl(nativeEffectsLock_);
        for (auto& [id, fx] : nativeEffects_)
            fx->prepare(sampleRate_, blockSize_);
    }

    // Prepare headphone correction EQ coefficients for current sample rate
    prepareHeadphoneCorrection(sampleRate_);
}

void PluginHost::audioDeviceStopped()
{
    juce::ScopedLock sl(slotsLock_);
    for (auto& [id, slot] : slots_)
        slot->instance->releaseResources();
    
    if (masterReverb_)
        masterReverb_->reset();

    {
        juce::ScopedLock nativeSl(nativeEffectsLock_);
        for (auto& [id, fx] : nativeEffects_)
            fx->reset();
    }
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

// ── Headphone Flat EQ ─────────────────────────────────────────────
void PluginHost::setHeadphoneFlatEnabled(bool enabled)
{
    headphoneFlatEnabled_.store(enabled, std::memory_order_relaxed);
}

bool PluginHost::isHeadphoneFlatEnabled() const
{
    return headphoneFlatEnabled_.load(std::memory_order_relaxed);
}

void PluginHost::prepareHeadphoneCorrection(double sr)
{
    // Sony WH-1000XM5 → Harman target correction (AutoEQ / oratory1990).
    // Preamp: -6.2 dB to prevent clipping from the boosts.
    hpCorrectionPreamp_ = (float)std::pow(10.0, -6.2 / 20.0);  // ≈ 0.49

    // Band 0: Low Shelf  105 Hz  -3.2 dB  Q 0.70
    hpCorrectionBands_[0].setLowShelf(sr, 105.0, -3.2);
    // Band 1: Peak  2448 Hz  +6.9 dB  Q 2.46
    hpCorrectionBands_[1].setPeak(sr, 2448.0, 2.46, 6.9);
    // Band 2: Peak  173 Hz  -5.6 dB  Q 0.96
    hpCorrectionBands_[2].setPeak(sr, 173.0, 0.96, -5.6);
    // Band 3: Peak  3028 Hz  -5.4 dB  Q 2.03
    hpCorrectionBands_[3].setPeak(sr, 3028.0, 2.03, -5.4);
    // Band 4: Peak  5765 Hz  +4.5 dB  Q 2.50
    hpCorrectionBands_[4].setPeak(sr, 5765.0, 2.50, 4.5);
    // Band 5: Peak  1049 Hz  +1.6 dB  Q 1.20
    hpCorrectionBands_[5].setPeak(sr, 1049.0, 1.20, 1.6);
    // Band 6: Peak  8255 Hz  -3.8 dB  Q 3.50
    hpCorrectionBands_[6].setPeak(sr, 8255.0, 3.50, -3.8);
    // Band 7: High Shelf  10000 Hz  +3.0 dB
    hpCorrectionBands_[7].setHighShelf(sr, 10000.0, 3.0);

    hpCorrectionPrepared_ = true;
}

void PluginHost::playSynthKick(double time, int trackIdx)
{
    juce::ScopedLock sl(synthLock_);
    const double startTime = (time > currentTime_ - 1.0 && time < currentTime_ + 5.0) ? time : currentTime_;
    synthVoices_.push_back({ startTime, 0.2, 60.0f, 1.0f, true, false, trackIdx });
}

void PluginHost::playSynthSnare(double time, int trackIdx)
{
    juce::ScopedLock sl(synthLock_);
    const double startTime = (time > currentTime_ - 1.0 && time < currentTime_ + 5.0) ? time : currentTime_;
    synthVoices_.push_back({ startTime, 0.15, 200.0f, 0.8f, true, false, trackIdx });
}

void PluginHost::playSynthHihat(double time, bool open, int trackIdx)
{
    juce::ScopedLock sl(synthLock_);
    const double startTime = (time > currentTime_ - 1.0 && time < currentTime_ + 5.0) ? time : currentTime_;
    synthVoices_.push_back({ startTime, open ? 0.3 : 0.05, 800.0f, 0.4f, true, false, trackIdx });
}

void PluginHost::playSynthClap(double time, int trackIdx)
{
    juce::ScopedLock sl(synthLock_);
    const double startTime = (time > currentTime_ - 1.0 && time < currentTime_ + 5.0) ? time : currentTime_;
    synthVoices_.push_back({ startTime, 0.1, 1000.0f, 0.6f, true, false, trackIdx });
}

void PluginHost::playSynthTone(double frequency, double time, double duration, float velocity, int trackIdx)
{
    juce::ScopedLock sl(synthLock_);
    const double startTime = (time > currentTime_ - 1.0 && time < currentTime_ + 5.0) ? time : currentTime_;
    synthVoices_.push_back({ startTime, duration, static_cast<float>(frequency), velocity, true, false, trackIdx });
}

void PluginHost::playSynthPiano(int midiNote, double time, double duration, float velocity, int trackIdx)
{
    const int note = juce::jlimit(0, 127, midiNote);
    const double frequency = 440.0 * std::pow(2.0, ((double)note - 69.0) / 12.0);
    juce::ScopedLock sl(synthLock_);
    const double startTime = (time > currentTime_ - 1.0 && time < currentTime_ + 5.0) ? time : currentTime_;
    synthVoices_.push_back({ startTime, juce::jlimit(0.08, 6.0, duration),
                             static_cast<float>(frequency),
                             juce::jlimit(0.0f, 1.0f, velocity),
                             true, true, trackIdx });
}

void PluginHost::playSynthBass(int midiNote, double time, double duration, float velocity, int trackIdx)
{
    const int note = juce::jlimit(0, 127, midiNote);
    const double frequency = 440.0 * std::pow(2.0, ((double)note - 69.0) / 12.0);
    juce::ScopedLock sl(synthLock_);
    const double startTime = (time > currentTime_ - 1.0 && time < currentTime_ + 5.0) ? time : currentTime_;

    SynthVoice voice;
    voice.startTime = startTime;
    voice.duration = juce::jlimit(0.08, 8.0, duration);
    voice.frequency = static_cast<float>(frequency);
    voice.velocity = juce::jlimit(0.0f, 1.0f, velocity);
    voice.active = true;
    voice.piano = false;
    voice.trackIdx = trackIdx;
    voice.bass = true;
    synthVoices_.push_back(voice);
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
