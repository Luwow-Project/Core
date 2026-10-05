#include "MessageBus.h"
#include <chrono>

namespace Luwow::Engine {

int MessageBus::subscribe(const std::string& topic) {
    std::lock_guard<std::mutex> lock(mutex);
    int id = nextSubscription++;
    subscriptions[id] = Subscription { topic, {} };
    return id;
}

void MessageBus::unsubscribe(int subscription) {
    std::lock_guard<std::mutex> lock(mutex);
    subscriptions.erase(subscription);
    messageAvailable.notify_all();
}

void MessageBus::publish(const std::string& topic, const std::string& data) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (closed) return;

        for (auto& [id, subscription] : subscriptions) {
            if (subscription.topic == topic) {
                subscription.queue.push_back(Message { topic, data });
            }
        }
    }
    messageAvailable.notify_all();
}

// Waits for a message on the subscription, a closed bus or a removed subscription end the wait.
// Once the bus is closed, messages already queued are still handed out, so subscribers can finish their work.
bool MessageBus::receive(int subscription, Message& message, int timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex);

    auto ready = [&] {
        if (closed) return true;
        auto it = subscriptions.find(subscription);
        return it == subscriptions.end() || !it->second.queue.empty();
    };

    if (timeoutMs < 0) {
        messageAvailable.wait(lock, ready);
    } else if (!messageAvailable.wait_for(lock, std::chrono::milliseconds(timeoutMs), ready)) {
        return false;
    }

    auto it = subscriptions.find(subscription);
    if (it == subscriptions.end() || it->second.queue.empty()) return false;

    message = std::move(it->second.queue.front());
    it->second.queue.pop_front();
    return true;
}

bool MessageBus::handle(const std::string& topic, RequestHandler handler, void* context) {
    std::lock_guard<std::mutex> lock(mutex);
    return handlers.emplace(topic, Handler { handler, context }).second;
}

// The handler runs outside the bus lock, so it can use the bus itself
bool MessageBus::request(Message& message) {
    Handler handler;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = handlers.find(message.topic);
        if (it == handlers.end()) return false;
        handler = it->second;
    }

    handler.function(handler.context, message);
    return true;
}

bool MessageBus::hasHandler(const std::string& topic) {
    std::lock_guard<std::mutex> lock(mutex);
    return handlers.find(topic) != handlers.end();
}

void MessageBus::close() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        closed = true;
    }
    messageAvailable.notify_all();
}

} // namespace Luwow::Engine
