#pragma once

#include "Package.h"
#include "ILuauModule.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace Luwow::Engine {

// Tag for the Engine object in the Luau userdata
#define EngineTag 1
typedef void (*CompilerCallbackType)(const std::filesystem::path& modulePath, std::string& resultingBytecode);
typedef void (*DebuggerNewLuauCallbackType)(lua_State* L, const std::string& full_path, bool is_entry);
typedef void (*MessagePumpCallbackType)();

class Engine {
public:
    Engine(Package context, std::filesystem::path filePath);
    ~Engine();

    // Statically registered internal binary modules, independent of Luau state or Engine.
    static void registerInternalModule(std::shared_ptr<ILuauModule> module);

    void setCompilerCallback(CompilerCallbackType callback);
    void setDebuggerNewLuauCallback(DebuggerNewLuauCallbackType callback);
    void setMessagePumpCallback(MessagePumpCallbackType callback);

    // Initializes the Luau State with built-ins
    void initialize(int argc, char* argv[]);
    void initializeRequire();
    void initializeGlobalArgs(int argc, char* argv[]);

    bool usesPackage() { return (!usesCompiler && package.getFileCount() > 0); };

    std::string getModuleName(const std::string key);

    int getModuleRef(lua_State* L, const std::string path);
    int setModuleRef(lua_State* L, const std::string path);
    int isInPackage(lua_State* L, const std::string path);

    int compileAndExecute(lua_State* L, const std::string path, const std::string formattedPath);

    int loadModuleFromBytecode(lua_State* L, const std::string& moduleName, const std::string& bytecode, bool saveRef);
    int executeModule(lua_State* L, const std::string& moduleName, const std::string& bytecode, bool saveRef);
    void run();

    lua_State* getMainState() { return mainState; };
private:
    lua_State* mainState;

    bool usesCompiler;
    CompilerCallbackType compilerCallback;
    bool usesDebuggerNewLuauCallback;
    DebuggerNewLuauCallbackType debuggerNewLuauCallback;
    Package package;
    std::filesystem::path filePath;
    bool usesMessagePump;
    MessagePumpCallbackType messagePumpCallback;
    // DLLs and internal modules
    std::unordered_map<std::string, std::shared_ptr<ILuauModule>> modules;
    std::unordered_map<std::string, int> luauModuleRefs;
};

} // namespace Luwow::Engine