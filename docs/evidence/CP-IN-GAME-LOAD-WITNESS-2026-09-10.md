# CP-IN-GAME-LOAD-WITNESS — 2026-09-10

## Provenance

- Source: owner comment in closed GitHub issue `my-lisp-cyberpunk#1`.
- Captured environment: Cyberpunk 2077 v2.31; RED4ext v1.30.0.
- Scope: the first read-only `(запиши-лог)` vertical slice only.

The source issue remains historical context. This file preserves the
game-process evidence with the repository so it survives issue archival,
forks and external-service changes.

## Observed RED4ext log

```text
[RED4ext] Loading plugin from '...red4ext\plugins\wsm-my-lisp-cyberpunk-plugin\my-lisp-cyberpunk-plugin.dll'...
[RED4ext] my-lisp-cyberpunk (version: 0.1.0, author(s): juv4uk) has been loaded
[RED4ext] Loading plugin from '...wsm_my_lisp_cyberpunk_dll.dll'...
[RED4ext] 1 plugin(s) loaded
[RED4ext] RED4ext has been started
[my-lisp-cyberpunk] Lisp host primitive запиши-лог invoked
[my-lisp-cyberpunk] (запиши-лог) => ()
[my-lisp-cyberpunk] wsm_my_lisp_cyberpunk_dll.dll loaded, session=<redacted>
```

## What this proves

The observed path is:

```text
RED4ext → Cyberpunk adapter → wsm_session_init → register запиши-лог
→ Ukrainian Lisp expression → host primitive → RED4ext log
```

It proves that Lisp executed in the live game process and invoked the
read-only logging mechanism. It does not prove player-handle, `(клас ...)`,
world-position or any mutating capability. Those require their own captured
evidence artifacts.
