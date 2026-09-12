// dinput8.dll proxy, so this plugin can run WITHOUT the Ultimate ASI Loader.
// Tekken 8 imports DINPUT8.dll; a dinput8.dll in the game folder is loaded in
// preference to the system one, which gets our code into the process. The real
// C:\Windows\System32\dinput8.dll is loaded eagerly from DllMain
// (dinput8_proxy_load) and the six exports are resolved up-front into function
// pointers; the forwarders below only call the cached pointer.
//
// Lifted verbatim from tekken-fashion-hub, where it is proven. A PURE
// forwarder: it exists only to get loaded, and hooks nothing.
//
// Why dinput8 and not winmm: T8 rejects a foreign winmm.dll in the game folder
// (the launcher relaunches in a loop, appending "Polaris" each retry). dinput8
// is the proven fighting-game-mod slot.
//
// COLLISION: only one thing can own the name. The Ultimate ASI Loader,
// opendojo and Enable Debug all install AS dinput8.dll — if any of those is
// present, install the .asi build instead of this one.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>  // IUnknown / REFIID / REFCLSID

#pragma comment(linker, "/export:DirectInput8Create")
#pragma comment(linker, "/export:DllCanUnloadNow,PRIVATE")
#pragma comment(linker, "/export:DllGetClassObject,PRIVATE")
#pragma comment(linker, "/export:DllRegisterServer,PRIVATE")
#pragma comment(linker, "/export:DllUnregisterServer,PRIVATE")
#pragma comment(linker, "/export:GetdfDIJoystick")

namespace {
HMODULE g_real = nullptr;
HRESULT(WINAPI* p_DirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN) = nullptr;
HRESULT(WINAPI* p_DllCanUnloadNow)(void) = nullptr;
HRESULT(WINAPI* p_DllGetClassObject)(REFCLSID, REFIID, LPVOID*) = nullptr;
HRESULT(WINAPI* p_DllRegisterServer)(void) = nullptr;
HRESULT(WINAPI* p_DllUnregisterServer)(void) = nullptr;
void*(WINAPI* p_GetdfDIJoystick)(void) = nullptr;  // real returns LPCDIDATAFORMAT
}  // namespace

// Load the real dinput8.dll and resolve all six forwarded exports. Call once,
// from DllMain(DLL_PROCESS_ATTACH), before anything can invoke an export.
void dinput8_proxy_load() {
    if (g_real) return;
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 16) return;
    wcscat_s(path, MAX_PATH, L"\\dinput8.dll");
    g_real = LoadLibraryW(path);
    if (!g_real) return;

    p_DirectInput8Create =
        reinterpret_cast<decltype(p_DirectInput8Create)>(GetProcAddress(g_real, "DirectInput8Create"));
    p_DllCanUnloadNow =
        reinterpret_cast<decltype(p_DllCanUnloadNow)>(GetProcAddress(g_real, "DllCanUnloadNow"));
    p_DllGetClassObject =
        reinterpret_cast<decltype(p_DllGetClassObject)>(GetProcAddress(g_real, "DllGetClassObject"));
    p_DllRegisterServer =
        reinterpret_cast<decltype(p_DllRegisterServer)>(GetProcAddress(g_real, "DllRegisterServer"));
    p_DllUnregisterServer = reinterpret_cast<decltype(p_DllUnregisterServer)>(
        GetProcAddress(g_real, "DllUnregisterServer"));
    p_GetdfDIJoystick =
        reinterpret_cast<decltype(p_GetdfDIJoystick)>(GetProcAddress(g_real, "GetdfDIJoystick"));
}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE h, DWORD v, REFIID r, LPVOID* p,
                                             LPUNKNOWN u) {
    return p_DirectInput8Create ? p_DirectInput8Create(h, v, r, p, u) : E_FAIL;
}
extern "C" HRESULT WINAPI DllCanUnloadNow(void) {
    return p_DllCanUnloadNow ? p_DllCanUnloadNow() : S_OK;
}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID c, REFIID r, LPVOID* p) {
    return p_DllGetClassObject ? p_DllGetClassObject(c, r, p) : E_FAIL;
}
extern "C" HRESULT WINAPI DllRegisterServer(void) {
    return p_DllRegisterServer ? p_DllRegisterServer() : S_OK;
}
extern "C" HRESULT WINAPI DllUnregisterServer(void) {
    return p_DllUnregisterServer ? p_DllUnregisterServer() : S_OK;
}
extern "C" void* WINAPI GetdfDIJoystick(void) {
    return p_GetdfDIJoystick ? p_GetdfDIJoystick() : nullptr;
}
