#include "Engine.h"
#include "lua.h"
#include "lualib.h"
#include "Package.h"
#include "ILuauModule.h"
#include <cstring>
#include <iostream>
#include <fstream>

namespace Luwow::Engine {

static std::unordered_map<std::string, std::shared_ptr<ILuauModule>> globalModules;

/*static*/ void Engine::registerInternalModule(std::shared_ptr<ILuauModule> module) {
    globalModules[module->getModuleName()] = module;
}

Engine::Engine(Package context, std::filesystem::path filePath) :
    L(nullptr),
    usesCompiler(false),
    usesDebuggerNewLuauCallback(false),
    compilerCallback(nullptr),
    debuggerNewLuauCallback(nullptr),
    package(context),
    filePath(filePath),
    usesMessagePump(false),
    messagePumpCallback(nullptr),
    modules(),
    luauModuleRefs()
{}

Engine::~Engine() {
    if (L) {
        for (auto& [name, ref] : luauModuleRefs) {
            lua_unref(L, ref);
        }
        lua_close(L);
    }
}

void Engine::setCompilerCallback(CompilerCallbackType callback) {
    usesCompiler = true;
    compilerCallback = callback;
}

void Engine::setDebuggerNewLuauCallback(DebuggerNewLuauCallbackType callback) {
    usesDebuggerNewLuauCallback = true;
    debuggerNewLuauCallback = callback;
}

void Engine::setMessagePumpCallback(MessagePumpCallbackType callback) {
    usesMessagePump = true;
    messagePumpCallback = callback;
}

void Engine::initialize() {
    L = luaL_newstate();
    if (!L) {
        throw std::runtime_error("Failed to create Lua state");
    }

    luaL_openlibs(L);

    initializeRequire();
}

static int static_require(lua_State* L) {
    std::string moduleName = luaL_checkstring(L, 1);
    Engine* engine = static_cast<Engine*>(lua_touserdata(L, lua_upvalueindex(EngineTag)));
    if (!engine) {
        luaL_error(L, "Internal error: Engine not found");
        return 0;
    }
    return engine->require(moduleName);
}

void Engine::initializeRequire() {
    lua_pushlightuserdata(L, this);
    lua_pushcclosure(L, static_require, "require", EngineTag);
    lua_setglobal(L, "require");
}

int Engine::loadModuleFromBytecode(lua_State* L, const std::string& moduleName, const std::string& bytecode) {
    int result = luau_load(L, moduleName.c_str(), bytecode.data(), bytecode.size(), 0);
    if (result != 0) {
        luaL_error(L, "Failed to load bytecode for module: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
    }

    if (usesDebuggerNewLuauCallback) {
        debuggerNewLuauCallback(L, moduleName, false);
    }

    // Execute the module function to get its result
    if (lua_pcall(L, 0, 1, 0) != 0) {
        luaL_error(L, "Failed to execute module: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
    }

    luauModuleRefs[moduleName] = lua_ref(L, -1);
    return 1;
}

int Engine::require(const std::string& moduleName) {
    // TODO: Make this conformant to the require library

    // Check if the module is already loaded
    auto moduleRef = luauModuleRefs.find(moduleName);
    if (moduleRef != luauModuleRefs.end()) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef->second);
        return 1;
    }

    // Is the module in internal modules?
    auto module = globalModules.find(moduleName);
    if (module != globalModules.end()) {
        // We initialize the module here on first require so that modules can have
        // access to the engine - some module implementations might need special
        // handling like preventing the runtime from exiting.
        ILuauModule* initializedModule = module->second->initialize(this);
        modules[moduleName] = std::shared_ptr<ILuauModule>(initializedModule);

        const LuauExport* exports = initializedModule->getExports();
        lua_createtable(L, 0, sizeof(exports) / sizeof(exports[0]));
        for (int i = 0; exports[i].name != nullptr; i++)
        {
            // Create a closure that captures the module instance
            lua_pushlightuserdata(L, initializedModule);
            lua_pushcclosure(L, exports[i].func, exports[i].name, 1);
            lua_setfield(L, -2, exports[i].name);
        }
        lua_setreadonly(L, -1, 1);
        luauModuleRefs[moduleName] = lua_ref(L, -1);
        return 1;
    }

    // Is the module in the package?
    int index = package.indexOfFile(moduleName);
    if (index != -1) {
        std::string bytecode = package.getFileContent(index);
        return loadModuleFromBytecode(L, moduleName, bytecode);
    }

    // Can we find a DLL with the module name?
    std::string dllName = moduleName + ".dll";
    std::filesystem::path dllPath = filePath.parent_path() / dllName;

    // TODO: Implement DLL loading per OS platform
    // HMODULE dllHandle = LoadLibraryA(dllPath.string().c_str());
    if (std::filesystem::exists(dllPath)) {
        luaL_error(L, "DLL loading not yet implemented");
        return 0;
    }

    // If we have a Luau compiler and are given a valid script path, use the compiler.
    std::filesystem::path modulePath = filePath.parent_path() / moduleName;
    if (std::filesystem::exists(modulePath) && usesCompiler) {
        std::string bytecode;
        compilerCallback(modulePath, bytecode);
        return loadModuleFromBytecode(L, moduleName, bytecode);
    }

    // We couldn't find the module in the cache, package, internal modules, DLL, or source script.
    luaL_error(L, "Module not found: %s", moduleName.c_str());
    return 0;
}

void Engine::run() {
    try {
        if (usesCompiler) {
            // Compile the first file in the package
            std::string bytecode;
            compilerCallback(filePath.string(), bytecode);
            int result = luau_load(L, filePath.string().c_str(), bytecode.data(), bytecode.size(), 0);
            if (result != 0) {
                throw std::runtime_error("Failed to load bytecode for script: " + filePath.string());
            }
        } else {
            // Load bytecode from the first file in the package
            if (package.getFileCount() == 0) {
                throw std::runtime_error("Package has no files");
            }
            std::string bytecode = package.getFileContent(0);
            int result = luau_load(L, package.getFileName(0).c_str(), bytecode.data(), bytecode.size(), 0);
            if (result != 0) {
                throw std::runtime_error("Failed to load bytecode for script: " + filePath.string());
            }
        }

        if (usesDebuggerNewLuauCallback) {
            debuggerNewLuauCallback(L, filePath.string(), true);
        }

        if (lua_pcall(L, 0, 0, 0) != 0) {
            std::cerr << "Failed to execute script: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
        }

        if (usesMessagePump) {
            messagePumpCallback();
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to execute script: " << e.what() << std::endl;
    }
}

} // namespace Luwow::Engine