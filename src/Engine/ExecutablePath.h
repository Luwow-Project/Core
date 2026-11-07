#pragma once

// The running executable's path

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif !defined(__linux__)
#error "Unsupported platform for getting executable path."
#endif

namespace Luwow::Engine {
#if defined(_WIN32)
    // Wide, and grown until it fits, so paths past MAX_PATH or outside the ANSI code page come through whole
    inline std::filesystem::path getExecutablePath() {
        std::wstring path(MAX_PATH, L'\0');
        while (true) {
            DWORD length = GetModuleFileNameW(NULL, path.data(), static_cast<DWORD>(path.size()));
            if (length == 0) throw std::runtime_error("could not get the executable's path");
            if (length < path.size()) {
                path.resize(length);
                return std::filesystem::path(path);
            }
            path.resize(path.size() * 2);
        }
    }
#elif defined(__linux__)
    inline std::filesystem::path getExecutablePath() {
        return std::filesystem::read_symlink("/proc/self/exe");
    }
#elif defined(__APPLE__)
    inline std::filesystem::path getExecutablePath() {
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::vector<char> buffer(size + 1, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) throw std::runtime_error("could not get the executable's path");
        return std::filesystem::path(buffer.data());
    }
#endif
} // namespace Luwow::Engine
