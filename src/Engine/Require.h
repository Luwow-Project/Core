#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ILuauModule.h"

namespace Luwow::Engine {
    class Engine;

    struct LocatedModule {
        std::string path;
        std::string formattedPath;
        enum {
            TYPE_FILE,
            TYPE_PACKAGE,
            TYPE_NATIVE_MODULE
        } type;
    };

    struct RequireContext {
        std::string callerDir;
        std::string selfDir;
        std::string root;
        bool isInit = false;
    };

    // The context from the working directory, as the file system spells it
    RequireContext getRequireContext(lua_State* L);
    // The context from a given root, such as a package's
    RequireContext getRequireContext(lua_State* L, const std::string& root);

    std::optional<LocatedModule> resolveModule(Engine* engine, lua_State* L, RequireContext& ctx, const std::string path);
    std::optional<LocatedModule> resolveModule(Engine* engine, lua_State* L, const std::string path);

    int luaRequire(Engine* engine, lua_State* L, std::string path_str);
} // namespace Luwow::Engine