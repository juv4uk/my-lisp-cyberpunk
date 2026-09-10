#include "GameHandleTable.hpp"

#include <utility>

GameHandleTable::Token GameHandleTable::Retain(RED4ext::Handle<RED4ext::IScriptable> handle)
{
    if (!handle)
    {
        return 0;
    }

    std::lock_guard lock(m_mutex);
    const Token token = m_nextToken++;
    // Token zero is reserved as "no handle". Overflow is unrealistic for one
    // game session, but preserving the invariant costs little.
    if (token == 0)
    {
        return 0;
    }
    m_handles.emplace(token, std::move(handle));
    return token;
}

RED4ext::Handle<RED4ext::IScriptable> GameHandleTable::Resolve(Token token) const
{
    if (token == 0)
    {
        return {};
    }

    std::lock_guard lock(m_mutex);
    const auto it = m_handles.find(token);
    return it == m_handles.end() ? RED4ext::Handle<RED4ext::IScriptable>{} : it->second;
}

void GameHandleTable::Release(Token token)
{
    std::lock_guard lock(m_mutex);
    m_handles.erase(token);
}

void GameHandleTable::Clear()
{
    std::lock_guard lock(m_mutex);
    m_handles.clear();
}

void* GameHandleTable::ToOpaqueToken(Token token) noexcept
{
    return reinterpret_cast<void*>(token);
}

GameHandleTable::Token GameHandleTable::FromOpaqueToken(void* token) noexcept
{
    return reinterpret_cast<Token>(token);
}
