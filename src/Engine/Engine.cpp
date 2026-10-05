#include "ILuauModule.h"
#include "Package.h"
#include "Require.h"
#include "Engine.h"
#include "Config.h"
#include "ModuleLoader.h"

#include "lua.h"
#include "lualib.h"

#include <cstring>
#include <iostream>
#include <fstream>
#include <mutex>

namespace Luwow::Engine {

static std::unordered_map<std::string, std::shared_ptr<ILuauModule>> globalModules;
static std::mutex globalModulesMutex; // Modules can be registered while other hosts are running

static std::string getModuleKey(const ILuauModule* module) {
    return std::string(module->getModuleAlias()) + "/" + module->getModuleName();
}

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
    package(context),
    filePath(filePath),
    modules(),
    luauModuleRefs()
{}

Engine::~Engine() {
    shutdownModules();

    if (mainState) {
        for (auto& [name, ref] : luauModuleRefs) {
            lua_unref(mainState, ref);
        }
        lua_close(mainState);
    }
}

// Waiting with the state held would keep every other thread out of Luau, so it's released meanwhile
bool Engine::receive(int subscription, Message& message, int timeoutMs) {
    int held = (timeoutMs != 0) ? stateMutex.release() : 0;
    bool received = messageBus.receive(subscription, message, timeoutMs);
    stateMutex.reacquire(held);
    return received;
}

// Compiles a script through the compiler-compile handler, false when nothing handles it
bool Engine::compile(const std::string& path, std::string& bytecode) {
    Message request;
    request.topic = Topics::CompilerCompile;
    request.data = path;
    if (!messageBus.request(request)) return false;

    bytecode = std::move(request.data);
    return true;
}

void Engine::initialize(int argc, char* argv[]) {
    mainState = luaL_newstate();
    if (!mainState) {
        throw std::runtime_error("Failed to create Lua state");
    }

    luaL_openlibs(mainState);
    initializeRequire();
    initializeGlobalArgs(argc, argv);
    initializeConfig();
    initializeNativeModules(mainState);
    initializeRuntimeSpecification();
    luaL_sandbox(mainState);

    // Parallel modules start before the script, so it can use them right away
    parallelLayer.start();
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
    lua_setreadonly(mainState, -1, true);
    lua_setglobal(mainState, "GlobalArgs");
}

void Engine::initializeConfig() {
    if (configPath.empty()) return;

    lua_State* T = lua_newthread(mainState);
    lua_pushstring(T, configPath.generic_string().c_str());
    try {
        Luwow::Engine::getConfig(this, T, std::filesystem::current_path().generic_string());
    } catch (const std::exception& e) {
        std::cerr << "Could not load config " << configPath.string() << ": " << e.what() << "\n";
    }
    lua_pop(mainState, 1);
}

// We do not have versioning or github specifications yet, these may come with an automated build system in the future.
void Engine::initializeRuntimeSpecification() {
    lua_newtable(mainState);
    lua_pushstring(mainState, "luwow");
    lua_setfield(mainState, -2, "name");
    lua_pushstring(mainState, "https://github.com/Luwow-Project");
    lua_setfield(mainState, -2, "url");
    lua_setreadonly(mainState, -1, true);
    lua_setglobal(mainState, "_RUNTIME");
}

void Engine::registerNativeModule(std::shared_ptr<ILuauModule> module) {
    std::lock_guard<std::mutex> lock(globalModulesMutex);
    globalModules[getModuleKey(module.get())] = module;
}

void Engine::initializeNativeModules(lua_State* L) {
    std::vector<std::string> keys;
    {
        std::lock_guard<std::mutex> lock(globalModulesMutex);
        for (auto& module: globalModules) keys.push_back(module.first);
    }

    for (const std::string& key : keys) {
        int res = initNativeModule(L, key);
        if (!res) std::cout << "Could not initialize native module: " << key << "\n";
    }
    nativeModulesInitialized = true;
}

// Registers the module of each DLL, and initializes it right away if native modules already were.
void Engine::loadDynamicModules(const std::vector<std::filesystem::path>& paths) {
    if (paths.empty()) return;

    if (!dynamicModulesEnabled) {
        std::cout << "DLL modules are not supported in this build (all modules are statically linked).\n";
        return;
    }

    for (const std::filesystem::path& path : paths) {
        std::string error;
        std::shared_ptr<ILuauModule> module = ModuleLoader::load(path, error);
        if (!module) {
            std::cout << "Could not load module " << path.string() << ": " << error << "\n";
            continue;
        }

        registerNativeModule(module);

        std::string key = getModuleKey(module.get());
        if (nativeModulesInitialized && modules.find(key) == modules.end()) {
            if (!initNativeModule(mainState, key)) std::cout << "Could not initialize native module: " << key << "\n";
        }
    }
}

int Engine::initNativeModule(lua_State* L, const std::string path) {
    std::shared_ptr<ILuauModule> prototype;
    {
        std::lock_guard<std::mutex> lock(globalModulesMutex);
        auto module = globalModules.find(path);
        if (module != globalModules.end()) prototype = module->second;
    }

    if (prototype) {
        ILuauModule* initializedModule = prototype->initialize(this);
        modules[path] = std::shared_ptr<ILuauModule>(initializedModule);

        if (initializedModule->getRunMode() == RunMode::Parallel) {
            parallelLayer.add(initializedModule);
        } else {
            serialLayer.add(initializedModule);
        }

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

    Message debuggerRequest;
    debuggerRequest.topic = Topics::DebuggerLoad;
    debuggerRequest.data = chunkName;
    debuggerRequest.state = L;
    messageBus.request(debuggerRequest);

    Message spawnRequest;
    spawnRequest.topic = Topics::SchedulerSpawn;
    spawnRequest.state = T;
    bool scheduled = messageBus.request(spawnRequest);
    int status = scheduled ? spawnRequest.result : lua_resume(T, nullptr, 0);

    if (status != LUA_OK) {
        if (status == LUA_YIELD) {
            std::cout << (scheduled ? "Thread yielded with no active tasks." : "Encountered an unexpected yield during Luau execution, please use a scheduler.") << "\n";
        } else {
            const char* error = lua_tostring(T, -1);
            std::cout << "Could not execute module: " << (error ? error : "unknown error") << "\n";
        }
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
    int status = executeModule(L, chunkName, bytecode, saveRef, useGivenState);
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
    std::lock_guard<std::mutex> lock(globalModulesMutex);
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
    if (!std::filesystem::exists(path)) return 0;

    std::string bytecode;
    if (!compile(path, bytecode)) return 0;
    return loadModuleFromBytecode(L, formattedPath, bytecode, true, useGivenState);
}

void Engine::run() {
    try {
        std::string chunkName;
        std::string bytecode;

        if (compile(filePath.string(), bytecode)) {
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

        {
            // Parallel modules wait for the state while the main script runs
            std::lock_guard<StateMutex> lock(stateMutex);
            int status = loadModuleFromBytecode(mainState, chunkName, bytecode, false, false);
            if (!status) throw std::runtime_error("Could not execute module: " + chunkName);
        }

        // Serial loops run on the main thread, once they finish the parallel modules are closed
        serialLayer.run();
    } catch (const std::exception& e) {
        std::cerr << "Failed to execute script: " << e.what() << "\n";
    }

    shutdownModules();
}

void Engine::shutdownModules() {
    parallelLayer.requestStop();
    messageBus.close();
    parallelLayer.join();
}

} // namespace Luwow::Engine