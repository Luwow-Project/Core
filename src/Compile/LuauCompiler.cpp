#include "LuauCompiler.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cctype>
#include <initializer_list>

using Package = Luwow::Engine::Package;

// Reads the whole file into contents, false when it can't be opened
static bool readFile(const std::string& path, std::string& contents)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return false;
    contents.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return true;
}

// Whether data begins with the given bytes, an image's signature
static bool startsWith(const std::string& data, std::initializer_list<unsigned char> bytes)
{
    if (data.size() < bytes.size()) return false;
    size_t i = 0;
    for (unsigned char byte : bytes) if (static_cast<unsigned char>(data[i++]) != byte) return false;
    return true;
}

void LuauCompiler::setOptions(const Luau::CompileOptions& options)
{
    this->options = options;
}

bool LuauCompiler::compileScript(const std::string& scriptPath, const std::string& name)
{
    std::string script;
    if (!readFile(scriptPath, script))
    {
        std::cerr << "Could not open script file: " << scriptPath << std::endl;
        return false;
    }

    // A script that doesn't compile gives bytecode starting with 0, then the error
    std::string bytecode = Luau::compile(script, options);
    if (bytecode.empty() || bytecode[0] == 0) {
        std::cerr << "Error: " << name << (bytecode.size() > 1 ? bytecode.c_str() + 1 : ": could not compile") << std::endl;
        return false;
    }

    package.addFile(name, bytecode);
    return true;
}

bool LuauCompiler::addAsset(const std::string& assetPath, const std::string& name)
{
    std::string asset;
    if (!readFile(assetPath, asset))
    {
        std::cerr << "Could not open asset file: " << assetPath << std::endl;
        return false;
    }

    std::string extension = std::filesystem::path(name).extension().string();
    for (char& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (extension == ".png" && !startsWith(asset, {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A})) {
        std::cerr << "Error: " << name << " isn't a PNG image" << std::endl;
        return false;
    }
    if ((extension == ".jpeg" || extension == ".jpg") && !startsWith(asset, {0xFF, 0xD8, 0xFF})) {
        std::cerr << "Error: " << name << " isn't a JPEG image" << std::endl;
        return false;
    }
    if (extension == ".ico" && !startsWith(asset, {0x00, 0x00, 0x01, 0x00})) {
        std::cerr << "Error: " << name << " isn't an ICO icon" << std::endl;
        return false;
    }

    package.addFile(name, asset);
    return true;
}

bool LuauCompiler::savePackage(const std::string& outputPath)
{
    return package.save(outputPath);
}
