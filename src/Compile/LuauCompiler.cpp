#include "LuauCompiler.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdint>

using Package = Luwow::Engine::Package;

// Reads the whole file into contents, false when it can't be opened
static bool readFile(const std::string& path, std::string& contents)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return false;
    contents.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
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

    package.addFile(name, asset);
    return true;
}

bool LuauCompiler::savePackage(const std::string& outputPath)
{
    return package.save(outputPath);
}

static bool markWindowed(std::string& executable, std::string& error)
{
    auto read16 = [&](size_t at) { return static_cast<uint16_t>(static_cast<uint8_t>(executable[at]) | (static_cast<uint8_t>(executable[at + 1]) << 8)); };
    if (executable.size() < 0x40 || executable[0] != 'M' || executable[1] != 'Z') {
        error = "only a Windows runner can be made windowed";
        return false;
    }
    size_t header = static_cast<size_t>(read16(0x3C)) | (static_cast<size_t>(read16(0x3E)) << 16);
    size_t subsystem = header + 24 + 68;
    if (subsystem + 2 > executable.size() || executable.compare(header, 4, std::string("PE\0\0", 4)) != 0) {
        error = "the runner has no PE header";
        return false;
    }
    uint16_t current = read16(subsystem);
    if (current != 2 && current != 3) {
        error = "the runner's subsystem is " + std::to_string(current) + ", not the console's or Windows GUI's";
        return false;
    }
    executable[subsystem] = 2;
    executable[subsystem + 1] = 0;
    return true;
}

bool LuauCompiler::saveExecutable(const std::string& runnerPath, const std::string& outputPath, bool windowed, std::string& error)
{
    std::error_code ec;
    if (std::filesystem::equivalent(runnerPath, outputPath, ec)) {
        error = "the executable would replace its runner";
        return false;
    }

    std::string runner;
    if (!readFile(runnerPath, runner)) {
        error = "could not open the runner " + runnerPath;
        return false;
    }
    if (windowed && !markWindowed(runner, error)) return false;

    std::ofstream out(outputPath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "could not write " + outputPath;
        return false;
    }
    out.write(runner.data(), static_cast<std::streamsize>(runner.size()));
    if (!package.write(out)) {
        error = "could not write the package into " + outputPath + " (it may be over 65535 files or 4 GB)";
        return false;
    }
    out.close();
    if (out.fail()) {
        error = "could not write " + outputPath;
        return false;
    }

#if !defined(_WIN32)
    std::filesystem::permissions(outputPath,
        std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec,
        std::filesystem::perm_options::add, ec);
    if (ec) {
        error = "could not make " + outputPath + " executable: " + ec.message();
        return false;
    }
#endif
    return true;
}
