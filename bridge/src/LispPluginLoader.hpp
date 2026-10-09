#pragma once

#include <windows.h>

namespace cyberpunk_bridge
{

class CanonicalReplHost;

// Mechanism-only loader for optional user-owned Lisp plugin source.
//
// Discovery, file I/O, hashing and evidence rendering live here.  Lisp
// parsing/evaluation/Session state stay exclusively in CanonicalReplHost's
// upstream my-lisp-embed Session.
class LispPluginLoader
{
public:
    [[nodiscard]] static bool LoadAndRecord(CanonicalReplHost& host, HMODULE ownerModule);
};

} // namespace cyberpunk_bridge
