# Local paths and work record — 2026-09-14

## Scope

This is the operational map for the local Cyberpunk installation and the
`my-lisp-cyberpunk` work completed during the NeuralDeck/F10 debugging pass.
It records facts, paths and completed changes. It does not claim that the
popup is visibly working until a fresh live witness exists.

## Authority and source paths

| Purpose | Local path | Owner |
| --- | --- | --- |
| Cyberpunk adapter repository | `C:\GitHub\my-lisp-cyberpunk` | this repository |
| Lisp semantics | `C:\GitHub\my-lisp` | `my-lisp` |
| Windows embedded runtime source | `C:\GitHub\my-lisp-cyberpunk\host-runtime` | this repository |
| RED4ext adapter source | `C:\GitHub\my-lisp-cyberpunk\adapter\src` | this repository |
| NeuralDeck presentation source | `C:\GitHub\my-lisp-cyberpunk\redscript\NeuralDeck\NeuralDeckOverlay.reds` | this repository |
| Lisp scenarios | `C:\GitHub\my-lisp-cyberpunk\scripts` | this repository |
| Current Release adapter build | `C:\GitHub\my-lisp-cyberpunk\adapter\build\src\Release\my-lisp-cyberpunk-plugin.dll` | build output |
| Current Release Redscript staging | `C:\GitHub\my-lisp-cyberpunk\adapter\build\src\Release\redscript\NeuralDeck\NeuralDeckOverlay.reds` | build output |

`my-lisp` remains semantic authority. The adapter and the Windows runtime are
hosts; they do not define an alternative Lisp language.

## Game and installed payload paths

| Purpose | Local path |
| --- | --- |
| Game root | `D:\games\Cyberpunk 2077 v.2.31 (2020)\Cyberpunk 2077` |
| Game executable | `...\bin\x64\Cyberpunk2077.exe` |
| RED4ext | `...\red4ext\RED4ext.dll` |
| Codeware | `...\red4ext\plugins\Codeware\Codeware.dll` |
| Our plugin directory | `...\red4ext\plugins\wsm-my-lisp-cyberpunk-plugin` |
| Adapter DLL | `...\red4ext\plugins\wsm-my-lisp-cyberpunk-plugin\my-lisp-cyberpunk-plugin.dll` |
| Lisp runtime DLL | `...\red4ext\plugins\wsm-my-lisp-cyberpunk-plugin\wsm_my_lisp_cyberpunk_dll.dll` |
| Installed Lisp scenarios | `...\red4ext\plugins\wsm-my-lisp-cyberpunk-plugin\scripts` |
| Installed NeuralDeck Redscript | `...\r6\scripts\NeuralDeck\NeuralDeckOverlay.reds` |
| RED4ext logs | `...\red4ext\logs` |
| my-lisp plugin logs | `...\red4ext\logs\my-lisp-cyberpunk-plugin-*.log` |
| Redscript log | `...\r6\logs\redscript_rCURRENT.log` |

The adapter and runtime DLL belong in the same RED4ext plugin folder because
the adapter loads the runtime adjacent to itself. NeuralDeck `.reds` belongs
under `r6/scripts`, where Redscript discovers and compiles it.

## Deployment state

`tools\Install-LocalGame.ps1` is the only deployment route used here. It:

1. refuses to copy while `Cyberpunk2077.exe` is running;
2. copies the adapter DLL, host-runtime DLL, Lisp scenarios and staged
   NeuralDeck Redscript to the paths above;
3. verifies SHA-256 hashes of both copied DLLs.

At the time of this record, the installed runtime and NeuralDeck Redscript
match their staged build artifacts. The installed adapter DLL predates the
current logging commit `e8b22e2`; the deployment test intentionally reports
that mismatch with `-RequireFreshAdapter`. This is expected until the next
explicit installation of the rebuilt adapter.

## What was implemented

| Commit | Change | Evidence |
| --- | --- | --- |
| `66b0193`, `e4e4837`, `7245986` | checked Lisp evaluation boundary and recovery evidence | malformed forms are guarded before raw nucleus operations |
| `1dd8821` | lifecycle state reset at plugin unload | adapter state witness |
| `fcfc3b7` through `cd47e5e` | machine-readable host-operation authority and generated documentation | registry mutation/projection tests |
| `6f28629` | player handle lifecycle evidence | epoch and opaque-handle discipline documented |
| `9b9da5f` | fixed NeuralDeck UI RTTI lookup | engine identity is `gameuiGameSystemUI`, not source alias `UISystem` |
| `ae192f0` | recorded F10 incident and diagnostic model | exact failure stage preserved |
| `c5ef8fb` | Redscript presentation contract test | validates typed event, Codeware service, non-blocking popup and receiver |
| `45c6357` | verification boundary documentation | separates local proof from live proof |
| `e8b22e2` | richer runtime diagnostics | startup, ABI, source, lifecycle and F10 boundary logs |
| `2f10d31` | deployment and launch-evidence tests | read-only checks against the actual game tree and logs |

## Tests and their meaning

| Test | Invocation | What it proves |
| --- | --- | --- |
| Adapter suite | `ctest --test-dir adapter/build -C Release --output-on-failure` | 14 focused CMake/C++ tests: registry, Canon guard, queue, state, F10 bridge, UI contract and local REPL behavior |
| Deployment test | `tools\Test-LocalGameDeployment.ps1 -GameDir <game root>` | expected files are in the real installation |
| Fresh-payload guard | same command plus `-RequireFreshAdapter` | installed adapter equals current Release adapter |
| Launch-evidence test | `tools\Test-GameLaunchEvidence.ps1 -GameDir <game root>` | RED4ext, Codeware, Redscript and Lisp session all completed their startup chain |
| F10 delivery test | same command plus `-RequireF10` | F10 reached the current adapter and produced a diagnostic record |

The first three checks can run with the game closed. The launch checks read
the latest logs after a game run. None starts the game or modifies its files.

## Latest verified evidence

The latest existing game run established all of the following:

- Cyberpunk 2077: 2.31;
- RED4ext: 1.30.0;
- Codeware: 1.20.3;
- Codeware initialized;
- Redscript compiled `NeuralDeckOverlay.reds` successfully;
- RED4ext loaded `my-lisp-cyberpunk`;
- the host Lisp session reached `session ready`;
- an earlier F10 edge reached the adapter and isolated the old failure to the
  incorrect UI RTTI class lookup.

The next live run must use the rebuilt adapter, then the launch-evidence test
with `-RequireF10` and a visible popup observation will close the remaining
F10 proof gap.

## Related documentation

- [Install procedure](install-red4ext-plugin.md)
- [Game verification runbook](game-verification-runbook.md)
- [F10 incident](neuraldeck-f10-incident-2026-09-14.md)
- [NeuralDeck verification boundary](neuraldeck-verification-2026-09-14.md)
- [NeuralDeck product contract](neuraldeck-repl.md)
