// OS Specific methods to determine the path to the executable
#if defined(_WIN32)
#include <windows.h>
std::filesystem::path getExecutablePath() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return path;
}
#elif defined(__linux__)
#include <unistd.h>
std::filesystem::path getExecutablePath() {
    return std::filesystem::read_symlink("/proc/self/exe");
}
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
std::filesystem::path getExecutablePath() {
    char buf[PATH_MAX];
    uint32_t bufsize = PATH_MAX;
    if (_NSGetExecutablePath(buf, &bufsize) == 0) {
        std::string pathStr(buf);
        std::filesystem::path path(pathStr);
        return path;
    }
    throw new std::exception();
}
#else
#error "Unsupported platform for getting executable path."
#endif