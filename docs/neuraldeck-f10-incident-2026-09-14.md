# NeuralDeck F10 incident — 2026-09-14

## Impact

NeuralDeck did not appear after F10 despite the plugin loading, Redscript
compiling, and the player capability working in a live game session.

## Actual failure

The adapter looked up the UI class as `UISystem`:

```cpp
rtti->GetClass("UISystem")
```

That is the Redscript source alias, not the REDengine RTTI identity. The
pinned RED4ext SDK defines the underlying class as `gameuiGameSystemUI`.
The live log recorded the exact rejected stage:

```text
2026-09-14 22:05:54  F10 NeuralDeck event rejected: UISystem class missing
```

The fixed adapter now uses `gameuiGameSystemUI` through
`neuraldeck::LookupUiSystemClass`.

The subsequent source, ABI, dependency and test verification is recorded in
[NeuralDeck verification record](neuraldeck-verification-2026-09-14.md).

## Errors made

1. The initial implementation treated a Redscript spelling as an RTTI class
   name without checking the generated RED4ext SDK alias.
2. Earlier F10 iterations were judged from a generic `not queued` log line.
   That hid whether the failure was input, RTTI, event allocation, method
   lookup, or event execution.
3. A current source build was assumed to be the game payload. The new log
   proved the deployed DLL still contained the old polling behavior until the
   installer copied and hash-verified the Release artifacts.

## Diagnostic model

Every in-game UI action is treated as a chain of independently observable
boundaries:

```text
keyboard edge
  -> adapter poll
  -> RTTI class lookup
  -> UISystem instance lookup
  -> event allocation
  -> QueueEvent execution
  -> Redscript receiver
  -> popup display
```

The adapter reports the first rejected boundary. A test double covers each
adapter-side rejection. A claim that the UI works requires a live log or
screenshot after the installed DLL, not only C++ tests or successful
Redscript compilation.

## Timeline

| Time, 2026-09-14 | Evidence | Meaning |
| --- | --- | --- |
| 15:58:21 | Redscript compilation complete | NeuralDeck source was syntactically accepted. |
| 16:03:04–16:03:06 | `F10 NeuralDeck event not queued` repeated | Old adapter lacked a useful rejection stage and edge handling. |
| 22:02:24 | New plugin loaded; local REPL and Lisp dispatch live | Installed payload loaded successfully. |
| 22:02:35 | Redscript compilation complete | Current NeuralDeck source loaded. |
| 22:05:54 | `rejected: UISystem class missing` | Diagnostic build isolated the RTTI lookup defect. |
| after diagnosis | SDK alias `gameuiGameSystemUI` confirmed; commit `9b9da5f` | Source fix and regression witness completed. |

## Prevention

- Use generated SDK identities for C++ RTTI lookups; never copy a Redscript
  alias as an engine class name without a source reference.
- Keep stage-specific diagnostics until a live UI receipt is captured.
- Install via `tools/Install-LocalGame.ps1`; it refuses a running game and
  verifies copied DLL hashes.
- Keep `adapter/tests/NeuralDeckBridgeWitness.cpp` green. Its RTTI probe
  accepts only `gameuiGameSystemUI`.
