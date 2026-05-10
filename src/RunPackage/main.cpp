#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

#include "Package.h"
#include "Engine.h"
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
 
    // Put your own binary modules here and DLLs here

    Engine engine(package, packagePath);
    engine.initialize(argc, argv);
    if (isConfigPath(scriptPath)) engine.setConfigPath(scriptPath);
    engine.run();

    return 0;
}
