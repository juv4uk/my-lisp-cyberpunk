// my-lisp-cyberpunk RED4ext host adapter.
//
// Purpose of THIS pass: prove wsm_my_lisp_cyberpunk_dll.dll loads inside the
// game process and host primitives work. RTTI/mutating capabilities later.
// Surface extensions (owner 2026-09-10): .my ↔ .мій for dispatch load.

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/ScriptGameInstance.hpp>

#include <windows.h>

#include "GameHandleTable.hpp"
#include "SurfaceExt.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

namespace
{
using Session = void;

constexpr uint64_t kWordNil = 1;
constexpr uint64_t kWordTrue = 2;

using WsmSessionInitFn = Session* (*)();
using WsmSessionFreeFn = void (*)(Session*);
using WsmHostPrimitiveFn = int32_t (*)(std::size_t argc, const uint64_t* argv, uint64_t* out);
using WsmRegisterPrimitiveFn = int32_t (*)(Session*, const char*, WsmHostPrimitiveFn);
using WsmBindFn = int32_t (*)(Session*, const char*, uint64_t);
using WsmEvalStringFn = char* (*)(Session*, const char*);
using WsmFreeStringFn = void (*)(char*);
using WsmWrapGameHandleFn = int32_t (*)(Session*, void*, uint64_t*);

HMODULE g_wsmModule = nullptr;
Session* g_wsmSession = nullptr;
RED4ext::v1::PluginHandle g_pluginHandle = nullptr;
const RED4ext::v1::Logger* g_logger = nullptr;
WsmEvalStringFn g_wsmEvalString = nullptr;
WsmFreeStringFn g_wsmFreeString = nullptr;
WsmBindFn g_wsmBind = nullptr;
WsmWrapGameHandleFn g_wsmWrapGameHandle = nullptr;
GameHandleTable g_gameHandles;
GameHandleTable::Token g_playerToken = 0;
std::string g_dispatchSource;

int32_t LogPrimitive(std::size_t argc, const uint64_t*, uint64_t* out)
{
    if (out == nullptr)
    {
        return 1;
    }
    if (argc != 0)
    {
        return 2;
    }
    if (g_logger == nullptr)
    {
        return 3;
    }

    static bool loggedOnce = false;
    if (!loggedOnce)
    {
        g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: Lisp host primitive запиши-лог invoked");
        loggedOnce = true;
    }
    *out = kWordNil;
    return 0;
}

int32_t PlayerPresentPrimitive(std::size_t argc, const uint64_t*, uint64_t* out)
{
    if (out == nullptr)
    {
        return 1;
    }
    if (argc != 0)
    {
        return 2;
    }
    if (g_logger == nullptr)
    {
        return 3;
    }

    RED4ext::ScriptGameInstance gameInstance;
    RED4ext::Handle<RED4ext::IScriptable> handle;
    bool executed = RED4ext::ExecuteGlobalFunction("GetPlayer;GameInstance", &handle, gameInstance);
    bool present = executed && static_cast<bool>(handle);

    if (present && g_playerToken == 0)
    {
        if (g_wsmSession == nullptr || g_wsmWrapGameHandle == nullptr || g_wsmBind == nullptr)
        {
            return 4;
        }

        const auto token = g_gameHandles.Retain(std::move(handle));
        if (token == 0)
        {
            return 5;
        }

        uint64_t playerWord = 0;
        if (g_wsmWrapGameHandle(g_wsmSession, GameHandleTable::ToOpaqueToken(token), &playerWord) != 0)
        {
            g_gameHandles.Release(token);
            return 6;
        }
        if (g_wsmBind(g_wsmSession, "гравець", playerWord) != 0)
        {
            g_gameHandles.Release(token);
            return 7;
        }

        g_playerToken = token;
        g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: retained player as opaque token=%zu",
                         static_cast<std::size_t>(token));
    }

    static int8_t loggedPresent = -1;
    const int8_t presentAsInt8 = present ? 1 : 0;
    if (presentAsInt8 != loggedPresent)
    {
        g_logger->InfoF(g_pluginHandle,
                         "my-lisp-cyberpunk: Lisp host primitive гравець-присутній? invoked, present=%s",
                         present ? "true" : "false");
        loggedPresent = presentAsInt8;
    }
    *out = present ? kWordTrue : kWordNil;
    return 0;
}

class WsmRuntimeGuard
{
public:
    WsmRuntimeGuard() = default;
    WsmRuntimeGuard(const WsmRuntimeGuard&) = delete;
    WsmRuntimeGuard& operator=(const WsmRuntimeGuard&) = delete;

    ~WsmRuntimeGuard()
    {
        if (m_released)
        {
            return;
        }
        if (m_session != nullptr && m_sessionFree != nullptr)
        {
            m_sessionFree(m_session);
        }
        if (m_module != nullptr)
        {
            FreeLibrary(m_module);
        }
    }

    void SetModule(HMODULE module)
    {
        m_module = module;
    }

    void SetSession(Session* session, WsmSessionFreeFn sessionFree)
    {
        m_session = session;
        m_sessionFree = sessionFree;
    }

    void Release()
    {
        m_released = true;
    }

private:
    HMODULE m_module = nullptr;
    Session* m_session = nullptr;
    WsmSessionFreeFn m_sessionFree = nullptr;
    bool m_released = false;
};

std::string g_lastDispatchResult;

bool DispatchRunningTick(RED4ext::CGameApplication*)
{
    if (g_wsmSession == nullptr || g_wsmEvalString == nullptr || g_wsmFreeString == nullptr ||
        g_logger == nullptr || g_dispatchSource.empty())
    {
        return true;
    }

    char* result = g_wsmEvalString(g_wsmSession, g_dispatchSource.c_str());
    if (result == nullptr)
    {
        g_logger->ErrorF(g_pluginHandle, "my-lisp-cyberpunk: fixed Lisp dispatch returned null");
        return false;
    }

    if (g_lastDispatchResult != result)
    {
        g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: Lisp dispatch => %s", result);
        g_lastDispatchResult = result;
    }
    g_wsmFreeString(result);
    return false;
}

std::wstring GetOwnDirectory()
{
    wchar_t path[MAX_PATH] = {};
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetOwnDirectory),
            &self))
    {
        return L"";
    }
    DWORD length = GetModuleFileNameW(self, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        return L"";
    }
    std::wstring full(path, length);
    auto slash = full.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"" : full.substr(0, slash + 1);
}

bool LoadDispatchSource(std::string& out)
{
    // Equal surface spellings: prefer product Cyrillic диспетчер.мій, fall back to .my twins.
    const std::filesystem::path scriptsDir =
        std::filesystem::path(GetOwnDirectory()) / L"scripts";
    std::filesystem::path used;
    if (!surface_ext::LoadFirstExisting(surface_ext::DispatchCandidates(scriptsDir), out, &used))
    {
        return false;
    }
    if (g_logger != nullptr)
    {
        // Log which spelling was found (UTF-8 path via u8string where available).
        g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: loaded dispatch source from scripts/");
    }
    return true;
}
} // namespace

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle aHandle, RED4ext::v1::EMainReason aReason,
                                         const RED4ext::v1::Sdk* aSdk)
{
    switch (aReason)
    {
    case RED4ext::v1::EMainReason::Load:
    {
        auto* logger = aSdk->logger;
        g_pluginHandle = aHandle;
        g_logger = logger;

        WsmRuntimeGuard guard;

        std::wstring dllPath = GetOwnDirectory() + L"wsm_my_lisp_cyberpunk_dll.dll";
        HMODULE wsmModule = LoadLibraryW(dllPath.c_str());
        if (wsmModule == nullptr)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: LoadLibraryW failed, GetLastError=%lu",
                            GetLastError());
            return false;
        }
        guard.SetModule(wsmModule);

        auto sessionInit =
            reinterpret_cast<WsmSessionInitFn>(GetProcAddress(wsmModule, "wsm_session_init"));
        if (sessionInit == nullptr)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: GetProcAddress(wsm_session_init) failed");
            return false;
        }

        Session* session = sessionInit();
        if (session == nullptr)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: wsm_session_init returned null");
            return false;
        }

        auto sessionFree =
            reinterpret_cast<WsmSessionFreeFn>(GetProcAddress(wsmModule, "wsm_session_free"));
        guard.SetSession(session, sessionFree);

        auto registerPrimitive =
            reinterpret_cast<WsmRegisterPrimitiveFn>(GetProcAddress(wsmModule, "wsm_register_primitive"));
        auto bind = reinterpret_cast<WsmBindFn>(GetProcAddress(wsmModule, "wsm_bind"));
        auto evalString = reinterpret_cast<WsmEvalStringFn>(GetProcAddress(wsmModule, "wsm_eval_string"));
        auto freeString = reinterpret_cast<WsmFreeStringFn>(GetProcAddress(wsmModule, "wsm_free_string"));
        auto wrapGameHandle =
            reinterpret_cast<WsmWrapGameHandleFn>(GetProcAddress(wsmModule, "wsm_wrap_game_handle"));
        if (registerPrimitive == nullptr || bind == nullptr || evalString == nullptr || freeString == nullptr ||
            wrapGameHandle == nullptr || sessionFree == nullptr)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: required WSM host ABI export is missing");
            return false;
        }

        if (registerPrimitive(session, "запиши-лог", &LogPrimitive) != 0)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: could not register запиши-лог");
            return false;
        }

        if (registerPrimitive(session, "гравець-присутній?", &PlayerPresentPrimitive) != 0)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: could not register гравець-присутній?");
            return false;
        }

        char* result = evalString(session, "(quote ())");
        if (result == nullptr)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: wsm_eval_string returned null");
            return false;
        }

        const bool matchesOracle = std::strcmp(result, "()") == 0;
        if (!matchesOracle)
        {
            logger->ErrorF(aHandle,
                            "my-lisp-cyberpunk: (quote ()) => %s, expected () -- vertical slice oracle "
                            "mismatch, failing load",
                            result);
            freeString(result);
            return false;
        }

        logger->InfoF(aHandle, "my-lisp-cyberpunk: (quote ()) => %s", result);
        freeString(result);

        if (!LoadDispatchSource(g_dispatchSource))
        {
            logger->ErrorF(aHandle,
                            "my-lisp-cyberpunk: could not load scripts/диспетчер.мій (or .my twin)");
            return false;
        }

        RED4ext::v1::GameState runningState{};
        runningState.OnEnter = nullptr;
        runningState.OnUpdate = &DispatchRunningTick;
        runningState.OnExit = nullptr;
        aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &runningState);

        g_wsmModule = wsmModule;
        g_wsmSession = session;
        g_wsmEvalString = evalString;
        g_wsmFreeString = freeString;
        g_wsmBind = bind;
        g_wsmWrapGameHandle = wrapGameHandle;
        guard.Release();

        logger->InfoF(aHandle, "my-lisp-cyberpunk: wsm_my_lisp_cyberpunk_dll.dll loaded, session ready");
        return true;
    }
    case RED4ext::v1::EMainReason::Unload:
    {
        g_dispatchSource.clear();
        g_lastDispatchResult.clear();
        g_playerToken = 0;
        g_gameHandles.Clear();
        g_wsmEvalString = nullptr;
        g_wsmFreeString = nullptr;
        g_wsmBind = nullptr;
        g_wsmWrapGameHandle = nullptr;
        if (g_wsmSession != nullptr && g_wsmModule != nullptr)
        {
            auto sessionFree =
                reinterpret_cast<WsmSessionFreeFn>(GetProcAddress(g_wsmModule, "wsm_session_free"));
            if (sessionFree != nullptr)
            {
                sessionFree(g_wsmSession);
            }
            g_wsmSession = nullptr;
        }
        if (g_wsmModule != nullptr)
        {
            FreeLibrary(g_wsmModule);
            g_wsmModule = nullptr;
        }
        g_logger = nullptr;
        g_pluginHandle = nullptr;
        return true;
    }
    default:
        return true;
    }
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* aInfo)
{
    aInfo->name = L"my-lisp-cyberpunk";
    aInfo->author = L"juv4uk";
    aInfo->version = RED4EXT_V1_SEMVER(0, 1, 0);
    // RUNTIME_VERSION_LATEST, not RUNTIME_VERSION_INDEPENDENT: since
    // гравець-присутній? started calling RED4ext::ExecuteGlobalFunction
    // against real game RTTI (GetPlayer;GameInstance, PlayerPuppet), this
    // adapter is no longer merely a passive logger of the loading
    // lifecycle -- INDEPENDENT would misrepresent that to RED4ext's own
    // version-compatibility check.
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
