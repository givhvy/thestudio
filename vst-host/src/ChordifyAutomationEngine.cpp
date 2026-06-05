#include "ChordifyAutomationEngine.h"
#include "ChordAnalysisEngine.h"

#include <juce_data_structures/juce_data_structures.h>

namespace
{
constexpr int DEFAULT_CDP_PORT = 9222;

std::atomic<bool> cdpAvailableCached { false };
std::atomic<bool> cdpCheckInFlight { false };
std::atomic<juce::uint32> cdpLastKickMs { 0 };

bool probeCdpOnce()
{
    juce::URL url("http://127.0.0.1:" + juce::String(DEFAULT_CDP_PORT) + "/json/version");
    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs(400);

    if (const auto stream = url.createInputStream(options))
        return stream->readEntireStreamAsString().containsIgnoreCase("Browser");

    return false;
}

juce::File chordifyReadyFlag()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stratum DAW")
        .getChildFile("chordify.ready");
}

juce::String extractJsonPayload(const juce::String& output)
{
    int searchFrom = 0;
    while (searchFrom < output.length())
    {
        const int start = output.indexOfChar(searchFrom, '{');
        if (start < 0)
            break;

        const int end = output.lastIndexOfChar('}');
        if (end <= start)
            break;

        const auto candidate = output.substring(start, end + 1).trim();
        if (candidate.contains("\"ok\""))
            return candidate;

        searchFrom = start + 1;
    }

    const int start = output.lastIndexOfChar('{');
    const int end = output.lastIndexOfChar('}');
    if (start >= 0 && end > start)
        return output.substring(start, end + 1).trim();
    return output.trim();
}
}

std::atomic<bool> ChordifyAutomationEngine::running_ { false };

juce::File ChordifyAutomationEngine::getProfileDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stratum DAW")
        .getChildFile("chordify-browser-profile");
}

juce::File ChordifyAutomationEngine::getAutomationScript()
{
    return ChordAnalysisEngine::getAnalysisDirectory().getChildFile("chordify_automation.py");
}

juce::File ChordifyAutomationEngine::getPythonExecutable()
{
    return ChordAnalysisEngine::getPythonExecutable();
}

bool ChordifyAutomationEngine::isReady()
{
    return getAutomationScript().existsAsFile()
        && getPythonExecutable().existsAsFile()
        && chordifyReadyFlag().existsAsFile();
}

bool ChordifyAutomationEngine::isCdpAvailable()
{
    return cdpAvailableCached.load();
}

void ChordifyAutomationEngine::refreshCdpStatus()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (now - cdpLastKickMs.load() < 2000)
        return;

    bool expected = false;
    if (! cdpCheckInFlight.compare_exchange_strong(expected, true))
        return;

    cdpLastKickMs.store(now);

    juce::Thread::launch([]()
    {
        cdpAvailableCached.store(probeCdpOnce());
        cdpCheckInFlight.store(false);
    });
}

bool ChordifyAutomationEngine::isFullyReady()
{
    refreshCdpStatus();
    return isReady() && cdpAvailableCached.load() && ! isRunning();
}

juce::File ChordifyAutomationEngine::getLockFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Stratum DAW")
        .getChildFile("chordify.lock");
}

void ChordifyAutomationEngine::forceRestart()
{
    getLockFile().deleteFile();
    running_.store(false);
}

bool ChordifyAutomationEngine::launchChrome()
{
   #if JUCE_WINDOWS
    const auto script = getAutomationScript().getParentDirectory().getChildFile("launch-chordify-chrome.ps1");
    if (! script.existsAsFile())
        return false;

    juce::StringArray args;
    args.add("powershell.exe");
    args.add("-NoProfile");
    args.add("-ExecutionPolicy");
    args.add("Bypass");
    args.add("-File");
    args.add(script.getFullPathName());

    juce::ChildProcess process;
    return process.start(args);
   #else
    const auto script = getAutomationScript().getParentDirectory().getChildFile("launch-chordify-chrome.sh");
    if (! script.existsAsFile())
        return false;

    juce::StringArray args;
    args.add("/bin/zsh");
    args.add(script.getFullPathName());

    juce::ChildProcess process;
    return process.start(args);
   #endif
}

bool ChordifyAutomationEngine::isRunning()
{
    return running_.load();
}

void ChordifyAutomationEngine::cancelPending()
{
    cancelled_.store(true);
}

ChordifyAutomationEngine::Result ChordifyAutomationEngine::parseJsonOutput(const juce::String& jsonText)
{
    Result result;
    const auto parsed = juce::JSON::parse(jsonText);
    if (! parsed.isObject())
    {
        result.error = "Invalid Chordify automation JSON";
        return result;
    }

    result.ok = (bool) parsed.getProperty("ok", false);
    result.error = parsed.getProperty("error", juce::String()).toString();
    result.bpm = (double) parsed.getProperty("bpm", 0.0);

    const juce::String midiPath = parsed.getProperty("midi_path", juce::String()).toString();
    if (midiPath.isNotEmpty())
        result.midiFile = juce::File(midiPath);

    if (const auto* notesVar = parsed.getProperty("notes", juce::var()).getArray())
    {
        for (const auto& noteVar : *notesVar)
        {
            if (! noteVar.isObject())
                continue;

            BassNote note;
            note.pitch = (int) noteVar.getProperty("pitch", 60);
            note.startStep = (int) noteVar.getProperty("startStep", 0);
            note.lengthSteps = juce::jmax(1, (int) noteVar.getProperty("lengthSteps", 1));
            note.velocity = juce::jlimit(1, 127, (int) noteVar.getProperty("velocity", 100));
            result.notes.push_back(note);
        }
    }

    return result;
}

ChordifyAutomationEngine::Result ChordifyAutomationEngine::runAutomation(const juce::File& audioFile,
                                                                            double bpmHint,
                                                                            int maxSteps)
{
    Result result;
    if (! audioFile.existsAsFile())
    {
        result.error = "Audio file not found";
        return result;
    }

    if (! isReady())
    {
        result.error = "Chordify CDP not set up. Run: powershell -File vst-host\\analysis\\run-chordify-login.ps1";
        return result;
    }

    const auto tempMidi = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getNonexistentChildFile("stratum-chordify", ".mid");
    const auto tempJson = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getNonexistentChildFile("stratum-chordify", ".json");

    juce::StringArray args;
    args.add(getPythonExecutable().getFullPathName());
    args.add(getAutomationScript().getFullPathName());
    args.add("--use-cdp");
    args.add("--cdp-port");
    args.add(juce::String(DEFAULT_CDP_PORT));
    args.add("--input");
    args.add(audioFile.getFullPathName());
    args.add("--output");
    args.add(tempMidi.getFullPathName());
    if (bpmHint > 0.0)
    {
        args.add("--bpm");
        args.add(juce::String(bpmHint, 2));
    }
    args.add("--json");

    juce::ChildProcess process;
    if (! process.start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        result.error = "Failed to start Chordify automation";
        return result;
    }

    process.waitForProcessToFinish(600000);
    const juce::String stdOut = process.readAllProcessOutput();

    juce::String jsonText = extractJsonPayload(stdOut);
    if (jsonText.isEmpty() && tempJson.existsAsFile())
        jsonText = tempJson.loadFileAsString();

    tempJson.deleteFile();

    result = parseJsonOutput(jsonText);

    if (! result.ok && tempMidi.existsAsFile() && tempMidi.getSize() > 32)
        result.midiFile = tempMidi;
    else if (result.midiFile.getFullPathName().isEmpty() && tempMidi.existsAsFile())
        result.midiFile = tempMidi;

    if (! result.ok && result.error.isEmpty())
        result.error = stdOut.trim().isNotEmpty() ? stdOut.trim() : "Chordify automation failed";

    return result;
}

void ChordifyAutomationEngine::fetchBassAsync(const juce::File& audioFile,
                                               double bpmHint,
                                               int maxSteps,
                                               Completion onComplete)
{
    cancelled_.store(false);

    if (running_.exchange(true))
    {
        Result busy;
        busy.ok = false;
        busy.error = "Chordify automation already running";
        if (onComplete)
            juce::MessageManager::callAsync([cb = std::move(onComplete), busy = std::move(busy)]() mutable
            {
                if (cb)
                    cb(std::move(busy));
            });
        return;
    }

    const juce::File fileCopy = audioFile;

    juce::Thread::launch([this, fileCopy, bpmHint, maxSteps, cb = std::move(onComplete)]()
    {
        auto result = runAutomation(fileCopy, bpmHint, maxSteps);
        running_.store(false);

        juce::MessageManager::callAsync([this, cb = std::move(cb), result = std::move(result)]() mutable
        {
            if (cancelled_.load())
                return;
            if (cb)
                cb(std::move(result));
        });
    });
}
