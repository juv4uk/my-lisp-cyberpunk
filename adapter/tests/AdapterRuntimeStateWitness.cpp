#include "AdapterRuntimeState.hpp"

#include <iostream>

int main()
{
    adapter_runtime_state::RuntimeState state;

    if (!state.ShouldLogCapability() || state.ShouldLogCapability())
    {
        std::cerr << "capability log deduplication is incorrect\n";
        return 1;
    }

    if (!state.PlayerPresenceChanged(true) || state.PlayerPresenceChanged(true) ||
        !state.PlayerPresenceChanged(false))
    {
        std::cerr << "player presence transition logging is incorrect\n";
        return 2;
    }

    state.ResetForUnload();
    if (!state.ShouldLogCapability() || !state.PlayerPresenceChanged(true))
    {
        std::cerr << "plugin reload did not reset lifecycle logging state\n";
        return 3;
    }

    return 0;
}
