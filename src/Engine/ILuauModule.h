#pragma once

// Forward declared Lua state
struct lua_State;

// Type definition for Luau function exports
struct LuauExport {
    const char* name;
    int (*func)(lua_State* L);
};

namespace Luwow::Engine {

class Engine;

// Interface for classes that want to export functions to Luau
class ILuauModule {
public:
    virtual ~ILuauModule() = default;
    
    // Get the module name
    virtual const char* getModuleName() const = 0;
    
    // Get the module exports
    virtual const LuauExport* getExports() const = 0;

    // Initialize the module
    virtual ILuauModule* initialize(Engine* engine) = 0;
};

} // namespace Luwow::Engine