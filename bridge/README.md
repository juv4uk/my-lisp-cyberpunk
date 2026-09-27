# bridge — CP-PROBE-INPROCESS-V0 (#31)

The minimal in-process bridge: a `version.dll` proxy loaded by the
game the same way CET's own `version.dll` used to be, but authored
entirely by this repo, with zero RED4ext/CET/Codeware dependency.

## What it does, exactly

1. Exports the same 16 real `version.dll` functions any Windows
   process expects to find there. Each is a thin forward: load the
   genuine system DLL (deployed alongside as `version-original.dll`)
   once, look up the real function by name, call it with its real
   `<winver.h>` signature. No game subsystem that legitimately needs
   real `version.dll` behavior can be broken by this proxy — see
   `src/BridgeDllMain.cpp`'s header comment for why plain `.def`
   forwarder syntax (`Name=OtherDll.Name`) was tried first and
   abandoned (MSVC `LNK2001: unresolved external`).
2. `DllMain` does nothing beyond spawning one thread
   (`DisableThreadLibraryCalls` + `CreateThread`), per this repo's own
   paid-for lesson
   (`docs/deep-penetration-roadmap-2026-09-10.md`, problem 1: "RTTI
   call during Load").
3. That thread waits 2s, then writes one
   [`game-observation/1`](../docs/observation-contract-v0.md) record to
   `bridge-observation.lisp` next to the DLL. The fact is
   `bridge-alive` — this first cut reports only that the bridge itself
   loaded and is running inside the target process (module base,
   process id), not any game/engine state. No RTTI, no game pointers,
   no mutating action, per #31's hard constraints.

## Verified without the game

`tests/LoadTest.cpp` (`bridge-load-test.exe`) is a standalone harness
— not injected into Cyberpunk — that `LoadLibrary`s the built
`version.dll` exactly as any Windows process would, calls one
forwarded function, and confirms the real system DLL answered. Run:

```powershell
cmake -S bridge -B bridge/build -G "Visual Studio 17 2022"
cmake --build bridge/build --config Release
copy C:\Windows\System32\version.dll bridge\build\Release\version-original.dll
bridge\build\Release\bridge-load-test.exe bridge\build\Release\version.dll
```

Confirmed this session: forwarding reaches the real system DLL, the
background thread fires, and the written record roundtrips through
canonical `my-lisp --oracle-check` as valid data.

## NOT yet done

This has not been deployed into the actual game directory or loaded by
`Cyberpunk2077.exe`. Per #31's stop condition, the next step is
exactly that one live load — nothing more (no RTTI browser, no event
framework, no additional facts) — and needs explicit confirmation
before touching the live game install, given this repo's own crash
history with in-process code.
