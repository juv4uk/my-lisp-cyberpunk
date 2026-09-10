#pragma once

#include <RED4ext/Handle.hpp>
#include <RED4ext/Scripting/IScriptable.hpp>

#include <cstdint>
#include <mutex>
#include <unordered_map>

// Owns RED4ext reference-counted handles for as long as Lisp may retain a
// matching opaque token. The token is an adapter-local number, never a raw
// REDengine object address.
class GameHandleTable
{
public:
    using Token = std::uintptr_t;

    [[nodiscard]] Token Retain(RED4ext::Handle<RED4ext::IScriptable> handle);
    [[nodiscard]] RED4ext::Handle<RED4ext::IScriptable> Resolve(Token token) const;
    void Release(Token token);
    void Clear();

    [[nodiscard]] static void* ToOpaqueToken(Token token) noexcept;
    [[nodiscard]] static Token FromOpaqueToken(void* token) noexcept;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<Token, RED4ext::Handle<RED4ext::IScriptable>> m_handles;
    Token m_nextToken = 1;
};
