// Main implementation from Bottersnike/eryx

#include "Config.h"
#include "Engine.h"

#include "lualib.h"
#include "lua.h"

#include "Luau/Config.h"
#include "Luau/LuauConfig.h"

#include <fstream>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <iostream>
#include <map>

#ifdef _WIN32
#define PATH_PRINTF "%ls"
#else
#define PATH_PRINTF "%s"
#endif

// #region: Code lifted from luau/Config/src/LuauConfig.cpp
#define RETURN_WITH_ERROR(msg)   \
    do {                         \
        if (error) *error = msg; \
        return std::nullopt;     \
    } while (false)
struct ThreadPopper {
    explicit ThreadPopper(lua_State* L) : L(L) { LUAU_ASSERT(L); }

    ThreadPopper(const ThreadPopper&) = delete;
    ThreadPopper& operator=(const ThreadPopper&) = delete;

    ~ThreadPopper() { lua_pop(L, 1); }

    lua_State* L = nullptr;
};

namespace fs = std::filesystem;
namespace Luwow::Engine {
    static std::optional<Luau::ConfigTable> serializeTable(lua_State* L, std::string* error) {
        ThreadPopper popper(L);  // Remove table from stack after processing
        Luau::ConfigTable table;

        lua_pushnil(L);

        while (lua_next(L, -2) != 0) {
            ThreadPopper popper(L);  // Remove value from stack after processing

            // Process key
            Luau::ConfigTableKey key;
            switch (lua_type(L, -2)) {
                case LUA_TNUMBER:
                    key = lua_tonumber(L, -2);
                    break;
                case LUA_TSTRING:
                    key = std::string{ lua_tostring(L, -2) };
                    break;
                default:
                    RETURN_WITH_ERROR("configuration table keys must be strings or numbers");
            }

            // Process value
            switch (lua_type(L, -1)) {
                case LUA_TNUMBER:
                    table[key] = lua_tonumber(L, -1);
                    break;
                case LUA_TSTRING:
                    table[key] = std::string{ lua_tostring(L, -1) };
                    break;
                case LUA_TBOOLEAN:
                    table[key] = static_cast<bool>(lua_toboolean(L, -1));
                    break;
                case LUA_TTABLE: {
                    lua_pushvalue(L, -1);  // Copy table for recursive call
                    if (std::optional<Luau::ConfigTable> nested = serializeTable(L, error))
                        table[key] = std::move(*nested);
                    else
                        return std::nullopt;  // Error already set in recursive call
                    break;
                }
                default:
                    std::string msg = "configuration value for key \"" + key.toString() +
                                      "\" must be a string, number, boolean, or nested table";
                    RETURN_WITH_ERROR(std::move(msg));
            }
        }

        return table;
    }

    static std::optional<std::string> createLuauConfigFromLuauTable(Luau::Config& config, const Luau::ConfigTable& luauTable, std::optional<Luau::ConfigOptions::AliasOptions> aliasOptions) {
        for (const auto& [k, v] : luauTable) {
            const std::string* key = k.get_if<std::string>();
            if (!key) return "configuration keys in \"luau\" table must be strings";

            if (*key == "languagemode") {
                const std::string* value = v.get_if<std::string>();
                if (!value) return "configuration value for key \"languagemode\" must be a string";

                if (std::optional<std::string> errorMessage = parseModeString(config.mode, *value))
                    return errorMessage;
            }

            if (*key == "lint") {
                const Luau::ConfigTable* lint = v.get_if<Luau::ConfigTable>();
                if (!lint) return "configuration value for key \"lint\" must be a table";

                // Handle wildcard first to ensure overrides work as expected.
                if (const Luau::ConfigValue* value = lint->find("*")) {
                    const bool* enabled = value->get_if<bool>();
                    if (!enabled) return "configuration values in \"lint\" table must be booleans";

                    if (std::optional<std::string> errorMessage = parseLintRuleString(
                            config.enabledLint, config.fatalLint, "*", *enabled ? "true" : "false"))
                        return errorMessage;
                }

                for (const auto& [k, v] : *lint) {
                    const std::string* warningName = k.get_if<std::string>();
                    if (!warningName) return "configuration keys in \"lint\" table must be strings";

                    if (*warningName == "*") continue;  // Handled above

                    const bool* enabled = v.get_if<bool>();
                    if (!enabled) return "configuration values in \"lint\" table must be booleans";

                    if (std::optional<std::string> errorMessage =
                            parseLintRuleString(config.enabledLint, config.fatalLint, *warningName,
                                                *enabled ? "true" : "false"))
                        return errorMessage;
                }
            }

            if (*key == "linterrors") {
                const bool* value = v.get_if<bool>();
                if (!value) return "configuration value for key \"linterrors\" must be a boolean";

                config.lintErrors = *value;
            }

            if (*key == "typeerrors") {
                const bool* value = v.get_if<bool>();
                if (!value) return "configuration value for key \"typeerrors\" must be a boolean";

                config.typeErrors = *value;
            }

            if (*key == "globals") {
                const Luau::ConfigTable* globalsTable = v.get_if<Luau::ConfigTable>();
                if (!globalsTable)
                    return "configuration value for key \"globals\" must be an array of strings";

                std::vector<std::string> globals;
                globals.resize(globalsTable->size());

                for (const auto& [k, v] : *globalsTable) {
                    const double* key = k.get_if<double>();
                    if (!key) return "configuration array \"globals\" must only have numeric keys";

                    const size_t index = static_cast<size_t>(*key);
                    if (index < 1 || globalsTable->size() < index)
                        return "configuration array \"globals\" contains invalid numeric key";

                    const std::string* global = v.get_if<std::string>();
                    if (!global) return "configuration value in \"globals\" table must be a string";

                    LUAU_ASSERT(0 <= index - 1 && index - 1 < globalsTable->size());
                    globals[index - 1] = *global;
                }

                config.globals = std::move(globals);
            }

            if (*key == "aliases") {
                const Luau::ConfigTable* aliases = v.get_if<Luau::ConfigTable>();
                if (!aliases) return "configuration value for key \"aliases\" must be a table";

                for (const auto& [k, v] : *aliases) {
                    const std::string* aliasKey = k.get_if<std::string>();
                    if (!aliasKey) return "configuration keys in \"aliases\" table must be strings";

                    const std::string* aliasValue = v.get_if<std::string>();
                    if (!aliasValue) return "configuration values in \"aliases\" table must be strings";

                    if (std::optional<std::string> errorMessage =
                            parseAlias(config, *aliasKey, *aliasValue, aliasOptions))
                        return errorMessage;
                }
            }
        }

        return std::nullopt;
    }
    // #endregion

    static void runLuauConfig(Engine* engine, lua_State* L, fs::path path, std::string source, std::string root, fs::path directory, Luau::Config& cfg) {
        int ok;
        if (!engine->usesPackage()) {
            fs::path fullPath = (fs::path(root) / path).lexically_normal();
            ok = engine->compileAndExecute(L, fullPath.generic_string(), path.generic_string(), true);
        } else {
            // Implement for packages later.
            return;
        }

        // Expect a table with an "aliases" sub-table
        if (!lua_istable(L, -1)) {
            luaL_error(L, PATH_PRINTF " must return a table", path.c_str());
        }

        std::string error;
        auto table = serializeTable(L, &error);

        if (!error.empty()) {
            luaL_error(L, "%s", error.c_str());
        }

        if (!table->contains("luau")) {
            return;
        }

        Luau::ConfigTable* luauTable = (*table)["luau"].get_if<Luau::ConfigTable>();
        if (!luauTable) {
            luaL_error(L, "configuration value for key \"luau\" must be a table");
        }

        auto aliasOptions = Luau::ConfigOptions::AliasOptions{
            .configLocation = directory.string(),
            .overwriteAliases = true,
        };

        auto maybeError = createLuauConfigFromLuauTable(cfg, *luauTable, aliasOptions);
        if (maybeError) {
            luaL_error(L, "%s", (*maybeError).c_str());
        }
    }

    std::optional<Config*> getConfig(Engine* engine, lua_State* L, std::string root) {
        auto out = new Config;
        out->found = false;

        Luau::Config cfg;
        fs::path configPath = engine->getConfigPath();
        if (configPath.empty()) return std::nullopt;

        std::ifstream f(configPath, std::ios::binary);
        if (!f) {
            luaL_error(L, "Failed to read " PATH_PRINTF, configPath.c_str());
            return std::nullopt;
        }

        std::string contents = std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        runLuauConfig(engine, L, configPath, contents, root, configPath.parent_path(), cfg);

        out->configDir = configPath.parent_path();
        out->enabledLints = cfg.enabledLint.warningMask;
        out->fatalLints = cfg.fatalLint.warningMask;
        out->lintErrors = cfg.lintErrors;
        out->typeErrors = cfg.typeErrors;
        out->globals = cfg.globals;
        out->found = true;

        for (auto& [key, info] : cfg.aliases) {
            // Resolve alias value relative to the config file's directory
            fs::path configDir{ std::string{ info.configLocation } };
            std::error_code ec;
            fs::path candidate = configDir / info.value;
            fs::path resolved = fs::weakly_canonical(candidate, ec);

            if (ec) {
                ec.clear();
                resolved = fs::absolute(candidate, ec);
                if (ec) resolved = candidate;
            }

            AliasInfo ainfo = {
                resolved.string(),
                std::string(info.configLocation),
                info.value,
            };
            out->aliases.insert_or_assign(key, ainfo);
        }

        return out;
    }
}