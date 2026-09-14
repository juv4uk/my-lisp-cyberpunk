# NeuralDeck F10 — verification record, 2026-09-14

## Purpose

This record separates facts already verified from the one fact that still
requires a player-visible game run. It replaces neither the incident record
nor the live witness requirement.

## Verified local environment

The last installed game logs identify this supported combination:

| Component | Observed version |
| --- | --- |
| Cyberpunk 2077 | 2.31 |
| RED4ext | 1.30.0 |
| Codeware | 1.20.3 |
| NeuralDeck Redscript | compiled successfully at 2026-09-14 22:02:34 |

The RED4ext log confirms that both Codeware and `my-lisp-cyberpunk` loaded.
Codeware's own script implementation confirms that custom popup operations
use `UISystem.QueueEvent`, and that `PopupsManager` receives typed popup
events. This is the same presentation route selected for NeuralDeck.

## Verified adapter boundary

The old installed build reached the F10 edge detector and reported the first
failed boundary precisely:

```text
F10 NeuralDeck event rejected: UISystem class missing
```

The defect was corrected in `9b9da5f`: C++ now looks up the engine RTTI name
`gameuiGameSystemUI`, not the Redscript source alias `UISystem`.

The pinned RED4ext SDK's `ExecuteFunction` template constructs call arguments
as `StackArgs_t{ nullptr, &argument }`. `Red4extNeuralDeckEngine` passes the
owned `Handle<IScriptable>` for `ref<NeuralDeckToggleEvent>` using that exact
ABI convention. The handle remains alive for the call and is released only
after `QueueEvent` returns.

## Automated evidence

`ctest --test-dir adapter/build -C Release --output-on-failure` passed all
14 tests after commit `c5ef8fb`.

Relevant coverage:

| Test | Proves |
| --- | --- |
| `neuraldeck-bridge-witness` | F10 edge behavior, all adapter rejection stages, RTTI identity, event lifetime, and recovery after a failed press. |
| `neuraldeck-redscript-contract` | Codeware imports, typed toggle event, non-blocking `CustomPopup`, service, `PopupsManager` receiver, and popup action remain present in the packaged Redscript source. |
| `neuraldeck-state-witness` | F10 toggles only once per press; Tab never closes the deck. |
| `neuraldeck-repl-witness` | Queue and transcript model preserve UI-only ownership. |

The CMake Release build also stages `redscript/NeuralDeck/NeuralDeckOverlay.reds`
next to the adapter output. `tools/Install-LocalGame.ps1` copies that staged
directory into `r6/scripts`, refuses to deploy while Cyberpunk runs, and
hash-verifies both DLLs after copying.

## What is not yet proven

No local test can prove that the game rendered the popup after the corrected
DLL was loaded. The required live witness is a fresh plugin log containing:

```text
F10 NeuralDeck event submitted: QueueEvent call succeeded; UI receipt not yet confirmed
```

and a visible non-blocking NeuralDeck panel. Until both exist, F10 remains
**in progress**. This constraint prevents a build or source inspection from
being misreported as player-visible success.

## Next live test

1. Keep the current Release payload installed while the game is closed.
2. Start Cyberpunk 2077, load into a running save, and press F10 once.
3. Read the newest `red4ext/logs/my-lisp-cyberpunk-plugin-*.log`.
4. If the panel is absent, use the exact rejected stage in that log as the
   next defect boundary; do not guess or change unrelated code.

## Runtime log contract

The adapter logs state transitions and user-visible boundaries, never every
frame. A fresh launch should contain these useful records:

```text
WSM host ABI validated version=1 features=... required=...
loaded dispatch source file=... bytes=...
entered Running state; fixed dispatch bytes=...; NeuralDeck hotkey=F10
NeuralDeck F10 edge vk=121 event=NeuralDeckToggleEvent ui-rtti=gameuiGameSystemUI outcome=...
```

The final record is emitted once per physical F10 press. `outcome` identifies
the first failed boundary, or states that `UISystem.QueueEvent` accepted the
typed event. This keeps the log small while making a failed live run actionable.
