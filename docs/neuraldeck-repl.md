# NeuralDeck: in-game my-lisp REPL

## Product decision

NeuralDeck is the primary REPL surface for Cyberpunk 2077. It is an in-game,
Cyberpunk-styled terminal, toggled by `F10`. It is not a TCP client and does
not create a second Lisp session.

```text
hotkey
  -> NeuralDeck ink overlay
  -> input buffer + history
  -> canonical game-thread my-lisp Session
  -> result/error transcript
  -> NeuralDeck output panel
```

The overlay is presentation only. It never parses, evaluates, selects
capabilities, or interprets game policy. Every submitted form goes directly
to the one host-configured canonical session. Thus a definition entered into
NeuralDeck is visible to the loaded dispatcher and later NeuralDeck commands.

## Interaction v0

| Input | Meaning |
| --- | --- |
| F10 | open or close NeuralDeck; the game continues running |
| Enter | submit the current complete UTF-8 form |
| Up / Down | move through local command history |
| Tab | remains a game key; it never closes NeuralDeck |
| Ctrl+L | clear visible transcript only |

The first visual composition is intentionally small: a translucent dark panel,
Cyberpunk-red frame accents, a `my-lisp // neuraldeck` header and an explicit
status line. It uses the game’s ink UI path and is deliberately non-blocking:
it does not set time dilation, open a radial-menu context, take the cursor, or
consume `Tab`. The canonical input and transcript controls follow once the
canonical session bridge is available.

## Thread and authority rules

- The hotkey and ink events may alter only UI state: open, focus, text buffer,
  cursor and history selection.
- Enter appends one pending source form to an in-process queue.
- `Running` drains that queue and evaluates with the canonical session on the
  game thread.
- Evaluation emits a structured transcript record: source, result or error,
  and monotonic sequence number.
- The UI reads transcript records on the game thread and renders them. It does
  not call the evaluator itself.
- Redscript/Codeware supplies the F10 input and ink presentation; C++/RED4ext
  supplies only the host mechanisms and game-thread Lisp boundary. `.lisp`
  chooses all game behavior after a form is evaluated.

## Delivery order

1. Finish canonical opaque GameHandle capability so the canonical session can
   retain the existing `(клас гравець)` scenario.
2. Switch the adapter from the fixed-dispatch runtime to that canonical
   session; prove definitions and host mechanisms in one game session.
3. Add the in-process `NeuralDeckQueue` and transcript model. This replaces
   TCP as the primary interactive transport.
4. Add a testable F10 toggle lifecycle. **In progress:** Codeware's
   `CallbackSystem` delivers `Input/Key` directly to `NeuralDeckService`;
   C++ does not invoke compiled Redscript UI functions or poll the keyboard.
5. Add the non-blocking ink overlay. **In progress:** `CustomPopup` provides
   the panel; live F10 evidence is required before this step is complete.
6. Capture a live game witness: open, define, call the definition, inspect the
   player class, submit malformed source, and continue successfully.

The existing loopback TCP server remains an optional development diagnostic.
It is not required for the player-facing REPL and may be disabled in release
configuration.
