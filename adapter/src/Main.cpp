// my-lisp-cyberpunk RED4ext host adapter.
//
// Purpose of THIS pass: prove wsm_my_lisp_cyberpunk_dll.dll loads inside the
// game process and host primitives work. RTTI/mutating capabilities later.
// `.lisp` is canonical; legacy extensions are compatibility-only for dispatch load.

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/ScriptGameInstance.hpp>
#include <RED4ext/Scripting/Utils.hpp>

#include <windows.h>

#include "generated/host_operations.generated.hpp"
#include "generated/host_bindings.generated.hpp"

#include "GameHandleTable.hpp"
#include "AdapterRuntimeState.hpp"
#include "LocalReplQueue.hpp"
#include "LocalReplServer.hpp"
#include "NeuralDeckBridge.hpp"
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
adapter_runtime_state::RuntimeState g_runtimeState;
std::string g_dispatchSource;
local_repl::RequestQueue g_replRequests;
local_repl::LocalReplServer g_replServer;
bool g_neuralDeckF10Down = false;

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

    if (g_runtimeState.ShouldLogCapability())
    {
        g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: Lisp host primitive запиши-лог invoked");
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
        if (g_wsmSession == nullptr || g_wsmBind == nullptr || g_wsmBind(g_wsmSession, host_bindings::CPB_0001.surface, g_wordNil) != 0)
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
        if (g_wsmBind(g_wsmSession, host_bindings::CPB_0001.surface, playerWord) != 0)
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
    if (g_runtimeState.PlayerPresenceChanged(present))
    {
        g_logger->InfoF(g_pluginHandle,
                         "my-lisp-cyberpunk: Lisp host primitive гравець-присутній? invoked, present=%s",
                         present ? "true" : "false");
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

// Hardware input is a host fact. The C++ adapter only turns its F10 edge into
// a typed UI event; compiled Redscript owns the popup lifecycle and visuals.
struct Red4extNeuralDeckEngine
{
    RED4ext::CRTTISystem* rtti = nullptr;
    RED4ext::CClass* eventClass = nullptr;
    RED4ext::CClass* uiClass = nullptr;
    RED4ext::CBaseFunction* queueEvent = nullptr;
    RED4ext::Handle<RED4ext::IScriptable> uiSystem;
    RED4ext::Handle<RED4ext::IScriptable> event;

    // Pre-call breadcrumbs, not post-call results: NeuralDeckBridge.hpp's
    // QueueToggle only logs its final outcome, AFTER every step has already
    // run. If any of the native calls below crash the process (which a raw
    // RED4ext::ExecuteFunction/CreateInstance call into live engine state
    // genuinely can), that final log line never fires and the incident
    // leaves zero trace -- exactly what happened 2026-09-14 22:46: the
    // plugin log's last line was at 22:45:33, CrashInfo.json's timeCrash
    // was 22:46:56, and not one "F10 edge" line exists in between. Every
    // method here that makes a native RTTI call now logs its own name
    // immediately before making that call, so a repeat crash leaves the
    // exact failing stage as the log's last line instead of silence.
    static void LogStage(const char* stage)
    {
        if (g_logger != nullptr)
        {
            g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: NeuralDeck stage entering: %s", stage);
        }
    }

    bool HasRtti() { rtti = RED4ext::CRTTISystem::Get(); return rtti != nullptr; }
    bool HasToggleEventClass() { eventClass = rtti->GetClass("NeuralDeckToggleEvent"); return eventClass != nullptr; }
    bool HasUiSystemClass() { uiClass = neuraldeck::LookupUiSystemClass(*rtti); return uiClass != nullptr; }
    bool HasQueueEventMethod() { queueEvent = uiClass->GetFunction("QueueEvent"); return queueEvent != nullptr; }
    bool GetUiSystem()
    {
        LogStage("GetUISystem (native ExecuteFunction call)");

        // Root cause of the 2026-09-15 F10 access-violation crash (see
        // docs/neuraldeck-f10-crash-2026-09-15.md): ExecuteFunction packs
        // each argument onto the VM stack by the target function's real
        // parameter type. GetUISystem expects GameInstance by value, the
        // same as the working GetPlayer call below in
        // PlayerPresentPrimitive -- passing &gameInstance put the address
        // of a local variable on the argument stack instead of the value
        // itself, which the engine then read through as if it were the
        // struct.
        RED4ext::ScriptGameInstance gameInstance;
        return RED4ext::ExecuteFunction("ScriptGameInstance", "GetUISystem", &uiSystem, gameInstance);
    }
    bool HasUiSystemHandle() const { return uiSystem != nullptr; }
    bool CreateToggleEvent()
    {
        LogStage("CreateInstance(NeuralDeckToggleEvent)");
        auto* rawEvent = static_cast<RED4ext::IScriptable*>(eventClass->CreateInstance());
        if (rawEvent == nullptr) return false;
        event = RED4ext::Handle<RED4ext::IScriptable>(rawEvent);
        return true;
    }
    bool QueueEventReturnsVoid() const { return queueEvent->returnType == nullptr; }
    bool SubmitToggleEvent()
    {
        LogStage("QueueEvent (native ExecuteFunction call on live UISystem instance)");
        RED4ext::StackArgs_t args;
        args.emplace_back(nullptr, &event);
        return RED4ext::ExecuteFunction(uiSystem.instance, queueEvent, nullptr, args);
    }
    void ReleaseToggleEvent() { event = nullptr; }
};
void PollNeuralDeckToggle()
{
    const bool f10Down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    Red4extNeuralDeckEngine engine;
    const char* outcome = neuraldeck::PollToggle(engine, f10Down, g_neuralDeckF10Down);
    if (outcome == nullptr)
    {
        return;
    }

    if (g_logger != nullptr)
    {
        // This is emitted only on a key edge, never per frame.  Keep every
        // identity required to diagnose a live UI failure in the one record.
        g_logger->InfoF(g_pluginHandle,
                         "my-lisp-cyberpunk: NeuralDeck F10 edge vk=%u event=NeuralDeckToggleEvent "
                         "ui-rtti=%s outcome=%s",
                         static_cast<unsigned>(VK_F10), neuraldeck::kUiSystemRttiName, outcome);
    }
}

bool OnRunningEnter(RED4ext::CGameApplication*)
{
    if (g_logger != nullptr)
    {
        g_logger->InfoF(g_pluginHandle,
                         "my-lisp-cyberpunk: entered Running state; fixed dispatch bytes=%zu; "
                         "NeuralDeck hotkey=F10",
                         g_dispatchSource.size());
    }
    return true;
}

bool OnRunningExit(RED4ext::CGameApplication*)
{
    if (g_logger != nullptr)
    {
        g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: left Running state");
    }
    return true;
}

bool DispatchRunningTick(RED4ext::CGameApplication*)
{
    PollNeuralDeckToggle();
    if (g_wsmSession == nullptr || g_wsmEvalString == nullptr || g_wsmFreeString == nullptr || g_logger == nullptr)
    {
        return true;
    }

    g_replServer.Drain([](const std::string& source) {
        char* result = g_wsmEvalString(g_wsmSession, source.c_str());
        if (result == nullptr)
        {
            return std::string("error: Lisp evaluation returned null");
        }
        std::string text(result);
        g_wsmFreeString(result);
        return text;
    });

    if (g_dispatchSource.empty())
    {
        return false;
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
        const auto name = used.filename().u8string();
        g_logger->InfoF(g_pluginHandle,
                         "my-lisp-cyberpunk: loaded dispatch source file=%.*s bytes=%zu",
                         static_cast<int>(name.size()), reinterpret_cast<const char*>(name.data()), out.size());
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
        logger->InfoF(aHandle,
                      "my-lisp-cyberpunk: WSM host ABI validated version=%u features=0x%llX required=0x%llX",
                      static_cast<unsigned>(abiVersion()), static_cast<unsigned long long>(featureBits()),
                      static_cast<unsigned long long>(kRequiredHostFeatures));
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
        runningState.OnEnter = &OnRunningEnter;
        runningState.OnUpdate = &DispatchRunningTick;
        runningState.OnExit = &OnRunningExit;
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
        if (!g_replServer.Start(g_replRequests))
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: could not start local REPL on 127.0.0.1:%u",
                           static_cast<unsigned>(local_repl::kDefaultPort));
        }
        else
        {
            logger->InfoF(aHandle, "my-lisp-cyberpunk: local REPL listening on 127.0.0.1:%u",
                          static_cast<unsigned>(local_repl::kDefaultPort));
        }
        guard.Release();

        logger->InfoF(aHandle, "my-lisp-cyberpunk: wsm_my_lisp_cyberpunk_dll.dll loaded, session ready");
        return true;
    }
    case RED4ext::v1::EMainReason::Unload:
    {
        g_replServer.Stop();
        g_runtimeState.ResetForUnload();
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
