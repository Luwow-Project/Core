#pragma once

// Forward declared Lua state
struct lua_State;

// Type definition for Luau function exports
struct LuauExport {
    const char* name;
    int (*func)(lua_State* L);
};

namespace Luwow::Engine {

class ILuauHost;

enum class RunMode {
    Serial, // Runs on the main thread.
    Parallel // Runs on their own thread.
};

// Interface for classes that want to export functions to Luau
class ILuauModule {
public:
    virtual ~ILuauModule() = default;

    // Gets the module name
    virtual const char* getModuleName() const = 0;

    // Gets the module alias
    virtual const char* getModuleAlias() const = 0;

    // Gets the module exports
    virtual const LuauExport* getExports() const = 0;

    // Gets how the module is run, libraries return LUWOW_MODULE_RUN_MODE which CMake sets.
    virtual RunMode getRunMode() const = 0;

    // Creates a module instance for the given host
    virtual ILuauModule* initialize(ILuauHost* host) = 0;

    // Runs the module's loop until it has no work left.
    // Code calling into Luau from here must hold the host's state lock.
    virtual void run() {}

    // Asks a parallel module's run to finish its remaining work and return, called from another thread.
    virtual void stop() {}
};

} // namespace Luwow::Engine

#define LUWOW_MODULE_ABI_VERSION 1

// The run mode a library was built with, from the LUWOW_<NAME>_MODE CMake option
#if defined(LUWOW_MODULE_PARALLEL) && LUWOW_MODULE_PARALLEL
    #define LUWOW_MODULE_RUN_MODE Luwow::Engine::RunMode::Parallel
#else
    #define LUWOW_MODULE_RUN_MODE Luwow::Engine::RunMode::Serial
#endif

// Symbols every module DLL exports
#define LUWOW_MODULE_ABI_SYMBOL "luwow_module_abi"
#define LUWOW_CREATE_MODULE_SYMBOL "luwow_create_module"

#if defined(_WIN32)
    #define LUWOW_MODULE_EXPORT extern "C" __declspec(dllexport)
#else
    #define LUWOW_MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define LUWOW_CONCAT_IMPL(a, b) a##b
#define LUWOW_CONCAT(a, b) LUWOW_CONCAT_IMPL(a, b)

/*
    Depending on how the library is being built, we export certain functions,
    or we define a uniquely named factory for the executable the library is being bound to.
*/
#if defined(LUWOW_BUILDING_MODULE)
    #define LUWOW_REGISTER_MODULE(Type) \
        LUWOW_MODULE_EXPORT int luwow_module_abi() { return LUWOW_MODULE_ABI_VERSION; } \
        LUWOW_MODULE_EXPORT Luwow::Engine::ILuauModule* luwow_create_module() { return new Type(); }
#elif defined(LUWOW_MODULE_ID)
    #define LUWOW_REGISTER_MODULE(Type) \
        Luwow::Engine::ILuauModule* LUWOW_CONCAT(luwow_create_module_, LUWOW_MODULE_ID)() { return new Type(); }
#else
    #define LUWOW_REGISTER_MODULE(Type)
#endif
