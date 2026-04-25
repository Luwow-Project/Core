#include "LuauCompiler.h"
#include <iostream>

void replaceCharacters(std::string& scriptPath) {
    for (char& c : scriptPath) {
        if (c == '\\')
            c = '/';
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        // TODO: Make arguments so people don't accidentally write over their last luau script
        std::cerr << "Usage: compile <script.luau> <script2.luau> <script3.luau> ... <output.package>" << std::endl;
        return 1;
    }
    
    LuauCompiler compiler;
    
    // Compile all files passed in on the command line through the luau compiler
    // and append them into the package in the order specified on the command line.
    for (int i = 1; i < argc - 1; i++) {
        std::string scriptPath = argv[i];
        replaceCharacters(scriptPath);
        if (!compiler.compileScript(scriptPath)) {
            std::cerr << "Failed to compile script: " << scriptPath << std::endl;
            return 1;
        }
    }

    std::string outputPath = argv[argc - 1];
    if (!compiler.savePackage(outputPath))
    {
        std::cerr << "Failed to write package: " << outputPath << std::endl;
        return 1;
    }

    return 0;
}
