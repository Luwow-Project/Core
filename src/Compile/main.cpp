#include "LuauCompiler.h"

#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

static void formatPath(std::string& scriptPath) {
    for (char& c : scriptPath) {
        if (c == '\\')
            c = '/';
    }
}

struct Args {
    std::optional<std::string> config;
    std::vector<std::string> scripts;
    std::optional<std::string> output;
    std::string error;
    bool ok = true;
};

static Args parseArgs(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" || arg == "--config") {
            if (i + 1 >= argc) { a.ok = false; a.error = "-c requires a value"; return a; }
            a.config = argv[++i];
        } else if (arg == "-s" || arg == "--scripts") {
            if (i + 1 >= argc) { a.ok = false; a.error = "-s requires at least one script"; return a; }
            // Collect following args until next flag or end
            for (int j = i + 1; j < argc; ++j) {
                std::string next = argv[j];
                if (!next.empty() && next[0] == '-') {
                    i = j - 1; // advance outer loop to before this flag
                    break;
                }
                a.scripts.push_back(next);
                i = j; // consume
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) { a.ok = false; a.error = "-o requires a value"; return a; }
            a.output = argv[++i];
        } else {
            a.ok = false;
            a.error = "Unexpected positional argument: " + arg;
            return a;
        }
    }
    return a;
}

int main(int argc, char* argv[]) {
    Args args = parseArgs(argc, argv);
    if (!args.ok) {
        std::cerr << "Error: " << args.error << "\n";
        std::cerr << "Usage: compile [-c <.config.luau>] [-s <script1.luau> <script2.luau> ...] [-o <output.pkg>]\n";
        return 1;
    }

    if (args.scripts.empty()) {
        std::cerr << "Error: at least one main script is required.\n";
        std::cerr << "Usage: compile [-c <.config.luau>] [-s <script1.luau> <script2.luau> ...] [-o <output.pkg>]\n";
        return 1;
    }

    if (!args.output) {
        std::cerr << "Error: an output file is required.\n";
        std::cerr << "Usage: compile [-c <.config.luau>] [-s <script1.luau> <script2.luau> ...] [-o <output.pkg>]\n";
        return 1;
    }
    
    LuauCompiler compiler;
    
    // Compile all files passed in on the command line through the luau compiler
    // and append them into the package in the order specified on the command line.
    for (auto &script : args.scripts) {
        formatPath(script);
        if (!compiler.compileScript(script)) {
            std::cerr << "Failed to compile script: " << script << std::endl;
            return 1;
        }
    }

    if (args.config.has_value()) {
        std::string config = *args.config;
        formatPath(config);

        if (!compiler.compileScript(config)) {
            std::cerr << "Failed to compile config script: " << config << std::endl;
            return 1;
        }
    }

    std::string outputPath = *args.output;
    if (!compiler.savePackage(outputPath))
    {
        std::cerr << "Failed to write package: " << outputPath << std::endl;
        return 1;
    }

    return 0;
}
