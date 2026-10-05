#pragma once

#include <atomic>
#include <mutex>
#include <thread>

namespace Luwow::Engine {

/*
    The Luau state lock. It's recursive and tracks how many levels its owner holds,
    so a thread can release all of them to wait, then restore them.
*/
class StateMutex {
public:
    void lock() {
        mutex.lock();
        owner = std::this_thread::get_id();
        ++depth;
    }

    void unlock() {
        if (--depth == 0) owner = std::thread::id();
        mutex.unlock();
    }

    // Releases every level this thread holds, 0 if it holds none
    int release() {
        if (owner.load() != std::this_thread::get_id()) return 0;
        int held = depth;
        for (int i = 0; i < held; ++i) unlock();
        return held;
    }

    void reacquire(int held) {
        for (int i = 0; i < held; ++i) lock();
    }

private:
    std::recursive_mutex mutex;
    std::atomic<std::thread::id> owner;
    int depth = 0; // Only changed by the owner while it holds the mutex
};

} // namespace Luwow::Engine
