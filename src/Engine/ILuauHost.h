#pragma once

#include <string>

// Forward declared Lua state
struct lua_State;

namespace Luwow::Engine {

typedef void (*DebuggerLuauCallbackType)(lua_State* L, const std::string& full_path, bool is_entry);
typedef void (*MessagePumpCallbackType)();
typedef int (*TaskSchedulerCallbackType)(lua_State* L, const std::string& chunkName, const std::string& bytecode, bool saveRef);

// Services a host gives to modules. Modules only reach the host through this interface,
// so they behave the same whether they're statically bound or loaded from a DLL.
class ILuauHost {
public:
    virtual ~ILuauHost() = default;

    // Get the main Luau state of this host
    virtual lua_State* getMainState() = 0;

    // Set the callback that runs the event loop after the main script finishes
    virtual void setMessagePumpCallback(MessagePumpCallbackType callback) = 0;

    // Set the callback that runs modules through a scheduler
    virtual void setTaskSchedulerCallback(TaskSchedulerCallbackType callback) = 0;

    // Notify the debugger, if any, that a chunk was loaded
    virtual void callDebuggerLuauCallback(lua_State* L, const std::string& full_path, bool is_entry) = 0;
};

} // namespace Luwow::Engine
