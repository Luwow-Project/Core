#include "LuauCompiler.h"
#include "luacode.h"
#include <fstream>
#include <iostream>

using Package = Luwow::Engine::Package;

bool LuauCompiler::compileScript(const std::string& scriptPath)
{
    std::ifstream file(scriptPath, std::ios::in | std::ios::binary);
    if (!file)
    {
        std::cerr << "Could not open script file: " << scriptPath << std::endl;
        return false;
    }

    std::string script(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    file.close();

    size_t bytecodeSize = 0;
    std::unique_ptr<char, void (*)(void*)> bytecode(
        luau_compile(script.c_str(), script.length(), nullptr, &bytecodeSize), free);
    if (!bytecode) {
        std::cerr << "Failed to compile script" << scriptPath << std::endl;
        std::cerr << "Error: " << bytecode.get() << std::endl;
        return false;
    }

    package.addFile(scriptPath, std::string(bytecode.get(), bytecodeSize));
    return true;
}

bool LuauCompiler::savePackage(const std::string& outputPath)
{
    return package.save(outputPath);
}
