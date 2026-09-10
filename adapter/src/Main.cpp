// my-lisp-cyberpunk RED4ext host adapter.
//
// Purpose of THIS pass: prove wsm_my_lisp_cyberpunk_dll.dll (dll/, the
// win64-nucleus + reader/eval/ffi crate) actually loads inside the game
// process and its wsm_session_init export is callable. It then proves one
// read-only Lisp -> host -> Lisp path with the log-only запиши-лог primitive.
// RTTI hooks and every state-changing game capability remain later steps.
//
// The adapter is intentionally Cyberpunk-specific. The loaded WSM runtime
// stays host-neutral in juv4uk/wsm-my-lisp.

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/ScriptGameInstance.hpp>

#include <windows.h>

#include "GameHandleTable.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace
{
// Matches dll/src/ffi.rs's `Session` opaque pointer type exactly: this
// plugin never dereferences it, only passes it back to wsm_session_free.
using Session = void;

// wsm-os-target::Tag::Nil / ::True as bare Words. The plugin intentionally
// keeps these ABI-level literals local until wsm-target-contract publishes
// a C header for them (same rationale as LogPrimitive's original comment).
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
// Stashed at Load so the later Running-state callback (a non-capturing
// function pointer, per RED4ext::v1::GameState's shape) can still reach
// them without re-resolving via GetProcAddress.
WsmEvalStringFn g_wsmEvalString = nullptr;
WsmFreeStringFn g_wsmFreeString = nullptr;
WsmBindFn g_wsmBind = nullptr;
WsmWrapGameHandleFn g_wsmWrapGameHandle = nullptr;
GameHandleTable g_gameHandles;
GameHandleTable::Token g_playerToken = 0;
// Єдина fixed-dispatch програма для подій Running. Її завантажуємо один раз
// під час Load; C++ не читає її результат як команду й не містить її policy.
std::string g_dispatchSource;

// The first host primitive is deliberately reversible: it writes only to
// RED4ext's log. It proves Lisp -> host -> Lisp without touching a save,
// inventory, player position, or raw game object.
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

    g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: Lisp host primitive запиши-лог invoked");
    *out = kWordNil;
    return 0;
}

// Read-only capability: читає факт наявності player instance і повертає t/().
// За першої появи механічно утримує RED4ext Handle у таблиці хоста та зв'язує
// непрозорий token як `гравець`; жодного game state не читає й не змінює.
//
// GetPlayer;GameInstance can genuinely fail this early (EMainReason::Load
// fires before a player instance necessarily exists) -- that is not an
// error, it is the honest current-game-state answer, so it maps to nil
// rather than a host-primitive error code.
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
    // Pattern taken directly from RED4ext.SDK's own
    // examples/accessing_properties/Main.cpp (IsPlayerCrouched): resolve the
    // player instance at call time via ExecuteGlobalFunction, not at plugin
    // Load, and treat ExecuteGlobalFunction returning false the same as an
    // empty handle -- both mean "no player right now."
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

    g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: Lisp host primitive гравець-присутній? invoked, present=%s",
                     present ? "true" : "false");
    *out = present ? kWordTrue : kWordNil;
    return 0;
}

// The plugin's own directory, where the loader placed both this DLL and
// wsm_my_lisp_cyberpunk_dll.dll side by side (RED4ext's own convention --
// see docs/game-injection-plan.md's "(a) The standard, sanctioned path").
// RAII guard over the loaded WSM module + its session. Every early-return
// failure path during EMainReason::Load must leave neither dangling: this
// guard's destructor calls wsm_session_free (if a session exists and the
// export was found) and FreeLibrary (if the module is still loaded), unless
// release() was called first. This replaces the previous pattern of
// hand-repeating sessionFree+FreeLibrary on each failure branch, which had
// already grown one real gap (the wsm_eval_string == nullptr path skipped
// cleanup entirely).
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

    // Disarms the guard: ownership of module/session moves to the caller
    // (the global g_wsmModule/g_wsmSession that outlive plugin Load).
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

// RED4ext викликає OnUpdate для Running на кожному frame. Це лише доставка
// події у фіксовану точку входу: сценарій у scripts/диспетчер.мій вирішує,
// які факти читати, які capabilities викликати й що робити з їх результатами.
// Поточний ABI не має persistable Lisp definitions, тому dispatch v0 є одним
// top-level `.my` виразом, завантаженим під час Load, без callback registry.
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
        return true;
    }

    // Це observability, не protocol: C++ не порівнює результат із t/() і не
    // робить з нього наступний крок. Наступний крок уже виконав `.my`.
    g_logger->InfoF(g_pluginHandle, "my-lisp-cyberpunk: Lisp dispatch => %s", result);
    g_wsmFreeString(result);
    return true;
}

std::wstring GetOwnDirectory()
{
    wchar_t path[MAX_PATH] = {};
    HMODULE self = nullptr;
    // GetModuleHandleExW with GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS finds
    // THIS DLL's own module handle from an address inside it (this
    // function), regardless of what the game's own module search path is.
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
    const std::filesystem::path dispatchPath =
        std::filesystem::path(GetOwnDirectory()) / L"scripts" / L"диспетчер.мій";
    std::ifstream source(dispatchPath, std::ios::binary);
    if (!source)
    {
        return false;
    }

    out.assign(std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>());
    return !out.empty();
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
            // GetLastError() is deliberately not decoded into a message
            // here -- untested against a real failure case (wrong path,
            // missing MSVC runtime, wrong bitness). A future pass should
            // use FormatMessageW for a readable error, not just the code.
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
            return false; // guard frees wsmModule
        }

        Session* session = sessionInit();
        if (session == nullptr)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: wsm_session_init returned null");
            return false; // guard frees wsmModule
        }

        auto sessionFree =
            reinterpret_cast<WsmSessionFreeFn>(GetProcAddress(wsmModule, "wsm_session_free"));
        guard.SetSession(session, sessionFree); // armed from here: every remaining
                                                 // early return frees session + module

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
            return false; // guard frees session + wsmModule
        }

        if (registerPrimitive(session, "запиши-лог", &LogPrimitive) != 0)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: could not register запиши-лог");
            return false; // guard frees session + wsmModule
        }

        if (registerPrimitive(session, "гравець-присутній?", &PlayerPresentPrimitive) != 0)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: could not register гравець-присутній?");
            return false; // guard frees session + wsmModule
        }

        // Це перевірка лише evaluator/ABI, без виклику capability: C++ не
        // вибирає жодної поведінки мода під час завантаження.
        char* result = evalString(session, "(quote ())");
        if (result == nullptr)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: wsm_eval_string returned null");
            return false; // guard frees session + wsmModule
        }

        // The vertical slice's oracle expectation (docs/vertical-slice.md) is
        // exactly "()", not merely "eval produced some non-null string". A
        // non-nil/non-error result here means the runtime disagrees with the
        // reference my-lisp oracle for this fixture, and plugin load must
        // fail closed rather than report a false success.
        const bool matchesOracle = std::strcmp(result, "()") == 0;
        if (!matchesOracle)
        {
            logger->ErrorF(aHandle,
                            "my-lisp-cyberpunk: (quote ()) => %s, expected () -- vertical slice oracle "
                            "mismatch, failing load",
                            result);
            freeString(result);
            return false; // guard frees session + wsmModule
        }

        logger->InfoF(aHandle, "my-lisp-cyberpunk: (quote ()) => %s", result);
        freeString(result);

        std::string dispatchSource;
        if (!LoadDispatchSource(dispatchSource))
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: could not load scripts/диспетчер.мій");
            return false;
        }

        logger->InfoF(aHandle,
                       "my-lisp-cyberpunk: wsm_my_lisp_cyberpunk_dll.dll loaded, session=%p", session);

        // Everything after this point owns the module/session for the
        // plugin's lifetime; EMainReason::Unload releases them explicitly.
        g_wsmModule = wsmModule;
        g_wsmSession = session;
        g_wsmEvalString = evalString;
        g_wsmFreeString = freeString;
        g_wsmBind = bind;
        g_wsmWrapGameHandle = wrapGameHandle;
        g_dispatchSource = std::move(dispatchSource);
        guard.Release();

        // OnUpdate доставляє Running tick. RTTI торкається лише та
        // capability, яку обере Lisp-диспетчер, а не сам C++ scheduler.
        static RED4ext::v1::GameState playerPresentState{
            .OnEnter = nullptr,
            .OnUpdate = &DispatchRunningTick,
            .OnExit = nullptr,
        };
        aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &playerPresentState);
        break;
    }
    case RED4ext::v1::EMainReason::Unload:
    {
        g_gameHandles.Clear();
        g_playerToken = 0;
        g_dispatchSource.clear();
        if (g_wsmModule != nullptr)
        {
            if (g_wsmSession != nullptr)
            {
                auto sessionFree =
                    reinterpret_cast<WsmSessionFreeFn>(GetProcAddress(g_wsmModule, "wsm_session_free"));
                if (sessionFree != nullptr)
                {
                    sessionFree(g_wsmSession);
                }
                g_wsmSession = nullptr;
            }
            FreeLibrary(g_wsmModule);
            g_wsmModule = nullptr;
        }
        g_wsmEvalString = nullptr;
        g_wsmFreeString = nullptr;
        g_wsmBind = nullptr;
        g_wsmWrapGameHandle = nullptr;
        g_logger = nullptr;
        g_pluginHandle = nullptr;
        break;
    }
    }

    return true;
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
