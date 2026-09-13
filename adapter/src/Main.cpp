// my-lisp-cyberpunk RED4ext host adapter.
//
// Purpose of THIS pass: prove wsm_my_lisp_cyberpunk_dll.dll loads inside the
// game process and host primitives work. RTTI/mutating capabilities later.
// `.lisp` is canonical; legacy extensions are compatibility-only for dispatch load.

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/ScriptGameInstance.hpp>

#include <windows.h>

#include "generated/host_operations.generated.hpp"

#include "GameHandleTable.hpp"
#include "PlayerHandleEpoch.hpp"
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

using WsmSessionInitFn = Session* (*)();
using WsmSessionFreeFn = void (*)(Session*);
using WsmHostPrimitiveFn = int32_t (*)(std::size_t argc, const uint64_t* argv, uint64_t* out);
using WsmRegisterPrimitiveFn = int32_t (*)(Session*, const char*, WsmHostPrimitiveFn);
using WsmBindFn = int32_t (*)(Session*, const char*, uint64_t);
using WsmEvalStringFn = char* (*)(Session*, const char*);
using WsmFreeStringFn = void (*)(char*);
using WsmWrapGameHandleFn = int32_t (*)(Session*, void*, uint64_t*);
using WsmUnwrapGameHandleFn = int32_t (*)(Session*, uint64_t, void**);
using WsmWrapTransientStringFn = int32_t (*)(Session*, const char*, uint64_t*);
using WsmWordFn = uint64_t (*)();
using WsmAbiVersionFn = uint32_t (*)();
using WsmFeatureBitsFn = uint64_t (*)();

constexpr uint32_t kExpectedHostAbiVersion = 1;
constexpr uint64_t kRequiredHostFeatures = (1ull << 0) | (1ull << 1) | (1ull << 2);

HMODULE g_wsmModule = nullptr;
Session* g_wsmSession = nullptr;
RED4ext::v1::PluginHandle g_pluginHandle = nullptr;
const RED4ext::v1::Logger* g_logger = nullptr;
WsmEvalStringFn g_wsmEvalString = nullptr;
WsmFreeStringFn g_wsmFreeString = nullptr;
WsmBindFn g_wsmBind = nullptr;
WsmWrapGameHandleFn g_wsmWrapGameHandle = nullptr;
WsmUnwrapGameHandleFn g_wsmUnwrapGameHandle = nullptr;
WsmWrapTransientStringFn g_wsmWrapTransientString = nullptr;
uint64_t g_wordNil = 0;
uint64_t g_wordTrue = 0;
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
    *out = g_wordNil;
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
    const bool executed = RED4ext::ExecuteGlobalFunction("GetPlayer;GameInstance", &handle, gameInstance);
    const bool present = executed && static_cast<bool>(handle);
    const auto retainedHandle = g_gameHandles.Resolve(g_playerToken);
    const auto action = player_handle_epoch::Decide(executed, handle.GetPtr(), retainedHandle.GetPtr());

    if (action == player_handle_epoch::Action::BindNil)
    {
        if (g_wsmSession == nullptr || g_wsmBind == nullptr || g_wsmBind(g_wsmSession, "гравець", g_wordNil) != 0)
        {
            return 4;
        }
        g_gameHandles.Release(g_playerToken);
        g_playerToken = 0;
        g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: released absent player handle");
    }
    else if (action == player_handle_epoch::Action::BindNew)
    {
        if (g_wsmSession == nullptr || g_wsmWrapGameHandle == nullptr || g_wsmBind == nullptr)
        {
            return 5;
        }

        const auto nextToken = g_gameHandles.Retain(std::move(handle));
        if (nextToken == 0)
        {
            return 6;
        }

        uint64_t playerWord = 0;
        if (g_wsmWrapGameHandle(g_wsmSession, GameHandleTable::ToOpaqueToken(nextToken), &playerWord) != 0)
        {
            g_gameHandles.Release(nextToken);
            return 7;
        }
        if (g_wsmBind(g_wsmSession, "гравець", playerWord) != 0)
        {
            g_gameHandles.Release(nextToken);
            return 8;
        }

        const auto previousToken = g_playerToken;
        g_playerToken = nextToken;
        g_gameHandles.Release(previousToken);
        g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: bound current player as opaque token=%zu",
                         static_cast<std::size_t>(nextToken));
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
    *out = present ? g_wordTrue : g_wordNil;
    return 0;
}

int32_t ClassPrimitive(std::size_t argc, const uint64_t* argv, uint64_t* out)
{
    if (argv == nullptr || out == nullptr)
    {
        return 1;
    }
    if (argc != 1)
    {
        return 2;
    }
    if (g_wsmSession == nullptr || g_wsmUnwrapGameHandle == nullptr || g_wsmWrapTransientString == nullptr)
    {
        return 3;
    }

    void* opaqueToken = nullptr;
    if (g_wsmUnwrapGameHandle(g_wsmSession, argv[0], &opaqueToken) != 0)
    {
        return 4;
    }
    const auto handle = g_gameHandles.Resolve(GameHandleTable::FromOpaqueToken(opaqueToken));
    if (!handle || handle.GetPtr() == nullptr)
    {
        return 5;
    }
    const auto* type = handle.GetPtr()->GetType();
    if (type == nullptr)
    {
        return 6;
    }
    const char* className = type->GetName().ToString();
    if (className == nullptr)
    {
        return 7;
    }
    return g_wsmWrapTransientString(g_wsmSession, className, out) == 0 ? 0 : 8;
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

        auto abiVersion = reinterpret_cast<WsmAbiVersionFn>(GetProcAddress(wsmModule, "wsm_host_abi_version"));
        auto featureBits = reinterpret_cast<WsmFeatureBitsFn>(GetProcAddress(wsmModule, "wsm_host_feature_bits"));
        if (abiVersion == nullptr || featureBits == nullptr || abiVersion() != kExpectedHostAbiVersion ||
            (featureBits() & kRequiredHostFeatures) != kRequiredHostFeatures)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: incompatible WSM host ABI");
            return false;
        }
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
        auto unwrapGameHandle =
            reinterpret_cast<WsmUnwrapGameHandleFn>(GetProcAddress(wsmModule, "wsm_unwrap_game_handle"));
        auto wrapTransientString =
            reinterpret_cast<WsmWrapTransientStringFn>(GetProcAddress(wsmModule, "wsm_wrap_transient_string"));
        auto wordNil = reinterpret_cast<WsmWordFn>(GetProcAddress(wsmModule, "wsm_word_nil"));
        auto wordTrue = reinterpret_cast<WsmWordFn>(GetProcAddress(wsmModule, "wsm_word_true"));
        if (registerPrimitive == nullptr || bind == nullptr || evalString == nullptr || freeString == nullptr ||
            wrapGameHandle == nullptr || unwrapGameHandle == nullptr || wrapTransientString == nullptr || wordNil == nullptr ||
            wordTrue == nullptr || sessionFree == nullptr)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: required WSM host ABI export is missing");
            return false;
        }

        if (registerPrimitive(session, host_operations::OP_CP_0001.surface, &LogPrimitive) != 0)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: could not register запиши-лог");
            return false;
        }

        if (registerPrimitive(session, host_operations::OP_CP_0002.surface, &PlayerPresentPrimitive) != 0)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: could not register гравець-присутній?");
            return false;
        }

        if (registerPrimitive(session, host_operations::OP_CP_0003.surface, &ClassPrimitive) != 0)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: could not register клас");
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
                            "my-lisp-cyberpunk: could not load canonical scripts/dispatcher.lisp (or compatibility alias)");
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
        g_wsmUnwrapGameHandle = unwrapGameHandle;
        g_wsmWrapTransientString = wrapTransientString;
        g_wordNil = wordNil();
        g_wordTrue = wordTrue();
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
        g_wsmUnwrapGameHandle = nullptr;
        g_wsmWrapTransientString = nullptr;
        g_wordNil = 0;
        g_wordTrue = 0;
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
