#include <iostream>
#include <fstream>
#include <filesystem>
#include "Package.h"
#include "Engine.h"
#include "getExecutablePath.h"

using Engine = Luwow::Engine::Engine;
using Package = Luwow::Engine::Package;

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
 
    // Put your own binary modules here and DLLs here

    Engine engine(package, packagePath);
    engine.initialize();
    engine.run();

    return 0;
}
