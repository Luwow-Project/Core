#pragma once

#include "ILuauHost.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Luwow::Engine {

/*
    Topic based messaging between modules that's safe to use from any thread.
    Published messages are queued per subscription, so they're read on the subscriber's thread.
    Requests run their topic's handler right away on the sender's thread, for services that must reply.
*/
class MessageBus {
public:
    int subscribe(const std::string& topic);
    void unsubscribe(int subscription);
    void publish(const std::string& topic, const std::string& data);
    bool receive(int subscription, Message& message, int timeoutMs);

    bool handle(const std::string& topic, RequestHandler handler, void* context);
    bool request(Message& message);
    bool hasHandler(const std::string& topic);

    // Wakes every waiting receiver so further receives hand out what's still queued, then closes the bus.
    void close();

private:
    struct Subscription {
        std::string topic;
        std::deque<Message> queue;
    };

    struct Handler {
        RequestHandler function;
        void* context;
    };

    std::mutex mutex;
    std::condition_variable messageAvailable;
    std::unordered_map<int, Subscription> subscriptions;
    std::unordered_map<std::string, Handler> handlers;
    int nextSubscription = 1;
    bool closed = false;
};

} // namespace Luwow::Engine
