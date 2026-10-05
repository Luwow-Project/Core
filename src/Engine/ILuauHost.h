#pragma once

#include <string>

// Forward declared Lua state
struct lua_State;

namespace Luwow::Engine {

// Topics the engine sends requests on, to whoever handles them provides that service.
namespace Topics {
    // Compiles the script at the path in message.data, replying with its bytecode in message.data
    inline constexpr const char* CompilerCompile = "compiler-compile";
    // Tells the debugger message.state is about to run the chunk named in message.data
    inline constexpr const char* DebuggerLoad = "debugger-load";

    /// Scheduler services libraries request, must be provided by a task scheduler library.

    // Runs message.state until it finishes, replying with its status (LUA_YIELD if nothing is left to resume it)
    inline constexpr const char* SchedulerSpawn = "scheduler-spawn";
    // Resumes message.state right away, like task.spawn. Errors are reported. Replies its status in result.
    inline constexpr const char* SchedulerResume = "scheduler-resume";
    // Resumes message.state on the scheduler's next cycle, like task.defer
    inline constexpr const char* SchedulerDefer = "scheduler-defer";
    // Resumes message.state after message.data seconds, like task.delay. A C function can make its
    // own thread wait by requesting this with no values, then returning `lua_yield(L, 0)`.
    inline constexpr const char* SchedulerDelay = "scheduler-delay";
    // Resumes message.state with true once the socket in message.data (its handle as a decimal number)
    // is readable, or with false if it can't be watched. Replies result 1 when the watch started.
    inline constexpr const char* SchedulerWatch = "scheduler-watch";
    // Cancels the pending watch of message.state without resuming it. Once this returns the socket isn't
    // polled anymore and can be closed. Replies result 1 if there was a watch.
    inline constexpr const char* SchedulerUnwatch = "scheduler-unwatch";
}

// A message sent through the host's message bus
struct Message {
    std::string topic;
    std::string data;

    // Only set on requests, which run on the sender's thread while it holds the state lock
    lua_State* state = nullptr;
    int result = 0;
};

// Handles requests on a topic, it fills in the message's reply fields
typedef void (*RequestHandler)(void* context, Message& message);

// Services a host gives to modules. Modules only reach the host through this interface,
// so they behave the same whether they're statically bound or loaded from a DLL.
class ILuauHost {
public:
    virtual ~ILuauHost() = default;
    virtual lua_State* getMainState() = 0;

    // Locks the Luau state. Only one thread may run Luau at a time, so code entering Luau from
    // a module's loop or a callback must hold it. The lock is recursive.
    virtual void lockState() = 0;
    virtual void unlockState() = 0;

    // Releases every level of the state lock this thread holds, returning how many to restore.
    // Used to wait on another thread that needs the state, whatever the caller's lock depth.
    virtual int releaseState() = 0;
    virtual void reacquireState(int held) = 0;

    // Subscribes to a topic, returns the subscription id used to receive its messages
    virtual int subscribe(const std::string& topic) = 0;
    virtual void unsubscribe(int subscription) = 0;

    // Queues a message for every subscriber of the topic, from any thread
    virtual void publish(const std::string& topic, const std::string& data) = 0;

    /*
        Takes the next message of a subscription, waiting up to `timeoutMs` (-1 waits forever).
        The state lock is released while waiting. Returns false on timeout, or once the host is shutting down
        and the subscription has no queued messages left.
    */
    virtual bool receive(int subscription, Message& message, int timeoutMs) = 0;

    // Claims a topic for requests, a topic has at most one handler. Returns false if it's taken.
    virtual bool handle(const std::string& topic, RequestHandler handler, void* context) = 0;

    // Runs the handler of message.topic right away on this thread. Returns false if no one handles it.
    virtual bool request(Message& message) = 0;
};

// Holds a host's state lock for a scope
class StateLock {
public:
    explicit StateLock(ILuauHost* host) : host(host) { host->lockState(); }
    ~StateLock() { host->unlockState(); }

    StateLock(const StateLock&) = delete;
    StateLock& operator=(const StateLock&) = delete;

private:
    ILuauHost* host;
};

// Releases every level of a host's state lock this thread holds for a scope
class StateRelease {
public:
    explicit StateRelease(ILuauHost* host) : host(host), held(host->releaseState()) {}
    ~StateRelease() { host->reacquireState(held); }

    StateRelease(const StateRelease&) = delete;
    StateRelease& operator=(const StateRelease&) = delete;

private:
    ILuauHost* host;
    int held;
};

} // namespace Luwow::Engine
