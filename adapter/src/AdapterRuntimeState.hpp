#pragma once

#include <cstdint>

// State owned by one loaded adapter instance.  It deliberately has an
// explicit Unload reset so a RED4ext script reload starts a fresh lifecycle.
namespace adapter_runtime_state
{
class RuntimeState
{
public:
    [[nodiscard]] bool ShouldLogCapability() noexcept
    {
        if (m_capabilityLogged)
        {
            return false;
        }
        m_capabilityLogged = true;
        return true;
    }

    [[nodiscard]] bool PlayerPresenceChanged(bool present) noexcept
    {
        const auto next = static_cast<int8_t>(present ? 1 : 0);
        if (m_playerPresence == next)
        {
            return false;
        }
        m_playerPresence = next;
        return true;
    }

    void ResetForUnload() noexcept
    {
        m_capabilityLogged = false;
        m_playerPresence = -1;
    }

private:
    bool m_capabilityLogged = false;
    int8_t m_playerPresence = -1;
};
} // namespace adapter_runtime_state
