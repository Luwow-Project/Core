#pragma once

#include "ILuauModule.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Luwow::Engine {

// File suffix of module DLLs on this platform, CMake builds them with the same suffix
#if defined(_WIN32)
inline constexpr const char* ModuleLibrarySuffix = ".dll";
#elif defined(__APPLE__)
inline constexpr const char* ModuleLibrarySuffix = ".dylib";
#else
inline constexpr const char* ModuleLibrarySuffix = ".so";
#endif

class ModuleLoader {
public:
    // Loads a module DLL and returns its module. Each DLL is loaded once per process and
    // stays loaded until exit, so modules shared between hosts never outlive their code.
    static std::shared_ptr<ILuauModule> load(const std::filesystem::path& path, std::string& error);

    // Lists the module DLLs directly inside a directory, sorted by name
    static std::vector<std::filesystem::path> listDirectory(const std::filesystem::path& directory, std::string& error);
};

} // namespace Luwow::Engine
