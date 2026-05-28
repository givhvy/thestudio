#include "ChordAnalysisEngine.h"

#include <juce_data_structures/juce_data_structures.h>
#include <memory>

namespace
{
juce::File resolveAnalysisDirectory()
{
    const auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();

    const juce::File candidates[] = {
        exeDir.getChildFile("analysis"),
        exeDir.getParentDirectory().getParentDirectory().getParentDirectory().getChildFile("analysis"),
        exeDir.getParentDirectory().getParentDirectory().getChildFile("analysis"),
        juce::File::getCurrentWorkingDirectory().getChildFile("vst-host").getChildFile("analysis"),
        juce::File::getCurrentWorkingDirectory().getChildFile("analysis"),
    };

    for (const auto& dir : candidates)
        if (dir.isDirectory() && dir.getChildFile("analyze_loop.py").existsAsFile())
            return dir;

    return candidates[0];
}
}

juce::File ChordAnalysisEngine::getAnalysisDirectory()
{
    return resolveAnalysisDirectory();
}

juce::File ChordAnalysisEngine::getPythonExecutable()
{
    const auto venvPython = getAnalysisDirectory().getChildFile(".venv").getChildFile("Scripts").getChildFile("python.exe");
    if (venvPython.existsAsFile())
        return venvPython;

#if JUCE_WINDOWS
    return juce::File("python.exe");
#else
    return juce::File("/usr/bin/python3");
#endif
}

juce::File ChordAnalysisEngine::getAnalyzeScript()
{
    return getAnalysisDirectory().getChildFile("analyze_loop.py");
}

bool ChordAnalysisEngine::isReady()
{
    const auto analysisDir = getAnalysisDirectory();
    const auto largeModel = analysisDir.getChildFile("third_party")
                                .getChildFile("LiveChord")
                                .getChildFile("backend")
                                .getChildFile("btc")
                                .getChildFile("btc_model_large_voca.pt");
    const auto chordMiniModel = analysisDir.getChildFile("third_party")
                                  .getChildFile("ChordMini")
                                  .getChildFile("checkpoints")
                                  .getChildFile("btc_model_large_voca.pt");

    return getAnalyzeScript().existsAsFile()
        && getPythonExecutable().existsAsFile()
        && (largeModel.existsAsFile() || chordMiniModel.existsAsFile());
}

void ChordAnalysisEngine::cancelPending()
{
    cancelled_.store(true);
}

ChordAnalysisEngine::Result ChordAnalysisEngine::parseJsonOutput(const juce::String& jsonText)
{
    Result result;
    const auto parsed = juce::JSON::parse(jsonText);
    if (! parsed.isObject())
    {
        result.error = "Invalid analysis JSON";
        return result;
    }

    result.ok = (bool) parsed.getProperty("ok", false);
    result.error = parsed.getProperty("error", juce::String()).toString();
    result.bpm = (double) parsed.getProperty("bpm", 0.0);
    result.key = parsed.getProperty("key", juce::String()).toString();

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

ChordAnalysisEngine::Result ChordAnalysisEngine::runAnalysis(const juce::File& audioFile,
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
        result.error = "ML analysis not installed. Run vst-host\\analysis\\setup-analysis.ps1";
        return result;
    }

    const auto tempJson = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getNonexistentChildFile("stratum-bass-analysis", ".json");

    juce::StringArray args;
    args.add(getPythonExecutable().getFullPathName());
    args.add(getAnalyzeScript().getFullPathName());
    args.add("--input");
    args.add(audioFile.getFullPathName());
    if (bpmHint > 0.0)
    {
        args.add("--bpm");
        args.add(juce::String(bpmHint, 2));
    }
    if (maxSteps > 0)
    {
        args.add("--max-steps");
        args.add(juce::String(maxSteps));
    }
    args.add("--output");
    args.add(tempJson.getFullPathName());
    args.add("--json");

    juce::ChildProcess process;
    if (! process.start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        result.error = "Failed to start Python analysis process";
        return result;
    }

    process.waitForProcessToFinish(600000);
    const juce::String stdOut = process.readAllProcessOutput();

    juce::String jsonText;
    if (tempJson.existsAsFile())
        jsonText = tempJson.loadFileAsString();
    else
        jsonText = stdOut.trim();

    tempJson.deleteFile();

    result = parseJsonOutput(jsonText);
    if (! result.ok && result.error.isEmpty())
        result.error = stdOut.trim().isNotEmpty() ? stdOut.trim() : "Analysis failed";

    return result;
}

void ChordAnalysisEngine::analyzeAsync(const juce::File& audioFile,
                                        double bpmHint,
                                        int maxSteps,
                                        Completion onComplete)
{
    cancelled_.store(false);
    const juce::File fileCopy = audioFile;

    juce::Thread::launch([this, fileCopy, bpmHint, maxSteps, cb = std::move(onComplete)]()
    {
        auto result = runAnalysis(fileCopy, bpmHint, maxSteps);

        juce::MessageManager::callAsync([this, cb = std::move(cb), result = std::move(result)]() mutable
        {
            if (cancelled_.load())
                return;
            if (cb)
                cb(std::move(result));
        });
    });
}
