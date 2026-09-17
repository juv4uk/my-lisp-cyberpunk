#pragma once

#include <windows.h>

namespace cyberpunk_bridge
{

// Runs on the bridge-owned worker thread after the loader lock is released.
// Owns the canonical Session and drains the already-existing loopback queue.
DWORD RunCanonicalRepl(HMODULE ownerModule);

// Controlled-unload mechanism used by the standalone harness and any future
// host that wants to unload the bridge explicitly.  DllMain never waits.
bool ShutdownCanonicalRepl(DWORD timeoutMs);

} // namespace cyberpunk_bridge
