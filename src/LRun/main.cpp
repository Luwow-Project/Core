#include <iostream>
#include <fstream>
#include <filesystem>
#include "Package.h"
#include "Engine.h"

using Engine = Luwow::Engine::Engine;
using Package = Luwow::Engine::Package;

#if defined(_WIN32)
#include <windows.h>
std::filesystem::path getExecutablePath() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return path;
}
#elif defined(__linux__)
#include <unistd.h>
std::filesystem::path getExecutablePath() {
    return std::filesystem::read_symlink("/proc/self/exe");
}
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
std::filesystem::path getExecutablePath() {
    char buf[PATH_MAX];
    uint32_t bufsize = PATH_MAX;
    if (_NSGetExecutablePath(buf, &bufsize) == 0) {
        std::string pathStr(buf);
        std::filesystem::path path(pathStr);
        return path;
    }
    throw new std::exception();
}
#else
#error "Unsupported platform for getting executable path."
#endif
int executeScript(const std::string& scriptPath);

int main(int argc, char* argv[]) {
    if (argc == 1) {
        std::filesystem::path exe_path = getExecutablePath();
        return executeScript(exe_path.string());
    }

    if (argc < 2) {
        std::cerr << "Usage: lrun <name.package>" << std::endl;
        return 1;
    }

    return executeScript(argv[1]);
}

int executeScript(const std::string& scriptPath) {
    std::cout << "Luau Runner - Execute Luau Packaged" << std::endl;

    Package package;
    if (!package.load(scriptPath)) {
        std::cerr << "Failed to load package: " << scriptPath << std::endl;
        return 1;
    }

    // Put your own binary modules here and DLLs here

    Engine engine(package, std::filesystem::path(scriptPath));
    engine.initialize();
    engine.run();

    return 0;
}
