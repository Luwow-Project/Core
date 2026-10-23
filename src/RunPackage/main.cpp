#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

#include "Package.h"
#include "Engine.h"
#include "StaticModules.h"
#include "getExecutablePath.h"

using Engine = Luwow::Engine::Engine;
using Package = Luwow::Engine::Package;

static std::filesystem::path getPackageLastScript(Package* package) {
    int fileCount = package->getFileCount();
    std::string lastScriptStr = package->getFileName(fileCount - 1);
    std::filesystem::path scriptPath(lastScriptStr);
    return scriptPath;
}

static bool isConfigPath(std::filesystem::path path) {
    std::string fileName = path.filename().string();

    auto pos = fileName.find('.');
    std::string configExtension = (pos == std::string::npos) ? std::string() : fileName.substr(pos);

    return configExtension == ".config.luau";
}

int main(int argc, char* argv[]) {
    std::filesystem::path exe_path = getExecutablePath();
    std::filesystem::path packagePath = exe_path;
    Package package;
    if (argc == 1) {
        if (!package.load(exe_path.string())) {
            std::cerr << "Usage: runpackage <name.package>" << std::endl;
            return 1;
        }
    }
    else
    {
        packagePath = argv[1];
        if (!package.load(packagePath.string())) {
            std::cerr << "Failed to load package: " << packagePath << std::endl;
            return 1;
        }
    }

    // Get the path of the last script saved in package
    std::filesystem::path scriptPath = getPackageLastScript(&package);

    // Register the libraries bound into this executable
    Luwow::registerStaticModules();

    Engine engine(package, packagePath);

    #ifdef LUWOW_MODULE_HOST
        // This executable exports the Luau API, so it can load the module DLLs the package's config lists.
        // Relative paths are found next to the package.
        engine.setDynamicModulesEnabled(true);
    #endif

    // The config is set before initializing so its DLLs are loaded before the script runs
    if (isConfigPath(scriptPath)) engine.setConfigPath(scriptPath);
    engine.initialize(argc, argv);
    engine.run();

    return 0;
}
