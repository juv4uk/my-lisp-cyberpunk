# NeuralDeck F10 crash — 2026-09-15

## Impact

Pressing F10 in a live game session crashed the game process
(`EXCEPTION_ACCESS_VIOLATION`).

## Evidence

The game's own crash-reporting queue (not `CrashInfo.json`, which only
carries high-level telemetry) is at:

```
%LOCALAPPDATA%\REDEngine\ReportQueue\Cyberpunk2077-<date>-<time>-<pid>-<tid>\
```

For this incident:
`Cyberpunk2077-20260915-131550-17608-1460\stacktrace.txt`:

```text
Error reason: Unhandled exception
Expression: EXCEPTION_ACCESS_VIOLATION (0xC0000005)
Message: The thread attempted to read inaccessible data at 0xFFFFFFFFFFFFFFFF.
File: <Unknown>(0)
```

`report.txt` in the same folder confirms both our DLLs
(`my-lisp-cyberpunk-plugin.dll`, `wsm_my_lisp_cyberpunk_dll.dll`) were
loaded (`LoadedModule[74]`, `[75]`). Neither `report.txt` nor
`attch/Cyberpunk2077.exe-*.txt` contains a symbolized callstack — both are
telemetry key/value dumps only. The binary `Cyberpunk2077.dmp` would carry
one, but no local debugger/symbol tool was used to parse it.

The crash timestamp (13:15:50) matches, to the second, our own adapter's
breadcrumb log line:

```
[2026-09-15 13:15:50.013] ... NeuralDeck stage entering: GetUISystem (native ExecuteFunction call)
```

This places the fault inside/immediately after the `GetUISystem` native
call in `Red4extNeuralDeckEngine::GetUiSystem()`
(`adapter/src/Main.cpp`).

## Root cause

Comparing the crashing call against a call that has run correctly every
frame for the whole session (`PlayerPresentPrimitive`, `Main.cpp`) shows a
concrete argument-passing mismatch:

Working call (`Main.cpp:110-112`):

```cpp
RED4ext::ScriptGameInstance gameInstance;
RED4ext::ExecuteGlobalFunction("GetPlayer;GameInstance", &handle, gameInstance);
```

`gameInstance` is passed **by value**.

Crashing call (`Main.cpp:334-335`):

```cpp
RED4ext::ScriptGameInstance gameInstance;
RED4ext::ExecuteFunction("ScriptGameInstance", "GetUISystem", &uiSystem, &gameInstance);
```

`&gameInstance` is passed as a **pointer**.

`RED4ext::ExecuteFunction`/`ExecuteGlobalFunction` push arguments onto the
VM stack according to the target function's real parameter type. The real
`GetUISystem` signature, like `GetPlayer`, expects `GameInstance` by
value. Passing `&gameInstance` puts the address of a local stack variable
onto the VM argument stack instead of the `ScriptGameInstance` value
itself; the engine reads that pointer's bytes as (or through) the
expected struct, and the resulting dereference lands on a garbage address
— consistent with the observed `0xFFFFFFFFFFFFFFFF` read.

This is the same class of bug as the 2026-09-14 F10 incident (Redscript
alias used as an RTTI name instead of the real identity): a mismatch
between what the native SDK call site assumes and what the engine's real
ABI/signature requires, caught only by a live crash, not by compilation
or unit tests.

## Status

Root cause identified from evidence (crash-dump exception data + log
timestamp correlation + working/crashing call-site comparison), not yet
fixed. A previously drafted, now-superseded hypothesis (that
`ScriptGameInstance`'s SDK-internal `sizeof()` vs RTTI `GetSize()` check
was aborting the process) is disproven by the stacktrace: the crash is a
hard access violation, not an `abort()`/`MessageBox` path.

## Next step

Change `GetUiSystem()` to pass `gameInstance` by value to
`ExecuteFunction`, matching the working `PlayerPresentPrimitive` call
shape; rebuild, run the full CTest suite, redeploy, and verify with a
single controlled F10 test.
