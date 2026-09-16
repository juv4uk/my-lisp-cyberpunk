// CP-PROBE-INPROCESS-V0 (#31): the minimal in-process bridge.
//
// Hard constraints from the issue, followed literally:
//   - no RED4ext/CET/Codeware runtime dependency (this IS the loader,
//     using the same version.dll proxy-load trick CET used, but our
//     own code, nothing borrowed);
//   - no evaluator, no Lisp policy, no scenario branching in this DLL
//     -- it emits one raw fact and stops;
//   - no raw game pointers cross the boundary -- the only fact this
//     first cut emits is about the bridge's OWN presence (module base,
//     process id), not anything read from game/engine state;
//   - no mutating game actions.
//
// Lesson already paid for in this repo's own history
// (docs/deep-penetration-roadmap-2026-09-10.md, problem 1: "RTTI call
// during Load"): DllMain must do nothing beyond spawning a thread.
// Anything heavier belongs on that thread, running after the loader
// has released its lock.
//
// Export forwarding: MSVC's linker does not resolve plain
// `Name=OtherDll.Name` .def entries as true PE export forwarders the
// way this was first attempted (LNK2001, unresolved external) --
// instead each real version.dll export is a thin function that loads
// the genuine system DLL (deployed alongside as "version-original.dll")
// once and tail-calls through a real function pointer with the exact
// signature from <winver.h>, so no game subsystem that legitimately
// needs real version.dll behavior can be broken by this proxy.

#include <windows.h>
#include <winver.h>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

HMODULE g_realVersionDll = nullptr;

HMODULE RealVersionDll()
{
    if (g_realVersionDll != nullptr)
    {
        return g_realVersionDll;
    }

    wchar_t modulePath[MAX_PATH] = {};
    HMODULE self = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&RealVersionDll),
        &self);
    GetModuleFileNameW(self, modulePath, MAX_PATH);

    std::wstring path(modulePath);
    const auto slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
    {
        path.resize(slash + 1);
    }
    path += L"version-original.dll";

    g_realVersionDll = LoadLibraryW(path.c_str());
    return g_realVersionDll;
}

template <typename Fn>
Fn RealProc(const char* name)
{
    HMODULE real = RealVersionDll();
    if (real == nullptr)
    {
        return nullptr;
    }
    return reinterpret_cast<Fn>(GetProcAddress(real, name));
}

std::wstring ObservationOutputPath()
{
    wchar_t modulePath[MAX_PATH] = {};
    HMODULE self = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&ObservationOutputPath),
        &self);
    GetModuleFileNameW(self, modulePath, MAX_PATH);

    std::wstring path(modulePath);
    const auto slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
    {
        path.resize(slash + 1);
    }
    path += L"bridge-observation.lisp";
    return path;
}

DWORD WINAPI BridgeThread(LPVOID)
{
    // Give the loader time to finish resolving imports for every DLL
    // in the load chain before we do anything, even file I/O.
    Sleep(2000);

    HMODULE self = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&BridgeThread),
        &self);

    const auto moduleBase = reinterpret_cast<uintptr_t>(self);
    const DWORD pid = GetCurrentProcessId();
    const long long timestamp = []() -> long long
    {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER uli{ft.dwLowDateTime, ft.dwHighDateTime};
        return static_cast<long long>((uli.QuadPart - 116444736000000000ULL) / 10000ULL);
    }();

    const std::wstring outPath = ObservationOutputPath();
    FILE* f = nullptr;
    if (_wfopen_s(&f, outPath.c_str(), L"w") == 0 && f != nullptr)
    {
        // game-observation/1 (docs/observation-contract-v0.md): this
        // first fact is deliberately about the bridge's own successful
        // load, not any game/engine state -- #31's evidence point 1
        // ("bridge реально завантажений лише нашим механізмом") is
        // exactly this record's entire content.
        fprintf(f,
                "(game-observation/1 (source in-process) (game-fingerprint \"unknown\") "
                "(fact bridge-alive) (value t) (timestamp %lld) (validity valid) "
                "(provenance ((channel-revision 1) (process-id %lu) (module-base \"0x%llX\"))))\n",
                timestamp, static_cast<unsigned long>(pid), static_cast<unsigned long long>(moduleBase));
        fclose(f);
    }

    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        HANDLE thread = CreateThread(nullptr, 0, BridgeThread, nullptr, 0, nullptr);
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
    }
    return TRUE;
}

// ---------------------------------------------------------------------
// Real version.dll export surface. Each is a thin forward to the real
// system DLL, using its actual <winver.h> signature -- never
// implemented here, only relayed.
// ---------------------------------------------------------------------

extern "C" {

BOOL WINAPI GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    using Fn = BOOL(WINAPI*)(LPCSTR, DWORD, DWORD, LPVOID);
    auto fn = RealProc<Fn>("GetFileVersionInfoA");
    return fn != nullptr ? fn(lptstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

BOOL WINAPI GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    using Fn = BOOL(WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID);
    auto fn = RealProc<Fn>("GetFileVersionInfoW");
    return fn != nullptr ? fn(lptstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

BOOL WINAPI GetFileVersionInfoExA(DWORD dwFlags, LPCSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    using Fn = BOOL(WINAPI*)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
    auto fn = RealProc<Fn>("GetFileVersionInfoExA");
    return fn != nullptr ? fn(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

BOOL WINAPI GetFileVersionInfoExW(DWORD dwFlags, LPCWSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    using Fn = BOOL(WINAPI*)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
    auto fn = RealProc<Fn>("GetFileVersionInfoExW");
    return fn != nullptr ? fn(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

DWORD WINAPI GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle)
{
    using Fn = DWORD(WINAPI*)(LPCSTR, LPDWORD);
    auto fn = RealProc<Fn>("GetFileVersionInfoSizeA");
    return fn != nullptr ? fn(lptstrFilename, lpdwHandle) : 0;
}

DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle)
{
    using Fn = DWORD(WINAPI*)(LPCWSTR, LPDWORD);
    auto fn = RealProc<Fn>("GetFileVersionInfoSizeW");
    return fn != nullptr ? fn(lptstrFilename, lpdwHandle) : 0;
}

DWORD WINAPI GetFileVersionInfoSizeExA(DWORD dwFlags, LPCSTR lpwstrFilename, LPDWORD lpdwHandle)
{
    using Fn = DWORD(WINAPI*)(DWORD, LPCSTR, LPDWORD);
    auto fn = RealProc<Fn>("GetFileVersionInfoSizeExA");
    return fn != nullptr ? fn(dwFlags, lpwstrFilename, lpdwHandle) : 0;
}

DWORD WINAPI GetFileVersionInfoSizeExW(DWORD dwFlags, LPCWSTR lpwstrFilename, LPDWORD lpdwHandle)
{
    using Fn = DWORD(WINAPI*)(DWORD, LPCWSTR, LPDWORD);
    auto fn = RealProc<Fn>("GetFileVersionInfoSizeExW");
    return fn != nullptr ? fn(dwFlags, lpwstrFilename, lpdwHandle) : 0;
}

BOOL WINAPI VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen)
{
    using Fn = BOOL(WINAPI*)(LPCVOID, LPCSTR, LPVOID*, PUINT);
    auto fn = RealProc<Fn>("VerQueryValueA");
    return fn != nullptr ? fn(pBlock, lpSubBlock, lplpBuffer, puLen) : FALSE;
}

BOOL WINAPI VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen)
{
    using Fn = BOOL(WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
    auto fn = RealProc<Fn>("VerQueryValueW");
    return fn != nullptr ? fn(pBlock, lpSubBlock, lplpBuffer, puLen) : FALSE;
}

DWORD WINAPI VerFindFileA(DWORD uFlags, LPCSTR szFileName, LPCSTR szWinDir, LPCSTR szAppDir,
                          LPSTR szCurDir, PUINT lpuCurDirLen, LPSTR szDestDir, PUINT lpuDestDirLen)
{
    using Fn = DWORD(WINAPI*)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT, LPSTR, PUINT);
    auto fn = RealProc<Fn>("VerFindFileA");
    return fn != nullptr ? fn(uFlags, szFileName, szWinDir, szAppDir, szCurDir, lpuCurDirLen, szDestDir, lpuDestDirLen) : 0;
}

DWORD WINAPI VerFindFileW(DWORD uFlags, LPCWSTR szFileName, LPCWSTR szWinDir, LPCWSTR szAppDir,
                          LPWSTR szCurDir, PUINT lpuCurDirLen, LPWSTR szDestDir, PUINT lpuDestDirLen)
{
    using Fn = DWORD(WINAPI*)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT, LPWSTR, PUINT);
    auto fn = RealProc<Fn>("VerFindFileW");
    return fn != nullptr ? fn(uFlags, szFileName, szWinDir, szAppDir, szCurDir, lpuCurDirLen, szDestDir, lpuDestDirLen) : 0;
}

DWORD WINAPI VerInstallFileA(DWORD uFlags, LPCSTR szSrcFileName, LPCSTR szDestFileName, LPCSTR szSrcDir,
                             LPCSTR szDestDir, LPCSTR szCurDir, LPSTR szTmpFile, PUINT lpuTmpFileLen)
{
    using Fn = DWORD(WINAPI*)(DWORD, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT);
    auto fn = RealProc<Fn>("VerInstallFileA");
    return fn != nullptr ? fn(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, lpuTmpFileLen) : 0;
}

DWORD WINAPI VerInstallFileW(DWORD uFlags, LPCWSTR szSrcFileName, LPCWSTR szDestFileName, LPCWSTR szSrcDir,
                             LPCWSTR szDestDir, LPCWSTR szCurDir, LPWSTR szTmpFile, PUINT lpuTmpFileLen)
{
    using Fn = DWORD(WINAPI*)(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT);
    auto fn = RealProc<Fn>("VerInstallFileW");
    return fn != nullptr ? fn(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, lpuTmpFileLen) : 0;
}

DWORD WINAPI VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD nSize)
{
    using Fn = DWORD(WINAPI*)(DWORD, LPSTR, DWORD);
    auto fn = RealProc<Fn>("VerLanguageNameA");
    return fn != nullptr ? fn(wLang, szLang, nSize) : 0;
}

DWORD WINAPI VerLanguageNameW(DWORD wLang, LPWSTR szLang, DWORD nSize)
{
    using Fn = DWORD(WINAPI*)(DWORD, LPWSTR, DWORD);
    auto fn = RealProc<Fn>("VerLanguageNameW");
    return fn != nullptr ? fn(wLang, szLang, nSize) : 0;
}

} // extern "C"
