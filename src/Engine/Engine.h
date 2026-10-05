#pragma once

#include "Package.h"
#include "ILuauModule.h"
#include "ILuauHost.h"
#include "MessageBus.h"
#include "ModuleLayers.h"
#include "StateMutex.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <vector>

namespace Luwow::Engine {

// Forward declared config struct
struct Config;

// Tag for the Engine object in the Luau userdata
#define EngineTag 1

class Engine : public ILuauHost {
public:
    Engine(Package context, std::filesystem::path filePath);
    ~Engine();

    // Statically registered native binary modules, independent of Luau state or Engine.
    static void registerNativeModule(std::shared_ptr<ILuauModule> module);
    void initializeNativeModules(lua_State* L);
    int initNativeModule(lua_State* L, const std::string path);

    // Loads module DLLs and registers their modules, only allowed in executables that export the Luau API.
    void loadDynamicModules(const std::vector<std::filesystem::path>& paths);
    void setDynamicModulesEnabled(bool enabled) { dynamicModulesEnabled = enabled; };

    void lockState() override { stateMutex.lock(); };
    void unlockState() override { stateMutex.unlock(); };
    int releaseState() override { return stateMutex.release(); };
    void reacquireState(int held) override { stateMutex.reacquire(held); };

    int subscribe(const std::string& topic) override { return messageBus.subscribe(topic); };
    void unsubscribe(int subscription) override { messageBus.unsubscribe(subscription); };
    void publish(const std::string& topic, const std::string& data) override { messageBus.publish(topic, data); };
    bool receive(int subscription, Message& message, int timeoutMs) override;

    bool handle(const std::string& topic, RequestHandler handler, void* context) override { return messageBus.handle(topic, handler, context); };
    bool request(Message& message) override { return messageBus.request(message); };

    // Initializes the Luau State with built-ins
    void initialize(int argc, char* argv[]);
    void initializeRequire();
    void initializeGlobalArgs(int argc, char* argv[]);
    void initializeConfig();
    void initializeRuntimeSpecification();
    
    // Sets the configuration path data.
    void setConfigPath(char* path) { configPath = std::filesystem::path(path); };
    void setConfigPath(std::filesystem::path path) { configPath = path; };
    void setConfig(Config* configRef) { config = configRef; };

    // Scripts are compiled when something handles compiler-compile, otherwise they come from the package.
    bool usesPackage() { return (!messageBus.hasHandler(Topics::CompilerCompile) && package.getFileCount() > 0); };

    std::string getModuleName(const std::string key);

    int getModuleRef(lua_State* L, const std::string path);
    int isInPackage(lua_State* L, const std::string path, bool useGivenState);

    int compileAndExecute(lua_State* L, const std::string path, const std::string formattedPath, bool useGivenState);
    int loadModuleFromBytecode(lua_State* L, const std::string& chunkName, const std::string& bytecode, bool saveRef, bool useGivenState);
    int executeModule(lua_State* L, const std::string& chunkName, const std::string& bytecode, bool saveRef, bool useGivenState);
    void run();

    // Stops the parallel modules and waits for their threads.
    void shutdownModules();

    std::filesystem::path getConfigPath() { return configPath; };
    lua_State* getMainState() override { return mainState; };
    Config* getConfig() { return config; };
private:
    bool compile(const std::string& path, std::string& bytecode);

    lua_State* mainState;
    Config* config = nullptr;

    Package package;
    std::filesystem::path filePath;
    std::filesystem::path configPath;

    // DLLs and native modules
    std::unordered_map<std::string, std::shared_ptr<ILuauModule>> modules;
    std::unordered_map<std::string, int> luauModuleRefs;
    bool dynamicModulesEnabled = false;
    bool nativeModulesInitialized = false;

    // Parallel/Serial library layer system
    StateMutex stateMutex;
    MessageBus messageBus;
    SerialLayer serialLayer;
    ParallelLayer parallelLayer;
};

} // namespace Luwow::Engine
