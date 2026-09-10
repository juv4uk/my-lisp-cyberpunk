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

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace
{
// Matches dll/src/ffi.rs's `Session` opaque pointer type exactly: this
// plugin never dereferences it, only passes it back to wsm_session_free.
using Session = void;

using WsmSessionInitFn = Session* (*)();
using WsmSessionFreeFn = void (*)(Session*);
using WsmHostPrimitiveFn = int32_t (*)(std::size_t argc, const uint64_t* argv, uint64_t* out);
using WsmRegisterPrimitiveFn = int32_t (*)(Session*, const char*, WsmHostPrimitiveFn);
using WsmEvalStringFn = char* (*)(Session*, const char*);
using WsmFreeStringFn = void (*)(char*);

HMODULE g_wsmModule = nullptr;
Session* g_wsmSession = nullptr;
RED4ext::v1::PluginHandle g_pluginHandle = nullptr;
const RED4ext::v1::Logger* g_logger = nullptr;

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
    // WORD_NIL, the canonical Lisp result (). The plugin intentionally keeps
    // this ABI-level literal local until wsm-target-contract publishes it.
    *out = 1;
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
        auto evalString = reinterpret_cast<WsmEvalStringFn>(GetProcAddress(wsmModule, "wsm_eval_string"));
        auto freeString = reinterpret_cast<WsmFreeStringFn>(GetProcAddress(wsmModule, "wsm_free_string"));
        if (registerPrimitive == nullptr || evalString == nullptr || freeString == nullptr ||
            sessionFree == nullptr)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: required WSM host ABI export is missing");
            return false; // guard frees session + wsmModule
        }

        if (registerPrimitive(session, "запиши-лог", &LogPrimitive) != 0)
        {
            logger->ErrorF(aHandle, "my-lisp-cyberpunk: could not register запиши-лог");
            return false; // guard frees session + wsmModule
        }

        char* result = evalString(session, "(запиши-лог)");
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
                            "my-lisp-cyberpunk: (запиши-лог) => %s, expected () -- vertical slice oracle "
                            "mismatch, failing load",
                            result);
            freeString(result);
            return false; // guard frees session + wsmModule
        }

        logger->InfoF(aHandle, "my-lisp-cyberpunk: (запиши-лог) => %s", result);
        freeString(result);

        logger->InfoF(aHandle,
                       "my-lisp-cyberpunk: wsm_my_lisp_cyberpunk_dll.dll loaded, session=%p", session);

        // Everything after this point owns the module/session for the
        // plugin's lifetime; EMainReason::Unload releases them explicitly.
        g_wsmModule = wsmModule;
        g_wsmSession = session;
        guard.Release();
        break;
    }
    case RED4ext::v1::EMainReason::Unload:
    {
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
    // RUNTIME_VERSION_INDEPENDENT: this adapter does not touch game RTTI or
    // state; it only writes a lifecycle proof to RED4ext's own logger, so
    // pinning to a specific game version isn't needed yet, unlike a
    // plugin that actually hooks game functions would require.
    aInfo->runtime = RED4EXT_V1_RUNTIME_VERSION_INDEPENDENT;
    aInfo->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
