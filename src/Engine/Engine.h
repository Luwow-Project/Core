#pragma once

#include "Package.h"
#include "ILuauModule.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace Luwow::Engine {

// Forward declared config struct
struct Config;

// Tag for the Engine object in the Luau userdata
#define EngineTag 1
typedef void (*CompilerCallbackType)(const std::filesystem::path& modulePath, std::string& resultingBytecode);
typedef void (*DebuggerLuauCallbackType)(lua_State* L, const std::string& full_path, bool is_entry);
typedef void (*MessagePumpCallbackType)();
typedef int (*TaskSchedulerCallbackType)(lua_State* L, const std::string& chunkName, const std::string& bytecode, bool saveRef);

class Engine {
public:
    Engine(Package context, std::filesystem::path filePath);
    ~Engine();

    // Statically registered native binary modules, independent of Luau state or Engine.
    static void registerNativeModule(std::shared_ptr<ILuauModule> module);
    void initializeNativeModules(lua_State* L);
    int initNativeModule(lua_State* L, const std::string path);

    void setCompilerCallback(CompilerCallbackType callback);
    void setDebuggerLuauCallback(DebuggerLuauCallbackType callback);
    void setMessagePumpCallback(MessagePumpCallbackType callback);
    void setTaskSchedulerCallback(TaskSchedulerCallbackType callback);

    void callDebuggerLuauCallback(lua_State* L, const std::string& full_path, bool is_entry);

    // Initializes the Luau State with built-ins
    void initialize(int argc, char* argv[]);
    void initializeRequire();
    void initializeGlobalArgs(int argc, char* argv[]);
    void initializeRuntimeSpecification();
    void setConfigPath(char* path) { configPath = std::filesystem::path(path); };
    void setConfigPath(std::filesystem::path path) { configPath = path; }; 
    void setConfig(Config* configRef) { config = configRef; };

    bool usesPackage() { return (!usesCompiler && package.getFileCount() > 0); };

    std::string getModuleName(const std::string key);

    int getModuleRef(lua_State* L, const std::string path);
    int isInPackage(lua_State* L, const std::string path, bool useGivenState);

    int compileAndExecute(lua_State* L, const std::string path, const std::string formattedPath, bool useGivenState);
    int loadModuleFromBytecode(lua_State* L, const std::string& chunkName, const std::string& bytecode, bool saveRef, bool useGivenState);
    int executeModule(lua_State* L, const std::string& chunkName, const std::string& bytecode, bool saveRef, bool useGivenState);
    void run();

    std::filesystem::path getConfigPath() { return configPath; };
    lua_State* getMainState() { return mainState; };
    Config* getConfig() { return config; };
private:
    lua_State* mainState;
    Config* config = nullptr;

    bool usesCompiler;
    CompilerCallbackType compilerCallback;
    bool usesDebuggerLuauCallback;
    DebuggerLuauCallbackType debuggerLuauCallback;
    bool usesMessagePump;
    MessagePumpCallbackType messagePumpCallback;
    bool usesTaskScheduler;
    TaskSchedulerCallbackType taskSchedulerCallback;
    Package package;
    std::filesystem::path filePath;
    std::filesystem::path configPath;
    // DLLs and internal modules
    std::unordered_map<std::string, std::shared_ptr<ILuauModule>> modules;
    std::unordered_map<std::string, int> luauModuleRefs;
};

} // namespace Luwow::Engine