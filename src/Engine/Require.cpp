#include "Require.h"
#include "Engine.h"

#include "lualib.h"
#include "lua.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <string.h>

#define CHUNK_PREFIX_LUWOW "Luwow"

// Checks if .luau extension exists
static bool hasLuauExtension(std::string path) {
    return (path.size() > 5 && path.substr(path.size() - 5) == ".luau");
}

// Checks if .lua extension exists
static bool hasLuaExtension(std::string path) {
    return (path.size() > 4 && path.substr(path.size() - 4) == ".lua");
}

// Adds .luau extension to path for init
static std::string addExtension(std::string path) {
    if (hasLuaExtension(path) | hasLuauExtension(path)) return path;
    return path + ".luau";
}

// Strips extension (.lua / .luau)
static std::string stripExtension(std::string path) {
    if (hasLuauExtension(path))
        path = path.substr(0, path.size() - 5);
    else if (hasLuaExtension(path))
        path = path.substr(0, path.size() - 4);
    return path;
}

// Formats path for execution
static std::string formatPath(std::string root, std::string path) {
    size_t pos = path.find(root);
    if (pos != std::string::npos) {
        path.erase(pos, root.length());
        path = "." + path;
    }
    return path;
}

namespace fs = std::filesystem;
namespace Luwow::Engine {
    RequireContext getRequireContext(lua_State* L) {
        RequireContext ctx;
        lua_Debug ar;
        std::string callerPath;

        ctx.root = fs::current_path().generic_string();

        for (int level = 1;; level++) {
            if (!lua_getinfo(L, level, "s", &ar)) break;
            if (!ar.source) continue;

            std::string source = formatPath(ctx.root, stripExtension(ar.source));
        
            // selfDir = directory portion of the key
            size_t lastSlash = source.rfind('/');
            ctx.selfDir = (lastSlash != std::string::npos) ? source.substr(0, lastSlash) : "";

            std::string stem = (lastSlash != std::string::npos) ? source.substr(lastSlash + 1) : source;
            ctx.isInit = (stem == "init");

            fs::path full = fs::path(ctx.root) / fs::path(ctx.selfDir).relative_path();
            ctx.callerDir = full.lexically_normal().generic_string();
        }

        return ctx;
    }

    static int requireNative(lua_State* L, const char* szLibrary) {
        luaL_error(L, "DLL modules are not supported in this build (all modules are statically linked).", "Tried to load: %s", szLibrary);
        return 0;
    }

    static std::optional<LocatedModule> resolveInternal(Engine* engine, std::string key) {
        std::string moduleName = engine->getModuleName(key);
        if (moduleName.empty()) return std::nullopt;
        return LocatedModule { .path = moduleName, .type = LocatedModule::TYPE_INTERNAL_MODULE};
    }

    static std::string resolvePath(lua_State* L, RequireContext& ctx, const std::string path) {
        fs::path fullPath = (fs::path(ctx.callerDir) / fs::path(path)).lexically_normal();

        if (fs::is_directory(fullPath)) {
            fs::path initFilePath = fs::path(fullPath.string() + "\\init.luau");
            if (!fs::exists(initFilePath)) {
                luaL_error(L, "Require path does not contain an init.luau file.");
                return std::string();
            }
            fullPath = initFilePath;
        }

        std::string resolvedPath = addExtension(fullPath.generic_string());
        return resolvedPath;
    }

    std::optional<LocatedModule> resolveModule(Engine* engine, lua_State* L, RequireContext& ctx, const std::string path) {
        // Alias Resolution
        if (path[0] == '@') {
            std::string alias, modulePath;

            size_t firstSlash = path.find('/', 1);
            if (firstSlash == std::string::npos) {
                alias = path.substr(1);
                modulePath = "";
            } else {
                alias = path.substr(1, firstSlash - 1);
                modulePath = path.substr(firstSlash + 1);
            }

            // Strip trailing slashes (e.g. "@self/" produces empty modulePath)
            while (!modulePath.empty() && modulePath.back() == '/') modulePath.pop_back();

            // Check for Luwow internal module aliases first.
            if (alias == CHUNK_PREFIX_LUWOW) {
                auto module = resolveInternal(engine, modulePath);
                if (module) {
                    return module;
                }
            } else if (alias == "self") {
                fs::path resolvedPath = fs::weakly_canonical(ctx.selfDir / fs::path(modulePath));
                std::string newPath = addExtension(resolvedPath.generic_string());
                std::string formattedPath = formatPath(ctx.root, newPath);

                if (engine->usesPackage()) {
                    return LocatedModule { .path = formattedPath, .type = LocatedModule::TYPE_PACKAGE };
                } else {
                    return LocatedModule { .path = newPath, .formattedPath = formattedPath, .type = LocatedModule::TYPE_FILE };
                }
            } else {
                luaL_error(L, "Require %s used undefined alias '@%s'", path.c_str(), alias.c_str());
            }
        } else if (!(path.starts_with("./") || path.starts_with("../"))) {
            luaL_error(L, "Require path must always start with @, ./ or ../");
        } else {
            // Try packages and files
            if (!ctx.callerDir.empty()) {
                std::string newPath = resolvePath(L, ctx, path);
                std::string formattedPath = formatPath(ctx.root, newPath);

                if (engine->usesPackage()) {
                    return LocatedModule { .path = formattedPath, .type = LocatedModule::TYPE_PACKAGE };
                } else {
                    return LocatedModule { .path = newPath, .formattedPath = formattedPath, .type = LocatedModule::TYPE_FILE };
                }
            }
            
            luaL_error(L, "Module %s not found", path.c_str());
        }

        return std::nullopt;
    }

    std::optional<LocatedModule> resolveModule(Engine* engine, lua_State* L, const std::string path) {
        RequireContext ctx = getRequireContext(L);
        return resolveModule(engine, L, ctx, path);
    }

    int luaRequire(Engine* engine, lua_State* L, std::string path_str) {
        auto resolved = resolveModule(engine, L, path_str);
        if (!resolved) {
            luaL_error(L, "Unable to locate %s", path_str.c_str());
        }

        int nret = 0;
        std::string chunkName;
        std::string formattedPath = (resolved->formattedPath.empty()) ? resolved->path : resolved->formattedPath;

        switch(resolved->type) {
            case LocatedModule::TYPE_FILE: {
                // Native modules silently shadow a lua script.
                // Windows: [module].dll, Linux: lib[module].so, macOS: lib[module].dylib
                fs::path basePath = fs::path(resolved->path);
                fs::path nativePath;

                #if defined(_WIN32)
                    nativePath = basePath.parent_path() / (basePath.stem().string() + ".dll");
                #elif defined(__APPLE__)
                    nativePath = basePath.parent_path() / ("lib" + basePath.stem().string() + ".dylib");
                #else
                    nativePath = basePath.parent_path() / ("lib" + basePath.stem().string() + ".so");
                #endif

                if (fs::exists(nativePath)) {
                    chunkName = nativePath.generic_string();
                    nret = requireNative(L, nativePath.string().c_str());
                } else {
                    if (!fs::exists(resolved->path)) {
                        nret = 0;
                        break;
                    }

                    // Check if the module is already loaded
                    nret = engine->getModuleRef(L, formattedPath);
                    if (nret > 0) break;

                    nret = engine->compileAndExecute(L, resolved->path, formattedPath);
                    chunkName = formattedPath;
                }
                break;
            }

            case LocatedModule::TYPE_INTERNAL_MODULE: {
                chunkName = CHUNK_PREFIX_LUWOW + formattedPath;
                return engine->setModuleRef(L, formattedPath);
            }

            case LocatedModule::TYPE_PACKAGE: {
                chunkName = formattedPath;
                nret = engine->getModuleRef(L, formattedPath);
                if (nret > 0) break;
                nret = engine->isInPackage(L, formattedPath);
                break;
            }

            default: {
                luaL_error(L, "Unsupported module type: %d", resolved->type);
            }
        }

        if (nret != 1) {
            luaL_error(L, "Module %s not found, or could not be executed", path_str.c_str());
        }

        return 1;
    }
} // namespace Luwow::Engine
