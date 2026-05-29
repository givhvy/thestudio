#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────
//  ElevenLabsClient
//  A small, self-contained REST client for the ElevenLabs API. Designed as the
//  template for further "API → DAW" integrations: keep transport/auth here,
//  keep UI in a panel, and hand the result back as a plain audio File.
//
//  All network calls are BLOCKING — always call them from a background thread
//  (the panel does this) and marshal the result back with
//  juce::MessageManager::callAsync.
//
//  Endpoints used:
//    GET  https://api.elevenlabs.io/v1/voices                  (list voices)
//    POST https://api.elevenlabs.io/v1/text-to-speech/{voice}  (synthesize)
// ──────────────────────────────────────────────────────────────────────────
class ElevenLabsClient
{
public:
    struct Voice
    {
        juce::String id;
        juce::String name;
    };

    // ── API-key persistence ───────────────────────────────────────────────
    // Stored alongside the app's other config in %APPDATA%/Stratum DAW.
    static juce::File configFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Stratum DAW");
        dir.createDirectory();
        return dir.getChildFile("elevenlabs.json");
    }

    static juce::String getApiKey()
    {
        auto f = configFile();
        if (!f.existsAsFile())
            return {};
        auto v = juce::JSON::parse(f.loadFileAsString());
        if (auto* obj = v.getDynamicObject())
            return obj->getProperty("apiKey").toString();
        return {};
    }

    static void setApiKey(const juce::String& key)
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("apiKey", key);
        configFile().replaceWithText(juce::JSON::toString(juce::var(obj.release())));
    }

    // ── Built-in default voices ────────────────────────────────────────────
    // The standard ElevenLabs voice library. Works without an extra fetch so
    // the panel is usable the moment a key is entered. Call fetchVoices() to
    // pull the user's custom/cloned voices from their account.
    static std::vector<Voice> defaultVoices()
    {
        return {
            { "21m00Tcm4TlvDq8ikWAM", "Rachel (F)" },
            { "AZnzlk1XvdvUeBnXmlld", "Domi (F)" },
            { "EXAVITQu4vr4xnSDxMaL", "Bella (F)" },
            { "MF3mGyEYCl7XYWbV9V6O", "Elli (F)" },
            { "pNInz6obpgDQGcFmaJgB", "Adam (M)" },
            { "ErXwobaYiN019PkySvjV", "Antoni (M)" },
            { "TxGEqnHWrfWFTfGW9XjX", "Josh (M)" },
            { "VR6AewLTigWG4xSOukaG", "Arnold (M)" },
            { "yoZ06aMxZJJ28mfd3POQ", "Sam (M)" },
        };
    }

    // ── List the account's voices (blocking) ───────────────────────────────
    static std::vector<Voice> fetchVoices(const juce::String& apiKey, juce::String& errorOut)
    {
        std::vector<Voice> out;
        if (apiKey.isEmpty()) { errorOut = "No API key set."; return out; }

        juce::URL url("https://api.elevenlabs.io/v1/voices");
        const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                 .withConnectionTimeoutMs(15000)
                                 .withExtraHeaders("xi-api-key: " + apiKey + "\r\nAccept: application/json");

        auto stream = url.createInputStream(options);
        if (stream == nullptr) { errorOut = "Could not connect to ElevenLabs."; return out; }

        const juce::String body = stream->readEntireStreamAsString();
        auto v = juce::JSON::parse(body);
        if (auto* obj = v.getDynamicObject())
        {
            if (auto* arr = obj->getProperty("voices").getArray())
            {
                for (const auto& item : *arr)
                    if (auto* vo = item.getDynamicObject())
                        out.push_back({ vo->getProperty("voice_id").toString(),
                                        vo->getProperty("name").toString() });
            }
        }

        if (out.empty())
            errorOut = body.contains("\"detail\"")
                           ? ("API error: " + body.substring(0, 200))
                           : "No voices returned (check the API key).";
        return out;
    }

    // ── Text-to-speech (blocking). Writes MP3 to outFile on success. ────────
    static bool textToSpeech(const juce::String& apiKey,
                             const juce::String& voiceId,
                             const juce::String& text,
                             const juce::String& modelId,
                             double stability,
                             double similarityBoost,
                             const juce::File& outFile,
                             juce::String& errorOut)
    {
        if (apiKey.isEmpty()) { errorOut = "No API key set."; return false; }
        if (voiceId.isEmpty()) { errorOut = "No voice selected."; return false; }
        if (text.trim().isEmpty()) { errorOut = "Nothing to synthesize."; return false; }

        // Build the JSON request body.
        auto settings = std::make_unique<juce::DynamicObject>();
        settings->setProperty("stability", stability);
        settings->setProperty("similarity_boost", similarityBoost);

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("text", text);
        root->setProperty("model_id", modelId.isNotEmpty() ? modelId : juce::String("eleven_multilingual_v2"));
        root->setProperty("voice_settings", juce::var(settings.release()));
        const juce::String jsonBody = juce::JSON::toString(juce::var(root.release()));

        juce::URL url("https://api.elevenlabs.io/v1/text-to-speech/" + voiceId);
        url = url.withPOSTData(jsonBody);

        const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                 .withConnectionTimeoutMs(60000)
                                 .withExtraHeaders("xi-api-key: " + apiKey
                                                   + "\r\nContent-Type: application/json"
                                                   + "\r\nAccept: audio/mpeg");

        auto stream = url.createInputStream(options);
        if (stream == nullptr) { errorOut = "Could not connect to ElevenLabs."; return false; }

        juce::MemoryBlock audio;
        stream->readIntoMemoryBlock(audio);

        // An error response comes back as JSON, not audio — detect that.
        if (audio.getSize() < 1024
            || (audio.getSize() > 0 && static_cast<const char*>(audio.getData())[0] == '{'))
        {
            const juce::String err(static_cast<const char*>(audio.getData()),
                                   juce::jmin((size_t)400, audio.getSize()));
            errorOut = err.isNotEmpty() ? ("API error: " + err) : "Empty audio response.";
            return false;
        }

        if (!outFile.getParentDirectory().exists())
            outFile.getParentDirectory().createDirectory();

        outFile.deleteFile();
        if (auto out = std::unique_ptr<juce::FileOutputStream>(outFile.createOutputStream()))
        {
            out->write(audio.getData(), audio.getSize());
            out->flush();
            return true;
        }

        errorOut = "Could not write output file.";
        return false;
    }

    // Where generated clips are saved.
    static juce::File outputDir()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Stratum DAW").getChildFile("ElevenLabs");
        dir.createDirectory();
        return dir;
    }
};
