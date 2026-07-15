#include "ILuauModule.h"
#include "Package.h"
#include "Require.h"
#include "Engine.h"

#include "lua.h"
#include "lualib.h"

#include <cstring>
#include <iostream>
#include <fstream>

namespace Luwow::Engine {

static std::unordered_map<std::string, std::shared_ptr<ILuauModule>> globalModules;

static void formatPath(std::string& path) {
    for (char& c : path) {
        if (c == '\\')
            c = '/';
    }
}

static int static_require(lua_State* L) {
    std::string path = luaL_checkstring(L, 1);
    Engine* engine = static_cast<Engine*>(lua_touserdata(L, lua_upvalueindex(EngineTag)));
    if (!engine) {
        luaL_error(L, "Internal error: Engine not found");
        return 0;
    }
    return luaRequire(engine, L, path);
}

Engine::Engine(Package context, std::filesystem::path filePath) :
    mainState(nullptr),
    usesCompiler(false),
    usesDebuggerLuauCallback(false),
    usesTaskScheduler(false),
    compilerCallback(nullptr),
    debuggerLuauCallback(nullptr),
    taskSchedulerCallback(nullptr),
    package(context),
    filePath(filePath),
    usesMessagePump(false),
    messagePumpCallback(nullptr),
    modules(),
    luauModuleRefs()
{}

Engine::~Engine() {
    if (mainState) {
        for (auto& [name, ref] : luauModuleRefs) {
            lua_unref(mainState, ref);
        }
        lua_close(mainState);
    }
}

void Engine::setCompilerCallback(CompilerCallbackType callback) {
    usesCompiler = true;
    compilerCallback = callback;
}

void Engine::setDebuggerLuauCallback(DebuggerLuauCallbackType callback) {
    usesDebuggerLuauCallback = true;
    debuggerLuauCallback = callback;
}

void Engine::setMessagePumpCallback(MessagePumpCallbackType callback) {
    usesMessagePump = true;
    messagePumpCallback = callback;
}

void Engine::setTaskSchedulerCallback(TaskSchedulerCallbackType callback) {
    usesTaskScheduler = true;
    taskSchedulerCallback = callback;
}

void Engine::initialize(int argc, char* argv[]) {
    mainState = luaL_newstate();
    if (!mainState) {
        throw std::runtime_error("Failed to create Lua state");
    }

    luaL_openlibs(mainState);
    initializeRequire();
    initializeGlobalArgs(argc, argv);
    initializeNativeModules(mainState);
    luaL_sandbox(mainState);
}

void Engine::initializeRequire() {
    lua_pushlightuserdata(mainState, this);
    lua_pushcclosure(mainState, static_require, "require", EngineTag);
    lua_setglobal(mainState, "require");
}

void Engine::initializeGlobalArgs(int argc, char* argv[]) {
    lua_newtable(mainState);
    for (int i = 2; i < argc; ++i) {
        lua_pushstring(mainState, argv[i]);
        lua_rawseti(mainState, -2, i - 1);
    }
    lua_setglobal(mainState, "GlobalArgs");
}

void Engine::registerNativeModule(std::shared_ptr<ILuauModule> module) {
    const char* moduleName = module->getModuleName();
    const char* moduleAlias = module->getModuleAlias();
    std::string moduleKey = std::string(moduleAlias) + "/" + moduleName;
    globalModules[moduleKey] = module;
}

void Engine::initializeNativeModules(lua_State* L) {
    for (auto& module: globalModules) {
        int res = initNativeModule(L, module.first);
        if (!res) std::cout << "Could not initialize native module: " << module.first << "\n";
    }
}

void Engine::callDebuggerLuauCallback(lua_State* L, const std::string& full_path, bool is_entry) {
    if (!usesDebuggerLuauCallback) return;
    debuggerLuauCallback(L, full_path, true);
}

int Engine::initNativeModule(lua_State* L, const std::string path) {
    auto module = globalModules.find(path);
    if (module != globalModules.end()) {
        ILuauModule* initializedModule = module->second->initialize(this);
        modules[path] = std::shared_ptr<ILuauModule>(initializedModule);

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
        luauModuleRefs[path] = lua_ref(L, -1);
        return 1;
    }
    return 0;
}

int Engine::executeModule(lua_State* L, const std::string& chunkName, const std::string& bytecode, bool saveRef, bool useGivenState) {
    lua_State* T = (useGivenState) ? lua_newthread(L) : lua_newthread(mainState);
    luaL_sandboxthread(T);

    int topres = lua_gettop(T);
    int result = luau_load(T, chunkName.c_str(), bytecode.data(), bytecode.size(), 0);
    if (result != 0) {
        luaL_error(L, "Failed to load bytecode for module: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
    }

    callDebuggerLuauCallback(L, chunkName, true);    

    int status = lua_resume(T, nullptr, 0);
    if (status != LUA_OK) {
        std::cout << (status == LUA_YIELD ? "Encountered an unexpected yield during Luau execution, please use a scheduler." : "Could not execute module: " + std::string(lua_tostring(T, -1))) << "\n";
        lua_pop(L, -1);
        return 0;
    }

    if (saveRef) {
        if ((lua_gettop(T) - topres) == 0) {
            luaL_error(L, "%s didn't return exactly one value", chunkName.c_str());
            return 0;
        }
        lua_xmove(T, L, 1);
    }

    return 1;
}

int Engine::loadModuleFromBytecode(lua_State* L, const std::string& chunkName, const std::string& bytecode, bool saveRef, bool useGivenState) {
    int status = usesTaskScheduler ? taskSchedulerCallback(L, chunkName, bytecode, saveRef) : executeModule(L, chunkName, bytecode, saveRef, useGivenState);
    if (!status) return 0;

    if (saveRef) {
        luauModuleRefs[chunkName] = lua_ref(L, -1);
        lua_remove(L, 1);
    } else {
        lua_pop(L, 1);
    }

    return 1;
}

std::string Engine::getModuleName(const std::string key) {
    auto it = globalModules.find(key);
    return (it == globalModules.end()) ? std::string() : key;
}

int Engine::getModuleRef(lua_State* L, const std::string path) {
    auto moduleRef = luauModuleRefs.find(path);
    if (moduleRef != luauModuleRefs.end()) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, moduleRef->second);
        return 1;
    }
    return 0;
}

int Engine::isInPackage(lua_State* L, const std::string path, bool useGivenState) {
    int index = package.indexOfFile(path);
    if (index != -1) {
        std::string bytecode = package.getFileContent(index);
        return loadModuleFromBytecode(L, path, bytecode, true, useGivenState);
    }
    return 0;
}

int Engine::compileAndExecute(lua_State* L, const std::string path, const std::string formattedPath, bool useGivenState) {
    if (!usesCompiler) return 0;
    if (std::filesystem::exists(path) && usesCompiler) {
        std::string bytecode;
        compilerCallback(path, bytecode);
        return loadModuleFromBytecode(L, formattedPath, bytecode, true, useGivenState);
    }
    return 0;
}

void Engine::run() {
    try {
        std::string chunkName;
        std::string bytecode;

        if (usesCompiler) {
            // Compile the first file in the package
            compilerCallback(filePath.string(), bytecode);
            chunkName = filePath.string();
        } else {
            // Load bytecode from the first file in the package
            if (package.getFileCount() == 0) {
                throw std::runtime_error("Package has no files");
            }

            bytecode = package.getFileContent(0);
            chunkName = package.getFileName(0).c_str();
        }

        formatPath(chunkName);

        int status = loadModuleFromBytecode(mainState, chunkName, bytecode, false, false);
        if (!status) throw std::runtime_error("Could not execute module: " + chunkName);

        if (usesMessagePump) {
            messagePumpCallback();
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to execute script: " << e.what() << "\n";
    }
}

} // namespace Luwow::Engine