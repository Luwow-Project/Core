#if defined(_MSC_VER)

#include <windows.h>
#include <delayimp.h>

#include <cstring>

#ifndef LUWOW_HOST_IMPORT
#error "LUWOW_HOST_IMPORT must name the host executable the module imports from"
#endif

static FARPROC WINAPI luwowModuleHostHook(unsigned notification, PDelayLoadInfo info) {
    if (notification == dliNotePreLoadLibrary && info && info->szDll && _stricmp(info->szDll, LUWOW_HOST_IMPORT) == 0) {
        return reinterpret_cast<FARPROC>(GetModuleHandleW(nullptr));
    }
    return nullptr;
}

extern "C" const PfnDliHook __pfnDliNotifyHook2 = luwowModuleHostHook;

#endif
