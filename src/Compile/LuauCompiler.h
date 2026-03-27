#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Package.h"

class LuauCompiler {
public:
    LuauCompiler() = default;
    ~LuauCompiler() = default;

    // Compiles a single Luau script file.
    // Returns true on success, false on failure.
    bool compileScript(const std::string& scriptPath);

    // Saves the compiled package to the given output path.
    // Returns true on success, false on failure.
    bool savePackage(const std::string& outputPath);

private:
    // Store the compiled package.
    Luwow::Engine::Package package;
};