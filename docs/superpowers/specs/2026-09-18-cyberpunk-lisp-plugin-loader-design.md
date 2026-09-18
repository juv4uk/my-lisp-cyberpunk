# Cyberpunk Lisp Plugin Loader Design

Date: 2026-09-18

Issue: `#35 CP-LISP-PLUGIN-LOADER-1`

## Goal

Make ordinary Cyberpunk extensions canonical `.lisp` files evaluated in the already-existing one-file bridge's **single persistent my-lisp Session**. Loading plugins must not create another evaluator, Session, native gameplay branch, or third-party mod-runtime dependency.

## Existing mechanisms reused

- `CanonicalReplHost::Evaluate()` — the already-proven canonical `my_lisp_embed_eval` path.
- The bridge-owned worker thread — the only owner of the canonical Session.
- `bridge-observation.lisp` — existing canonical-readable runtime evidence.
- The `my-idea` plugin semantics already proven in the ecosystem: `init.lisp` first, then sorted `plugins/*.lisp`, isolated per-file failure, explicit reload rather than hidden watcher.
- Windows CNG SHA-256 — host mechanism only, used to identify exact plugin bytes.

## Layout v0

Relative to the installed `version.dll`:

```text
my-lisp/
  init.lisp
  plugins/
    a-first.lisp
    b-second.lisp
```

The directory is optional. If it does not exist, the portable one-file runtime behaves exactly as before.

Only regular, non-symlink files whose extension is exactly `.lisp` are admitted from `plugins/`. Discovery is non-recursive.

## Load order

1. `init.lisp`, if it is a regular non-symlink file.
2. Direct children of `plugins/` ending in `.lisp`, sorted lexically by filename.
3. Each file is read as exact bytes, SHA-256 is computed over those bytes, and the source is passed unchanged to `CanonicalReplHost::Evaluate()`.
4. A reply starting with canonical embed's `error:` transport marker records a failed plugin and loading continues.
5. Successful definitions remain in the same Session later exposed by the REPL.

No plugin is evaluated on a socket thread. Loading runs on the same bridge worker thread that created and later evaluates the Session.

## Evidence

The loader appends one ordinary Lisp form to `bridge-observation.lisp`:

```lisp
(plugin-load-report/1
  (root "my-lisp")
  (entries
    ((path "init.lisp") (sha256 "...") (status loaded))
    ((path "plugins/a.lisp") (sha256 "...") (status loaded))
    ((path "plugins/b.lisp") (sha256 "...") (status error))))
```

Paths are relative to the plugin root and rendered with `/`. Read failures remain explicit; when bytes cannot be read the hash is represented as `()` rather than invented.

The existing canonical `my-lisp --oracle-check` gate must continue accepting the complete observation file.

## Failure policy

- Missing plugin root: empty report, normal REPL startup.
- Broken Lisp plugin: report error, continue with later plugins, preserve Session.
- Unreadable file: report error, continue.
- Symlink/non-regular entry: not executed.
- Failure to write the load report: fail bridge startup; do not claim an unobservable plugin state.
- No automatic watcher or reload in v0. #38 owns explicit reload semantics.

## RED -> GREEN witness

The existing real `bridge-load-test` creates fixtures beside the staged bridge before the main `LoadLibrary`:

- `init.lisp` defines a shared base;
- `plugins/a-first.lisp` consumes the init definition;
- `plugins/b-broken.lisp` errors;
- `plugins/c-after.lisp` consumes the earlier good definition and defines a later value.

Current bridge RED: after load, REPL does not know those plugin definitions and no `plugin-load-report/1` exists.

GREEN requires:
- REPL reads init/plugin definitions from the same Session;
- later valid plugin loads after the broken one;
- report order is init → a → b → c;
- each readable file has the exact SHA-256 of fixture bytes;
- broken plugin status is `error`, good ones `loaded`;
- canonical reader accepts runtime evidence;
- all existing one-DLL, forwarding, reconnect, error-recovery, provenance and consumer-parity gates remain GREEN.

## Non-goals

- hot reload (#38);
- event hooks (#37);
- UI (#39);
- game RTTI/mutation;
- plugin package manager;
- recursive dependency resolution;
- plugin namespaces beyond ordinary canonical Lisp definitions;
- compatibility layers for RED4ext/CET/Codeware.

## Principle

A new ordinary mod should normally be a new `.lisp` source file evaluated by the same canonical Session, while native code remains filesystem/hash/thread/evidence mechanism only.
