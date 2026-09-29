#include "ModuleLoader.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace Luwow::Engine {

using ModuleAbiFn = int (*)();
using CreateModuleFn = ILuauModule* (*)();

struct LoadedLibrary {
    void* handle = nullptr;
    std::shared_ptr<ILuauModule> module;
};

static std::mutex loaderMutex;
static std::unordered_map<std::string, LoadedLibrary> loadedLibraries;

#if defined(_WIN32)
static std::string lastSystemError() {
    DWORD code = GetLastError();
    char* buffer = nullptr;
    DWORD length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<char*>(&buffer), 0, nullptr
    );
    std::string message = length ? std::string(buffer, length) : "error code " + std::to_string(code);
    LocalFree(buffer);

    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) message.pop_back();
    return message;
}

static void* openLibrary(const fs::path& path, std::string& error) {
    // Search the DLL's own directory for its dependencies
    HMODULE handle = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!handle) error = lastSystemError();
    return handle;
}

static void* findSymbol(void* handle, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
}

static void closeLibrary(void* handle) {
    FreeLibrary(static_cast<HMODULE>(handle));
}
#else
static void* openLibrary(const fs::path& path, std::string& error) {
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) error = dlerror();
    return handle;
}

static void* findSymbol(void* handle, const char* name) {
    return dlsym(handle, name);
}

static void closeLibrary(void* handle) {
    dlclose(handle);
}
#endif

std::shared_ptr<ILuauModule> ModuleLoader::load(const fs::path& path, std::string& error) {
    std::error_code ec;
    fs::path fullPath = fs::weakly_canonical(path, ec);
    if (ec || !fs::is_regular_file(fullPath)) {
        error = "file not found";
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(loaderMutex);

    std::string key = fullPath.generic_string();
    auto loaded = loadedLibraries.find(key);
    if (loaded != loadedLibraries.end()) return loaded->second.module;

    void* handle = openLibrary(fullPath, error);
    if (!handle) return nullptr;

    auto moduleAbi = reinterpret_cast<ModuleAbiFn>(findSymbol(handle, LUWOW_MODULE_ABI_SYMBOL));
    auto createModule = reinterpret_cast<CreateModuleFn>(findSymbol(handle, LUWOW_CREATE_MODULE_SYMBOL));
    if (!moduleAbi || !createModule) {
        closeLibrary(handle);
        error = "not a Luwow module, missing " LUWOW_CREATE_MODULE_SYMBOL;
        return nullptr;
    }

    int abi = moduleAbi();
    if (abi != LUWOW_MODULE_ABI_VERSION) {
        closeLibrary(handle);
        error = "built for module ABI " + std::to_string(abi) + ", expected " + std::to_string(LUWOW_MODULE_ABI_VERSION);
        return nullptr;
    }

    std::shared_ptr<ILuauModule> module(createModule());
    if (!module) {
        closeLibrary(handle);
        error = "module creation failed";
        return nullptr;
    }

    loadedLibraries[key] = LoadedLibrary{ handle, module };
    return module;
}

std::vector<fs::path> ModuleLoader::listDirectory(const fs::path& directory, std::string& error) {
    std::vector<fs::path> libraries;
    std::error_code ec;

    fs::directory_iterator it(directory, ec);
    if (ec) {
        error = "could not list module directory '" + directory.string() + "': " + ec.message();
        return libraries;
    }

    for (; it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) break;
        const fs::path& path = it->path();
        if (it->is_regular_file(ec) && path.extension() == ModuleLibrarySuffix) {
            libraries.push_back(path);
        }
    }
    if (ec) {
        error = "could not list module directory '" + directory.string() + "': " + ec.message();
    }

    std::sort(libraries.begin(), libraries.end());
    return libraries;
}

} // namespace Luwow::Engine
