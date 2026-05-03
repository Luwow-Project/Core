#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <map>

#include "ILuauModule.h"
#include "Engine.h"

namespace Luwow::Engine {
    struct AliasInfo {
        std::string qualified;
        std::string configPath;
        std::string path;
    };

    struct Config {
        std::filesystem::path configDir;  // Directory where the config was found

        uint64_t enabledLints;
        uint64_t fatalLints;
        bool lintErrors;
        bool typeErrors;

        std::vector<std::string> globals;
        std::map<std::string, AliasInfo> aliases;  // alias name -> resolved path

        bool found = false;
    };

    std::optional<Config*> getConfig(Engine* engine, lua_State* L, std::string callerDir);
}

