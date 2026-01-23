#include <iostream>
#include <fstream>
#include <filesystem>
#include <debugger.h>
#include "luacode.h"
#include "Engine.h"

using Engine = Luwow::Engine::Engine;
using Package = Luwow::Engine::Package;

// Compiles the script from the filesystem and returns the bytecode
void compilerCallback(const std::filesystem::path& modulePath, std::string& resultingBytecode) {
    std::ifstream file(modulePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open script file: " + modulePath.string());
    }
    std::string script(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    file.close();
    
    std::cout << "Compiling \"" << modulePath << "\"" << std::endl;

    lua_CompileOptions options = {};
    options.optimizationLevel = 1;
    options.debugLevel = 2;
    options.typeInfoLevel = 1;
    options.coverageLevel = 0;
    options.vectorLib = nullptr;
    options.vectorCtor = nullptr;
    options.vectorType = nullptr;
    options.mutableGlobals = nullptr;
    options.userdataTypes = nullptr;
    options.librariesWithKnownMembers = nullptr;
    options.libraryMemberConstantCb = nullptr;
    options.libraryMemberTypeCb = nullptr;
    options.disabledBuiltins = nullptr;

    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(script.c_str(), script.length(), &options, &bytecodeSize);
    if (!bytecode) {
        throw std::runtime_error("Failed to compile script: " + modulePath.string());
    }
    resultingBytecode = std::string(bytecode, bytecodeSize);
}

luau::debugger::Debugger* pDebugger = nullptr;
void debuggerCallback(lua_State* L, const std::string& path, bool is_entry)
{
    if (!pDebugger) return;
    std::filesystem::path filePath(path);
    if (filePath.is_relative()) {
        filePath = std::filesystem::current_path() / filePath;
    }
    std::cout << "Loading file \"" << path << "\" and naming it \"" <<
        filePath.string() << "\"" << std::endl;
    pDebugger->onLuaFileLoaded(L, path, is_entry);
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: lremotedebug <port> <script.luau>" << std::endl;
        std::cerr << "Note that port 59000 is recommended." << std::endl;
        return 1;
    }

    int remoteDebuggerPort = std::atoi(argv[1]);

    // Put your own binary modules here and DLLs here

    // Initialize the luau-debugger component
    auto log_handler = [](std::string_view msg) {
        printf("%s", msg.data());
    };
    auto error_handler = [](std::string_view msg) {
        fprintf(stderr, "%s", msg.data());
    };
    luau::debugger::log::install(log_handler, error_handler);    
    luau::debugger::Debugger debugger(true);
    pDebugger = &debugger;

    try
    {
        std::filesystem::path filePath = std::filesystem::path(argv[2]);
        // if (filePath.is_relative()) {
        //     filePath = std::filesystem::current_path() / filePath;
        // }

        // Initialize the engine
        Engine engine((Package()), filePath);
        engine.setCompilerCallback(compilerCallback);
        engine.setDebuggerNewLuauCallback(debuggerCallback);
        engine.initialize();

        debugger.initialize(engine.getMainState());
        debugger.listen(remoteDebuggerPort);

        engine.run();

        debugger.stop();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    pDebugger = nullptr;
    return 0;
}
