#include "LuauCompiler.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

#include "ExecutablePath.h"

namespace fs = std::filesystem;

static const char* const Usage =
    "Usage: compile [-c <.config.luau>] [-s <main.luau> <script.luau | folder> ...] [-a <asset | folder> ...]\n"
    "               [-f <option=level> ...] [-o <output.pkg>] [-e <executable> [-r <runpackage>] [-w]]\n"
    "  -a  files (except .luau and .lua) stored as they are, or folders of them\n"
    "  -f  the Luau compiler's options: optimizationlevel, debuglevel and coveragelevel, each 0 to 2\n"
    "  -e  an executable that runs the package: the runner with the package after it\n"
    "  -r  the runner for -e, the runpackage beside compile by default\n"
    "  -w  for -e on Windows, opens no console\n";

#if defined(_WIN32)
static const char* const RunnerName = "runpackage.exe";
#else
static const char* const RunnerName = "runpackage";
#endif

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

static bool isScript(const fs::path& path) {
    return !Luwow::Engine::Package::isAssetName(path.generic_string());
}

static bool isAsset(const fs::path& path) {
    return Luwow::Engine::Package::isAssetName(path.generic_string());
}

// Adds the files an entry names, each by its full path and only once
static bool addEntry(const std::string& entry, bool (*matches)(const fs::path&), bool checkNamed,
                     std::vector<fs::path>& files, std::set<fs::path>& added) {
    std::error_code ec;
    if (!fs::is_directory(entry, ec)) {
        if (checkNamed && !matches(fs::path(entry))) return false;
        fs::path file = fullPath(entry);
        if (added.insert(file).second) files.push_back(file);
        return true;
    }
    std::vector<std::string> found;
    for (const auto& file : fs::recursive_directory_iterator(entry, ec)) {
        if (!file.is_regular_file(ec)) continue;
        if (matches(file.path())) found.push_back(file.path().generic_string());
    }
    std::sort(found.begin(), found.end());
    for (const std::string& path : found) {
        fs::path file = fullPath(path);
        if (added.insert(file).second) files.push_back(file);
    }
    return true;
}

// Sets the compiler's options
static std::optional<std::string> parseFlags(const std::vector<std::string>& flags, Luau::CompileOptions& options) {
    for (const std::string& flag : flags) {
        size_t equals = flag.find('=');
        if (equals == std::string::npos) return "-f takes option=level, got " + flag;
        std::string name = flag.substr(0, equals);
        std::string value = flag.substr(equals + 1);

        int* level = nullptr;
        if (name == "optimizationlevel") level = &options.optimizationLevel;
        else if (name == "debuglevel") level = &options.debugLevel;
        else if (name == "coveragelevel") level = &options.coverageLevel;
        else return "unknown compiler option " + name + ", expected optimizationlevel, debuglevel or coveragelevel";

        if (value.size() != 1 || value[0] < '0' || value[0] > '2') return name + " must be 0, 1 or 2, got " + value;
        *level = value[0] - '0';
    }
    return std::nullopt;
}

struct Args {
    std::optional<std::string> config;
    std::vector<std::string> scripts;
    std::vector<std::string> assets;
    std::vector<std::string> flags;
    std::optional<std::string> output;
    std::optional<std::string> executable;
    std::optional<std::string> runner;
    bool windowed = false;
    std::string error;
    bool ok = true;
};

// Collects the values after argv[i] until the next flag, leaving i on the last one taken
static void collectValues(int argc, char* argv[], int& i, std::vector<std::string>& values) {
    for (int j = i + 1; j < argc; ++j) {
        std::string next = argv[j];
        if (!next.empty() && next[0] == '-') break;
        values.push_back(next);
        i = j;
    }
}

static Args parseArgs(int argc, char* argv[]) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" || arg == "--config") {
            if (i + 1 >= argc) { a.ok = false; a.error = "-c requires a value"; return a; }
            a.config = argv[++i];
        } else if (arg == "-s" || arg == "--scripts") {
            if (i + 1 >= argc) { a.ok = false; a.error = "-s requires at least one script"; return a; }
            collectValues(argc, argv, i, a.scripts);
        } else if (arg == "-a" || arg == "--assets") {
            size_t before = a.assets.size();
            collectValues(argc, argv, i, a.assets);
            if (a.assets.size() == before) { a.ok = false; a.error = "-a requires at least one asset"; return a; }
        } else if (arg == "-f" || arg == "--flags") {
            size_t before = a.flags.size();
            collectValues(argc, argv, i, a.flags);
            if (a.flags.size() == before) { a.ok = false; a.error = "-f requires at least one option=level"; return a; }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) { a.ok = false; a.error = "-o requires a value"; return a; }
            a.output = argv[++i];
        } else if (arg == "-e" || arg == "--executable") {
            if (i + 1 >= argc) { a.ok = false; a.error = "-e requires a value"; return a; }
            a.executable = argv[++i];
        } else if (arg == "-r" || arg == "--runner") {
            if (i + 1 >= argc) { a.ok = false; a.error = "-r requires a value"; return a; }
            a.runner = argv[++i];
        } else if (arg == "-w" || arg == "--windowed") {
            a.windowed = true;
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
        std::cerr << "Error: " << args.error << "\n" << Usage;
        return 1;
    }

    if (args.scripts.empty()) {
        std::cerr << "Error: at least one main script is required.\n" << Usage;
        return 1;
    }

    if (!args.output && !args.executable) {
        std::cerr << "Error: an output package (-o) or executable (-e) is required.\n" << Usage;
        return 1;
    }

    if ((args.runner || args.windowed) && !args.executable) {
        std::cerr << "Error: -r and -w are for an executable (-e).\n" << Usage;
        return 1;
    }

    // The runner is checked before compiling, so a missing one fails fast
    fs::path runner;
    if (args.executable) {
        std::error_code ec;
        runner = args.runner ? fs::path(*args.runner) : Luwow::Engine::getExecutablePath().parent_path() / RunnerName;
        if (!fs::is_regular_file(runner, ec)) {
            std::cerr << "Error: there's no runner at " << runner.string() << (args.runner ? "" : ", give one with -r") << ".\n";
            return 1;
        }
    }

    Luau::CompileOptions options;
    if (std::optional<std::string> error = parseFlags(args.flags, options)) {
        std::cerr << "Error: " << *error << "\n" << Usage;
        return 1;
    }

    LuauCompiler compiler;
    compiler.setOptions(options);

    // Each file by its full path, once however it was given: the scripts, the assets, and the config last
    std::vector<fs::path> files;
    std::set<fs::path> added;

    for (const std::string& entry : args.scripts) addEntry(entry, isScript, false, files, added);
    size_t scriptCount = files.size();

    for (const std::string& entry : args.assets) {
        if (!addEntry(entry, isAsset, true, files, added)) {
            std::cerr << "Error: " << entry << " is a script, asset files are any other file.\n";
            return 1;
        }
    }
    size_t assetEnd = files.size();

    if (args.config.has_value()) files.push_back(fullPath(*args.config));

    std::optional<fs::path> root = commonFolder(files);
    if (!root) {
        std::cerr << "Error: the scripts, the assets and the config must be on the same drive.\n";
        return 1;
    }

    // Compile all files passed in on the command line through the luau compiler
    // and append them into the package in the order specified on the command line.
    // Assets are stored as they are.
    for (size_t i = 0; i < files.size(); i++) {
        std::string name = files[i].lexically_relative(*root).generic_string();
        if (i >= scriptCount && i < assetEnd) {
            if (!compiler.addAsset(files[i].string(), name)) {
                std::cerr << "Failed to add asset: " << name << std::endl;
                return 1;
            }
            continue;
        }
        if (!compiler.compileScript(files[i].string(), name)) {
            bool isConfig = i >= assetEnd;
            std::cerr << (isConfig ? "Failed to compile config script: " : "Failed to compile script: ") << name << std::endl;
            return 1;
        }
    }

    if (args.output) {
        std::string outputPath = *args.output;
        if (!compiler.savePackage(outputPath))
        {
            std::cerr << "Failed to write package: " << outputPath << " (it can't be written, or is over 65535 files or 4 GB)" << std::endl;
            return 1;
        }
    }

    if (args.executable) {
        std::string error;
        if (!compiler.saveExecutable(runner.string(), *args.executable, args.windowed, error)) {
            std::cerr << "Failed to write executable: " << error << std::endl;
            return 1;
        }
    }

    return 0;
}
