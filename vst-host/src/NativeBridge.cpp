#include "NativeBridge.h"

static juce::var makeError (const juce::String& msg)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("error", msg);
    return juce::var (o.get());
}

static juce::var makeSuccess()
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("success", true);
    return juce::var (o.get());
}

static juce::var makeOk (const juce::var& data = juce::var())
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("data", data);
    return juce::var (o.get());
}

static juce::String varToJSON (const juce::var& v)
{
    return juce::JSON::toString (v).replace ("\\", "\\\\").replace ("'", "\\'");
}

NativeBridge::NativeBridge (PluginHost& ph, AudioEngine& ae)
    : pluginHost (ph), audioEngine (ae) {}

void NativeBridge::setBrowser (juce::WebBrowserComponent* b) { browser = b; }

void NativeBridge::setPendingCompletion (const juce::String& id,
                                          juce::WebBrowserComponent::NativeFunctionCompletion completion)
{
    juce::ScopedLock sl (pendingSyncLock_);
    pendingCompletions_[id.toStdString()] = std::move (completion);
}

void NativeBridge::sendCallback (const juce::String& id, const juce::var& result)
{
    // HTTP bridge mode: intercept pending sync callbacks
    {
        juce::ScopedLock sl (pendingSyncLock_);
        auto it = pendingSyncCallbacks_.find (id.toStdString());
        if (it != pendingSyncCallbacks_.end())
        {
            auto cb = std::move (it->second);
            pendingSyncCallbacks_.erase (it);
            cb (result);
            return;
        }
        // JUCE 8 NativeFunctionCompletion mode
        auto it2 = pendingCompletions_.find (id.toStdString());
        if (it2 != pendingCompletions_.end())
        {
            auto completion = std::move (it2->second);
            pendingCompletions_.erase (it2);
            completion (result);
            return;
        }
    }
    if (! browser) return;
    auto json = varToJSON (result);
    auto js = "window.__juceCallbacks['" + id + "'](JSON.parse('" + json + "'));";
    browser->evaluateJavascript (js);
}

void NativeBridge::callSync (const juce::String& channel, const juce::var& args,
                              std::function<void(const juce::var&)> resultCallback)
{
    static std::atomic<int> counter { 0 };
    auto id = "__sync__" + juce::String (++counter);
    {
        juce::ScopedLock sl (pendingSyncLock_);
        pendingSyncCallbacks_[id.toStdString()] = std::move (resultCallback);
    }
    juce::MessageManager::callAsync ([this, channel, args, id]() mutable {
        handleJSInvoke (channel, args, id);
    });
}

void NativeBridge::sendEventToJS (const juce::String& ch, const juce::var& d)
{
    if (! browser) return;
    auto js = "(window.__juceListeners['" + ch + "']||[]).forEach(cb=>cb('"
              + varToJSON (d) + "'));";
    browser->evaluateJavascript (js);
}

void NativeBridge::handleJSInvoke (const juce::String& channel,
                                   const juce::var& args,
                                   const juce::String& cb)
{
    if (channel == "dialog:openFile")      { handleDialogOpenFile (args, cb); return; }
    if (channel == "dialog:saveFile")        { handleDialogSaveFile (args, cb); return; }
    if (channel == "dialog:openDirectory")   { handleDialogOpenDirectory (args, cb); return; }
    if (channel == "fs:readFile")          { handleFsReadFile (args, cb); return; }
    if (channel == "fs:writeFile")           { handleFsWriteFile (args, cb); return; }
    if (channel == "fs:readBinaryFile")      { handleFsReadBinaryFile (args, cb); return; }
    if (channel == "fs:deleteFile")          { handleFsDeleteFile (args, cb); return; }
    if (channel == "fs:getProjectsDir")      { handleFsGetProjectsDir (args, cb); return; }
    if (channel == "fs:listProjects")        { handleFsListProjects (args, cb); return; }
    if (channel == "fs:listDirectory")       { handleFsListDirectory (args, cb); return; }
    if (channel == "log")                    { handleLog (args); sendCallback (cb, makeOk()); return; }
    if (channel == "app:minimize")           { handleAppMinimize (args); sendCallback (cb, makeOk()); return; }
    if (channel == "app:maximize")           { handleAppMaximize (args); sendCallback (cb, makeOk()); return; }
    if (channel == "app:close")              { handleAppClose (args); sendCallback (cb, makeOk()); return; }
    if (channel == "vst:connect")            { handleVstConnect (args, cb); return; }
    if (channel == "vst:call")               { handleVstCall (args, cb); return; }
    if (channel == "vst:scanFolder")          { handleVstScanFolder (args, cb); return; }
    sendCallback (cb, makeError ("Unknown channel: " + channel));
}

void NativeBridge::handleDialogOpenFile (const juce::var& args, const juce::String& cb)
{
    auto title = args.isArray() && args.getArray()->size() > 0
                     ? args.getArray()->getReference (0).toString()
                     : juce::String ("Open File");
    auto initial = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    juce::FileChooser chooser (title, initial, "*", true);
    if (chooser.browseForFileToOpen())
        sendCallback (cb, chooser.getURLResult().toString (true));
    else
        sendCallback (cb, juce::var());
}

void NativeBridge::handleDialogSaveFile (const juce::var& args, const juce::String& cb)
{
    auto title = args.isArray() && args.getArray()->size() > 0
                     ? args.getArray()->getReference (0).toString()
                     : juce::String ("Save File");
    auto initial = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    juce::FileChooser chooser (title, initial, "*", true);
    if (chooser.browseForFileToSave (true))
        sendCallback (cb, chooser.getURLResult().toString (true));
    else
        sendCallback (cb, juce::var());
}

void NativeBridge::handleDialogOpenDirectory (const juce::var& args, const juce::String& cb)
{
    auto title = args.isArray() && args.getArray()->size() > 0
                     ? args.getArray()->getReference (0).toString()
                     : juce::String ("Select Folder");
    auto initial = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    juce::FileChooser chooser (title, initial, "", true);
    if (chooser.browseForDirectory())
        sendCallback (cb, chooser.getURLResult().toString (true));
    else
        sendCallback (cb, juce::var());
}

void NativeBridge::handleFsReadFile (const juce::var& args, const juce::String& cb)
{
    if (! args.isArray() || args.getArray()->isEmpty())
    {
        sendCallback (cb, makeError ("Missing file path"));
        return;
    }
    auto path = args.getArray()->getReference (0).toString();
    juce::File file (path);
    if (! file.existsAsFile())
    {
        sendCallback (cb, makeError ("File not found: " + path));
        return;
    }
    auto content = file.loadFileAsString();
    auto parsed = juce::JSON::parse (content);
    if (parsed.isVoid())
        sendCallback (cb, juce::var (content));
    else
        sendCallback (cb, parsed);
}

void NativeBridge::handleFsWriteFile (const juce::var& args, const juce::String& cb)
{
    if (! args.isArray() || args.getArray()->size() < 2)
    {
        sendCallback (cb, makeError ("Missing path or content"));
        return;
    }
    auto path = args.getArray()->getReference (0).toString();
    auto content = args.getArray()->getReference (1).toString();
    juce::File file (path);
    if (file.replaceWithText (content))
        sendCallback (cb, makeSuccess());
    else
        sendCallback (cb, makeError ("Failed to write file"));
}

void NativeBridge::handleFsReadBinaryFile (const juce::var& args, const juce::String& cb)
{
    if (! args.isArray() || args.getArray()->isEmpty())
    {
        sendCallback (cb, makeError ("Missing file path"));
        return;
    }
    auto path = args.getArray()->getReference (0).toString();
    juce::File file (path);
    if (! file.existsAsFile())
    {
        sendCallback (cb, makeError ("File not found: " + path));
        return;
    }
    juce::FileInputStream stream (file);
    if (stream.openedOk())
    {
        juce::MemoryBlock block;
        stream.readIntoMemoryBlock (block);
        auto b64 = juce::Base64::toBase64 (block.getData(), (int) block.getSize());
        sendCallback (cb, makeOk (b64));
    }
    else
    {
        sendCallback (cb, makeError ("Cannot read file: " + path));
    }
}

void NativeBridge::handleFsDeleteFile (const juce::var& args, const juce::String& cb)
{
    if (! args.isArray() || args.getArray()->isEmpty())
    {
        sendCallback (cb, makeError ("Missing file path"));
        return;
    }
    auto path = args.getArray()->getReference (0).toString();
    juce::File file (path);
    if (file.deleteFile())
        sendCallback (cb, makeSuccess());
    else
        sendCallback (cb, makeError ("Failed to delete file"));
}

void NativeBridge::handleFsGetProjectsDir (const juce::var&, const juce::String& cb)
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("StratumProjects");
    dir.createDirectory();
    sendCallback (cb, dir.getFullPathName());
}

void NativeBridge::handleFsListProjects (const juce::var&, const juce::String& cb)
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("StratumProjects");
    dir.createDirectory();
    juce::var arr;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.stratum"))
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("name", f.getFileNameWithoutExtension());
        o->setProperty ("path", f.getFullPathName());
        o->setProperty ("modified", f.getLastModificationTime().toString (true, true));
        arr.append (juce::var (o.get()));
    }
    sendCallback (cb, arr);
}

void NativeBridge::handleFsListDirectory (const juce::var& args, const juce::String& cb)
{
    if (! args.isArray() || args.getArray()->isEmpty())
    {
        sendCallback (cb, makeError ("Missing directory path"));
        return;
    }
    auto path = args.getArray()->getReference (0).toString();
    juce::File dir (path);
    if (! dir.isDirectory())
    {
        sendCallback (cb, makeError ("Not a directory: " + path));
        return;
    }
    juce::var arr;
    for (auto& f : dir.findChildFiles (juce::File::findFilesAndDirectories, false))
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("name", f.getFileName());
        o->setProperty ("path", f.getFullPathName());
        o->setProperty ("isDirectory", f.isDirectory());
        o->setProperty ("isFile", !f.isDirectory());
        o->setProperty ("size", (double) f.getSize());
        arr.append (juce::var (o.get()));
    }
    sendCallback (cb, arr);
}

void NativeBridge::handleLog (const juce::var& args)
{
    if (args.isArray() && args.getArray()->size() > 0)
        juce::Logger::writeToLog ("[JS] " + args.getArray()->getReference (0).toString());
}

void NativeBridge::handleAppMinimize (const juce::var&)
{
    if (onMinimize) onMinimize();
}

void NativeBridge::handleAppMaximize (const juce::var&)
{
    if (onMaximize) onMaximize();
}

void NativeBridge::handleAppClose (const juce::var&)
{
    if (onClose) onClose();
}

void NativeBridge::handleVstConnect (const juce::var&, const juce::String& cb)
{
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty ("port", 9001);
    sendCallback (cb, juce::var (o.get()));
}

void NativeBridge::handleVstCall (const juce::var& args, const juce::String& cb)
{
    if (! args.isArray() || args.getArray()->size() < 2)
    {
        sendCallback (cb, makeError ("Missing method or args"));
        return;
    }
    auto method = args.getArray()->getReference (0).toString();
    auto methodArgs = args.getArray()->getReference (1);

    try
    {
        juce::var result;
        if      (method == "ping")              result = juce::var ("pong");
        else if (method == "scanPlugins")       result = pluginHost.scanDirectory (methodArgs["path"].toString());
        else if (method == "loadPlugin")
        {
            juce::String err;
            int id = pluginHost.loadPlugin (methodArgs["fileOrIdentifier"].toString(), err);
            juce::DynamicObject::Ptr o = new juce::DynamicObject();
            o->setProperty ("slotId", id);
            o->setProperty ("error", err);
            result = juce::var (o.get());
        }
        else if (method == "unloadPlugin")
        {
            pluginHost.unloadPlugin ((int) methodArgs["slotId"]);
            result = juce::var (true);
        }
        else if (method == "noteOn")
        {
            pluginHost.sendMidiNote ((int) methodArgs["slotId"], (int) methodArgs["channel"],
                                     (int) methodArgs["note"], (int) methodArgs["velocity"], true);
            result = juce::var (true);
        }
        else if (method == "noteOff")
        {
            pluginHost.sendMidiNote ((int) methodArgs["slotId"], (int) methodArgs["channel"],
                                     (int) methodArgs["note"], 0, false);
            result = juce::var (true);
        }
        else if (method == "midiCC")
        {
            pluginHost.sendMidiCC ((int) methodArgs["slotId"], (int) methodArgs["channel"],
                                   (int) methodArgs["cc"], (int) methodArgs["value"]);
            result = juce::var (true);
        }
        else if (method == "setParameter")
        {
            pluginHost.setParameter ((int) methodArgs["slotId"], (int) methodArgs["paramIndex"],
                                     (float) (double) methodArgs["value"]);
            result = juce::var (true);
        }
        else if (method == "getParameters")
            result = pluginHost.getParameters ((int) methodArgs["slotId"]);
        else if (method == "getLoadedPlugins")
            result = pluginHost.getLoadedPlugins();
        else if (method == "showEditor")
        {
            pluginHost.showEditor ((int) methodArgs["slotId"], (bool) methodArgs["show"]);
            result = juce::var (true);
        }
        else
        {
            sendCallback (cb, makeError ("Unknown VST method: " + method));
            return;
        }
        sendCallback (cb, result);
    }
    catch (...)
    {
        sendCallback (cb, makeError ("Exception in VST method: " + method));
    }
}

void NativeBridge::handleVstScanFolder (const juce::var& args, const juce::String& cb)
{
    if (! args.isArray() || args.getArray()->isEmpty())
    {
        sendCallback (cb, makeError ("Missing folder path"));
        return;
    }
    auto path = args.getArray()->getReference (0).toString();
    auto result = pluginHost.scanDirectory (path);
    sendCallback (cb, result);
}

