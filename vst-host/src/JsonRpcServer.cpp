#include "JsonRpcServer.h"

JsonRpcServer::JsonRpcServer(int port)
    : juce::Thread("JsonRpcServer"), port_(port)
{
    startThread();
}

JsonRpcServer::~JsonRpcServer()
{
    stopThread(2000);
}

void JsonRpcServer::registerMethod(const juce::String& name, Handler h)
{
    handlers_[name.toStdString()] = std::move(h);
}

void JsonRpcServer::broadcastEvent(const juce::String& event, const juce::var& data)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("event", event);
    obj->setProperty("data", data);
    juce::String line = juce::JSON::toString(juce::var(obj.get()), true) + "\n";

    juce::ScopedLock sl(clientLock_);
    for (auto* c : clients_)
        if (c->isConnected())
            c->write(line.toRawUTF8(), (int)line.getNumBytesAsUTF8());
}

void JsonRpcServer::run()
{
    juce::StreamingSocket server;
    if (!server.createListener(port_))
    {
        juce::Logger::writeToLog("JsonRpcServer: failed to listen on port " + juce::String(port_));
        return;
    }
    juce::Logger::writeToLog("JsonRpcServer: listening on port " + juce::String(port_));

    while (!threadShouldExit())
    {
        if (server.waitUntilReady(true, 500) == 1)
        {
            auto* client = server.waitForNextConnection();
            if (client)
            {
                {
                    juce::ScopedLock sl(clientLock_);
                    clients_.add(client);
                }
                // Handle client on a detached lambda thread
                juce::Thread::launch([this, client] { handleClient(client); });
            }
        }
    }
}

void JsonRpcServer::handleClient(juce::StreamingSocket* client)
{
    juce::String buffer;
    char tmp[4096];

    while (!threadShouldExit() && client->isConnected())
    {
        int bytes = client->read(tmp, sizeof(tmp) - 1, false);
        if (bytes <= 0)
            break;
        tmp[bytes] = '\0';
        buffer += juce::String::fromUTF8(tmp, bytes);

        // Process all complete lines
        int newlinePos;
        while ((newlinePos = buffer.indexOfChar('\n')) >= 0)
        {
            juce::String line = buffer.substring(0, newlinePos).trim();
            buffer = buffer.substring(newlinePos + 1);
            if (line.isEmpty()) continue;

            juce::var req;
            juce::Result parseResult = juce::JSON::parse(line, req);
            if (parseResult.failed())
            {
                juce::String err = R"({"id":null,"error":"parse error"})" "\n";
                client->write(err.toRawUTF8(), (int)err.getNumBytesAsUTF8());
                continue;
            }

            auto id = req["id"];
            juce::String method = req["method"].toString();
            const juce::var& params = req["params"];

            juce::DynamicObject::Ptr resp = new juce::DynamicObject();
            resp->setProperty("id", id);

            auto it = handlers_.find(method.toStdString());
            if (it == handlers_.end())
            {
                resp->setProperty("error", "unknown method: " + method);
            }
            else
            {
                try
                {
                    juce::var result = it->second(params);
                    resp->setProperty("result", result);
                }
                catch (const std::exception& e)
                {
                    resp->setProperty("error", juce::String(e.what()));
                }
            }

            juce::String respLine = juce::JSON::toString(juce::var(resp.get()), true) + "\n";
            client->write(respLine.toRawUTF8(), (int)respLine.getNumBytesAsUTF8());
        }
    }

    {
        juce::ScopedLock sl(clientLock_);
        clients_.removeObject(client);
    }
}
