#pragma once

#include "ILuauHost.h"

#include "lua.h"
#include "lualib.h"

#include <exception>
#include <functional>
#include <string>
#include <thread>
#include <utility>

namespace Luwow::Engine {

// An error to raise, or a function pushing its results.
// The push runs once the state is held again.
struct OffThreadResult {
    std::string error;
    std::function<int(lua_State*)> push;
};

// The continuation of a C function that yielded in runOffThread.
// Raises the work's error in the coroutine, or returns its results.
inline int finishOffThread(lua_State* L, int status) {
    (void)status;
    bool ok = lua_toboolean(L, -1);
    int count = static_cast<int>(lua_tointeger(L, -2));
    lua_pop(L, 2);
    if (!ok) lua_error(L);
    return count;
}

/*
    Runs blocking work on a thread of its own while the calling coroutine yields,
    this allows a scheduler to keep running other threads meanwhile.
    Once the work is complete, the coroutine is resumed with what its result pushes,
    or the error is raised within it. The callinc C function returns what this returns,
    and must have finishOffThread as its continuation.

    From a thread that cannot yield, or the engine is ran without a scheduler, or when worthwile is false,
    the work runs on the calling thread instead, with the state released meanwhile so other threads can access Luau.
*/
inline int runOffThread(lua_State* L, ILuauHost* host, std::function<OffThreadResult()> work, bool worthwhile = true) {
    auto runGuarded = [](std::function<OffThreadResult()>& job) {
        try {
            return job();
        } catch (const std::exception& e) {
            return OffThreadResult { e.what(), nullptr };
        } catch (...) {
            return OffThreadResult { "unknown error", nullptr };
        }
    };

    // The scheduler holds on while the work is out, so a script waiting on it isn't seen as finished
    Message hold;
    hold.topic = Topics::SchedulerHold;
    hold.result = 1;

    if (!worthwhile || !lua_isyieldable(L) || !host->request(hold)) {
        OffThreadResult result;
        {
            StateRelease release(host);
            result = runGuarded(work);
        }
        if (!result.error.empty()) luaL_error(L, "%s", result.error.c_str());
        int count = result.push ? result.push(L) : 0;
        // The continuation expects the count and success on top
        lua_pushinteger(L, count);
        lua_pushboolean(L, true);
        return finishOffThread(L, 0);
    }

    // The coroutine is kept alive until it's resumed
    lua_pushthread(L);
    int threadRef = lua_ref(L, -1);
    lua_pop(L, 1);

    std::thread([L, host, threadRef, runGuarded, work = std::move(work)]() mutable {
        OffThreadResult result = runGuarded(work);
        StateLock lock(host);
        
        int pushed = 0;
        if (result.error.empty()) {
            int count = result.push ? result.push(L) : 0;
            lua_pushinteger(L, count);
            lua_pushboolean(L, true);
            pushed = count + 2;
        } else {
            lua_pushlstring(L, result.error.data(), result.error.size());
            lua_pushinteger(L, 0);
            lua_pushboolean(L, false);
            pushed = 3;
        }

        Message resume;
        resume.topic = Topics::SchedulerDefer;
        resume.state = L;
        resume.result = pushed;
        host->request(resume);

        Message release;
        release.topic = Topics::SchedulerHold;
        release.result = -1;
        host->request(release);

        lua_unref(L, threadRef);
    }).detach();

    return lua_yield(L, 0);
}

} // namespace Luwow::Engine
