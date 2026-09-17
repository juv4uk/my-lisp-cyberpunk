#pragma once

#include <windows.h>

#include <cstdint>
#include <string>

#include "my_lisp_embed.h"

namespace cyberpunk_bridge
{

// Mechanism-only owner of one canonical my-lisp-embed Session.
//
// This class intentionally knows nothing about Lisp syntax or game policy.
// It only loads the exact adjacent embed DLL, verifies the upstream ABI,
// owns one Session, relays UTF-8 source to upstream eval, and frees upstream
// result strings through the matching ABI function.
class CanonicalReplHost
{
public:
    CanonicalReplHost() = default;
    ~CanonicalReplHost();

    CanonicalReplHost(const CanonicalReplHost&) = delete;
    CanonicalReplHost& operator=(const CanonicalReplHost&) = delete;

    [[nodiscard]] bool Start(HMODULE ownerModule);
    [[nodiscard]] std::string Evaluate(const std::string& source);
    void Stop();

    [[nodiscard]] std::uint32_t AbiVersion() const noexcept { return m_abiVersion; }
    [[nodiscard]] bool Running() const noexcept { return m_session != nullptr; }

private:
    using AbiVersionFn = decltype(&my_lisp_embed_abi_version);
    using SessionNewFn = decltype(&my_lisp_embed_session_new);
    using EvalFn = decltype(&my_lisp_embed_eval);
    using FreeStringFn = decltype(&my_lisp_embed_free_string);
    using SessionFreeFn = decltype(&my_lisp_embed_session_free);

    HMODULE m_embedModule = nullptr;
    MyLispEmbedSession* m_session = nullptr;
    AbiVersionFn m_abiVersionFn = nullptr;
    SessionNewFn m_sessionNew = nullptr;
    EvalFn m_eval = nullptr;
    FreeStringFn m_freeString = nullptr;
    SessionFreeFn m_sessionFree = nullptr;
    std::uint32_t m_abiVersion = 0;
};

} // namespace cyberpunk_bridge
