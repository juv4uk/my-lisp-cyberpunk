# Local REPL contract

## Goal

Expose one persistent `my-lisp` session already owned by the Cyberpunk host
as an interactive, local-only REPL. The REPL is a presentation and transport
surface; `my-lisp` remains the owner of source semantics, while RED4ext and
C++ provide capabilities and scheduling only.

## Scope of the first slice

- Bind only `127.0.0.1`; never a LAN interface.
- Accept one UTF-8 Lisp form per newline-delimited request.
- Return one newline-delimited UTF-8 result. The result is exactly the text
  returned by `wsm_eval_string`, including its existing `error:` rendering.
- Keep one session for the installed plugin lifetime. A `define` made through
  the REPL remains visible to later REPL requests and to `dispatcher.lisp`.
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
