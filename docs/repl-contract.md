# Local REPL contract

## Goal

Expose the Cyberpunk host session through an interactive REPL. NeuralDeck,
the in-game ink overlay opened by a hotkey, is the primary product surface;
see [NeuralDeck](neuraldeck-repl.md). The loopback protocol below is an
optional development transport. `my-lisp` remains the owner of source
semantics, while RED4ext and C++ provide capabilities and scheduling only.

## Current semantic boundary

The current `host-runtime` is deliberately a fixed-dispatch evaluator. It has
`quote`, `cond`, host bindings and registered host primitives, but no Lisp
`def`/`визначити`, `let` or closures. The loopback transport therefore supports
interactive evaluation of the admitted host surface today, but it cannot yet
truthfully promise user-defined state across requests.

The full REPL milestone depends on adopting a canonical `my-lisp` session (or
an equivalently proven embedding surface supplied by `my-lisp`) rather than
adding ad-hoc definitions to this evaluator. No UI or transport code may claim
that semantic authority.

## Optional loopback development transport

- Bind only `127.0.0.1:40777`; never a LAN interface.
- Accept one UTF-8 Lisp form per newline-delimited request.
- Return one newline-delimited UTF-8 result. The result is exactly the text
  returned by `wsm_eval_string`, including its existing `error:` rendering.
- Keep one host session for the installed plugin lifetime. Once its evaluator
  becomes canonical, a `define` made through the REPL must remain visible to
  later REPL requests and to `dispatcher.lisp`.
- Evaluate only from the RED4ext `Running` update callback. The socket thread
  may receive bytes and enqueue requests, but it must never call the WSM ABI.
- Limit a request to 16 KiB and queue at most 32 pending requests. Reject a
  larger request or a full queue with a textual error; do not drop it silently.

## Why this is not the CLI TCP server copied into the game

`my-lisp-cli/src/tcp_repl.rs` is the behavioural precedent: loopback-only,
line-based input and a persistent session. It intentionally creates a fresh
canonical session per TCP connection. Cyberpunk already owns exactly one
host-configured session containing RED4ext capabilities and opaque handles,
so creating a second session would make game state and REPL state diverge.

The host runtime's Win64 WSM arena is process-global and unsynchronised.
Calling `wsm_eval_string` from a Winsock worker would race the game tick.
The queue boundary keeps all evaluation on the game thread.

## Protocol

```text
client → server:  (клас гравець)\n
server → client:  "PlayerPuppet"\n
```

Blank lines are ignored. The connection may issue multiple requests, in
order. A malformed form returns the normal Lisp error text and leaves the
session usable. Networking errors close only that client connection.

## Non-goals

- No in-game UI in this slice; `my-idea` or any local terminal can be a
  client later.
- No remote access, authentication scheme, callback registry, or plugin
  framework.
- No new Lisp syntax or host-specific semantic exception.
- No mutation capability beyond capabilities already registered by the host.

## Evidence required

1. A native queue test proves FIFO delivery, 16 KiB rejection and full-queue
   rejection without RED4ext or sockets.
2. A loopback integration witness sends two forms on one connection and proves
   state persists across them.
3. A live game transcript records the listener address, a successful result,
   and a malformed request whose later valid request still succeeds.
