#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Package.h"
#include "Luau/Compiler.h"

class LuauCompiler {
public:
    LuauCompiler() = default;
    ~LuauCompiler() = default;

    // The options every script after this is compiled with.
    void setOptions(const Luau::CompileOptions& options);

    // Compiles a single Luau script file, stored in the package under name.
    // Returns true on success, false on failure.
    bool compileScript(const std::string& scriptPath, const std::string& name);

    // Stores a file in the package as it is, under name.
    // Returns true on success, false on failure.
    bool addAsset(const std::string& assetPath, const std::string& name);

    // Saves the compiled package to the given output path.
    // Returns true on success, false on failure.
    bool savePackage(const std::string& outputPath);

private:
    // Store the compiled package.
    Luwow::Engine::Package package;
    Luau::CompileOptions options;
};