#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include "luacode.h"
#include "Engine.h"
#include "StaticModules.h"

using Engine = Luwow::Engine::Engine;
using Package = Luwow::Engine::Package;
using Message = Luwow::Engine::Message;

// Handles compiler-compile: compiles the script at message.data and replies with its bytecode
void compileRequest(void* context, Message& message) {
    std::filesystem::path modulePath(message.data);
    std::ifstream file(modulePath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open script file: " + modulePath.string());
    }
    std::string script(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    file.close();

    size_t bytecodeSize = 0;
    char* bytecode = luau_compile(script.c_str(), script.length(), nullptr, &bytecodeSize);
    if (!bytecode) {
        throw std::runtime_error("Failed to compile script: " + modulePath.string());
    }
    message.data = std::string(bytecode, bytecodeSize);
    free(bytecode);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: runscript <script.luau> <config.luau>" << std::endl;
        return 1;
    }

    // Register the libraries bound into this executable
    Luwow::registerStaticModules();

    Engine engine((Package()), std::filesystem::path(argv[1]));
    engine.handle(Luwow::Engine::Topics::CompilerCompile, compileRequest, nullptr);

    #ifdef LUWOW_MODULE_HOST
        // This executable exports the Luau API, so it can load the module DLLs listed in the config
        engine.setDynamicModulesEnabled(true);
    #endif

    // The config is set before initializing so its DLLs are loaded before the script runs
    if (argc > 2) engine.setConfigPath(argv[2]);
    engine.initialize(argc, argv);
    engine.run();

    return 0;
}
