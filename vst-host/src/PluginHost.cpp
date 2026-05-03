#include "PluginHost.h"

PluginHost::PluginHost()
{
#if JUCE_PLUGINHOST_VST3
    formatManager_.addFormat(std::make_unique<juce::VST3PluginFormat>());
#endif
}

PluginHost::~PluginHost()
{
    juce::ScopedLock sl(slotsLock_);
    slots_.clear();
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

    juce::ScopedLock sl(slotsLock_);
    for (auto& [id, slot] : slots_)
    {
        juce::ScopedLock sl2(slot->lock);
        slot->buffer.setSize(2, numSamples, false, false, true);
        slot->buffer.clear();

        slot->instance->processBlock(slot->buffer, slot->pendingMidi);
        slot->pendingMidi.clear();

        // Mix into output
        for (int ch = 0; ch < std::min(numOut, 2); ++ch)
            juce::FloatVectorOperations::add(out[ch], slot->buffer.getReadPointer(ch), numSamples);
    }
}

void PluginHost::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate_ = device->getCurrentSampleRate();
    blockSize_  = device->getCurrentBufferSizeSamples();
    juce::ScopedLock sl(slotsLock_);
    for (auto& [id, slot] : slots_)
        slot->instance->prepareToPlay(sampleRate_, blockSize_);
}

void PluginHost::audioDeviceStopped()
{
    juce::ScopedLock sl(slotsLock_);
    for (auto& [id, slot] : slots_)
        slot->instance->releaseResources();
}
