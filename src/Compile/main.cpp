#include "LuauCompiler.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace fs = std::filesystem;

// A file's full path, as the file system spells it
static fs::path fullPath(const std::string& path) {
    std::error_code ec;
    fs::path absolute = fs::absolute(path, ec);
    if (ec) return fs::path(path);
    fs::path canonical = fs::weakly_canonical(absolute, ec);
    return ec ? absolute.lexically_normal() : canonical;
}

// The deepest folder holding every one of the files, none when they're on different drives
static std::optional<fs::path> commonFolder(const std::vector<fs::path>& files) {
    std::optional<fs::path> common;
    for (const fs::path& file : files) {
        fs::path folder = file.parent_path();
        if (!common) {
            common = folder;
            continue;
        }
        fs::path shared;
        auto a = common->begin();
        auto b = folder.begin();
        for (; a != common->end() && b != folder.end() && *a == *b; ++a, ++b) shared /= *a;
        if (shared.empty()) return std::nullopt;
        common = shared;
    }
    return common;
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
        std::cerr << "Usage: compile [-c <.config.luau>] [-s <main.luau> <script.luau | folder> ...] [-o <output.pkg>]\n";
        return 1;
    }

    if (args.scripts.empty()) {
        std::cerr << "Error: at least one main script is required.\n";
        std::cerr << "Usage: compile [-c <.config.luau>] [-s <main.luau> <script.luau | folder> ...] [-o <output.pkg>]\n";
        return 1;
    }

    if (!args.output) {
        std::cerr << "Error: an output file is required.\n";
        std::cerr << "Usage: compile [-c <.config.luau>] [-s <main.luau> <script.luau | folder> ...] [-o <output.pkg>]\n";
        return 1;
    }
    
    LuauCompiler compiler;

    // Each script by its full path, once however it was given, and the config last
    std::vector<fs::path> files;
    std::set<fs::path> added;

    for (const std::string& entry : args.scripts) {
        std::error_code ec;
        if (!fs::is_directory(entry, ec)) {
            fs::path file = fullPath(entry);
            if (added.insert(file).second) files.push_back(file);
            continue;
        }
        std::vector<std::string> found;
        for (const auto& file : fs::recursive_directory_iterator(entry, ec)) {
            if (!file.is_regular_file(ec)) continue;
            std::string extension = file.path().extension().string();
            if (extension == ".luau" || extension == ".lua") found.push_back(file.path().generic_string());
        }
        std::sort(found.begin(), found.end());
        for (const std::string& script : found) {
            fs::path file = fullPath(script);
            if (added.insert(file).second) files.push_back(file);
        }
    }

    size_t scriptCount = files.size();
    if (args.config.has_value()) files.push_back(fullPath(*args.config));

    std::optional<fs::path> root = commonFolder(files);
    if (!root) {
        std::cerr << "Error: the scripts and the config must be on the same drive.\n";
        return 1;
    }

    // Compile all files passed in on the command line through the luau compiler
    // and append them into the package in the order specified on the command line.
    for (size_t i = 0; i < files.size(); i++) {
        std::string name = files[i].lexically_relative(*root).generic_string();
        if (!compiler.compileScript(files[i].string(), name)) {
            bool isConfig = i >= scriptCount;
            std::cerr << (isConfig ? "Failed to compile config script: " : "Failed to compile script: ") << name << std::endl;
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
