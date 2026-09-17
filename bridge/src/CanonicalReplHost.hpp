#pragma once

#include <cstdint>
#include <string>

#include "my_lisp_embed.h"

namespace cyberpunk_bridge
{

// Mechanism-only owner of one canonical my-lisp-embed Session.
//
// The canonical runtime is statically linked into version.dll.  This class
// intentionally knows nothing about Lisp syntax or game policy: it verifies
// the existing upstream ABI, owns one Session, relays UTF-8 source to upstream
// eval, and frees upstream result strings through that same C ABI.
class CanonicalReplHost
{
public:
    CanonicalReplHost() = default;
    ~CanonicalReplHost();

    CanonicalReplHost(const CanonicalReplHost&) = delete;
    CanonicalReplHost& operator=(const CanonicalReplHost&) = delete;

    [[nodiscard]] bool Start();
    [[nodiscard]] std::string Evaluate(const std::string& source);
    void Stop();

    [[nodiscard]] std::uint32_t AbiVersion() const noexcept { return m_abiVersion; }
    [[nodiscard]] bool Running() const noexcept { return m_session != nullptr; }

private:
    MyLispEmbedSession* m_session = nullptr;
    std::uint32_t m_abiVersion = 0;
};

} // namespace cyberpunk_bridge
