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

// Interface for classes that want to export functions to Luau
class ILuauModule {
public:
    virtual ~ILuauModule() = default;
    
    // Get the module name
    virtual const char* getModuleName() const = 0;

    // Get the module alias
    virtual const char* getModuleAlias() const = 0;
    
    // Get the module exports
    virtual const LuauExport* getExports() const = 0;

    // Create a module instance for the given host
    virtual ILuauModule* initialize(ILuauHost* host) = 0;
};

} // namespace Luwow::Engine

// Bumped whenever ILuauModule or ILuauHost change, DLLs built for another version are rejected
#define LUWOW_MODULE_ABI_VERSION 1

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

// Declares the entry point of a library module, use once per library at global scope.
// CMake decides the form: DLL exports for a module DLL, or a uniquely named factory
// (luwow_create_module_<id>) that runscript registers when the library is statically bound.
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
