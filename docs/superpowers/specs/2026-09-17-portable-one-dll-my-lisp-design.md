# Portable one-DLL canonical my-lisp design

Date: 2026-09-17

Issues: `my-lisp-cyberpunk#47`, upstream `my-lisp#311`

## Goal

The production vanilla Cyberpunk mod is deployable as one runtime artifact:

```text
Cyberpunk 2077/bin/x64/version.dll
```

That DLL contains the canonical `my-lisp` evaluator/runtime statically. A player does not install or copy `my_lisp_embed.dll`, `my-lisp.exe`, Rust/Cargo/Git, CET, RED4ext, or Codeware.

Portability changes linkage and packaging only. It must not create a second evaluator, parser, semantic registry, language contract, or Cyberpunk-owned copy of Lisp semantics.

## Current proven boundary

PR #45 proves the semantic/lifecycle shape with dynamic embedding:

```text
Cyberpunk/standalone process
        |
        v
version.dll
  |- Windows version.dll forwarding
  |- LocalReplServer / RequestQueue
  `- CanonicalReplHost
        |
        +-- LoadLibrary(my_lisp_embed.dll)
        +-- ABI v4 check
        +-- one persistent canonical Session
        `-- my_lisp_embed_eval(...)
```

The same bridge witness already proves:

- one persistent canonical Session;
- Ukrainian definition and closure persistence;
- state survival across TCP reconnects;
- language error followed by successful recovery;
- bounded early shutdown and clean lifecycle;
- exact pinned my-lisp SHA + accepted embed ABI provenance;
- canonical parsing of runtime evidence.

The one-DLL work preserves those claims and removes only the adjacent runtime DLL dependency.

## Selected approach

### Upstream: add `staticlib` to `my-lisp-embed`

The existing crate remains the canonical embedding boundary:

```toml
[lib]
crate-type = ["cdylib", "staticlib", "rlib"]
```

The existing `include/my_lisp_embed.h` remains the only C contract. The exported `my_lisp_embed_*` functions and `MY_LISP_EMBED_ABI_VERSION` remain authoritative.

No Cyberpunk-specific API or policy enters upstream `my-lisp`.

### Consumer: link the static library into `version.dll`

Cyberpunk CMake consumes the static library built from its exact pinned `runtime/my-lisp` gitlink. `CanonicalReplHost` stops dynamically loading/resolving `my_lisp_embed.dll`; instead it calls the same declared C ABI symbols directly.

Target runtime:

```text
Cyberpunk2077.exe
        |
        v
version.dll
  |- system version.dll forwarding
  |- bridge lifecycle
  |- existing LocalReplServer / RequestQueue
  `- statically linked my-lisp-embed
       |- canonical parser
       |- canonical evaluator
       |- canonical core library
       `- one persistent Session
```

## Why not the alternatives

### Bundled two-file runtime

Keeping `version.dll + my_lisp_embed.dll` is already functional but does not satisfy the owner's portability requirement. It remains useful only as historical/transition evidence.

### Rewrite the whole bridge in Rust

A Rust `cdylib` could also contain `my-lisp` directly, but replacing the already-proven C++ `version.dll` forwarding/lifecycle path would enlarge scope without improving semantic authority. The bridge is already mechanism-only; static linking is the smallest architectural change.

## Authority and provenance

The source of truth stays the pinned `runtime/my-lisp` gitlink.

Build evidence must retain:

```text
my-lisp-sha=<40-hex gitlink SHA>
embed-abi=<numeric ABI from upstream header/runtime contract>
linkage=static
```

No independent version number or semantic manifest is introduced in Cyberpunk.

The final runtime observation must still report the same exact SHA and ABI that built the embedded code.

## Build flow

```text
runtime/my-lisp @ exact gitlink
        |
        +-- cargo test -p my-lisp-embed
        |
        +-- cargo build --release -p my-lisp-embed
        |      |- dynamic artifact (kept for upstream compatibility tests)
        |      `- static library artifact
        |
        v
bridge/CMake
        |
        +-- compile bridge C++
        +-- link my-lisp-embed static library
        +-- link required Windows/Rust native dependencies
        v
version.dll
```

Artifact discovery must be based on actual Cargo output/build evidence, not a permanently guessed filename if Rust's MSVC output naming differs between crate types.

## Runtime flow

`DllMain` remains minimal and does not evaluate Lisp under loader lock.

The existing bridge-owned worker thread remains the Session owner. On that thread:

1. call `my_lisp_embed_abi_version()` directly;
2. verify it equals the upstream header constant;
3. call `my_lisp_embed_session_new()`;
4. start the existing loopback REPL;
5. serialize all `my_lisp_embed_eval()` calls on that owner thread;
6. on controlled shutdown, stop transport then call `my_lisp_embed_session_free()` exactly once.

The removal of `LoadLibrary/GetProcAddress/FreeLibrary` for the embed runtime must not change Session ownership or shutdown ordering.

## RED -> GREEN evidence

### Upstream RED

At the pinned/current upstream state, `my-lisp-embed` has no `staticlib` crate type and therefore cannot be linked directly into a native consumer.

### Upstream GREEN

A native Windows/MSVC witness using the existing header links the generated static library and proves:

- `my_lisp_embed_abi_version()`;
- `session_new`;
- definition persistence;
- closure persistence or equivalent existing session witness;
- `session_free`;
- existing dynamic/rlib tests stay GREEN.

### Cyberpunk RED

The existing bridge currently fails when `my_lisp_embed.dll` is absent beside `version.dll`.

### Cyberpunk GREEN

Run the same bridge harness with **no adjacent `my_lisp_embed.dll` and no `my-lisp.exe`**. It must still prove:

- `LoadLibrary(version.dll)` succeeds;
- Windows version forwarding succeeds;
- REPL appears on loopback;
- canonical Ukrainian definition/closure persists across reconnects;
- language error does not destroy the Session;
- controlled early and normal shutdown succeed;
- provenance reports exact SHA + ABI + static linkage;
- canonical `--oracle-check` accepts runtime evidence.

Additionally inspect `version.dll` dependencies using an existing Windows tool (`dumpbin /DEPENDENTS` or equivalent runner-provided mechanism) and fail if `my_lisp_embed.dll` remains a dynamic dependency.

## Packaging contract

The final production runtime package for the mod contains exactly the mod binary:

```text
version.dll
```

Build/debug evidence files may exist in CI artifacts, but they are not runtime dependencies and are not installed into the game directory.

The system Windows `version.dll` is not packaged by the mod; the proxy continues forwarding to the real system implementation through the already-proven bridge mechanism.

## Compatibility

- `my-lisp-embed` keeps `cdylib` for existing consumers.
- `rlib` remains available.
- C ABI/header remain unchanged for this feature.
- PR #45's dynamic witness remains useful during transition but the final Cyberpunk runtime witness must exercise static linkage.
- PR #46 consumer conformance should run against the same one-file bridge after this change; its canonical oracle may still use `my-lisp.exe` in CI because that executable is a **test oracle**, not a shipped game dependency.

## Non-goals

This work does not:

- add game RTTI or mutation primitives;
- add UI/NeuralDeck;
- implement plugin loading or hot reload;
- rewrite Windows forwarding in Rust;
- remove historical RED4ext code from the repository;
- change Lisp semantics or Canon identities;
- make the semantic TCP oracle a runtime dependency of the game.

## Failure policy

Fail closed if:

- static embed ABI differs from the upstream header;
- the static library cannot be traced to the pinned gitlink;
- CMake accidentally links a different embed implementation;
- `version.dll` still requires `my_lisp_embed.dll` at runtime;
- the canonical Session/reconnect/error/lifecycle witnesses regress.

A failed static-link experiment must not be hidden by falling back silently to dynamic `LoadLibrary(my_lisp_embed.dll)`.

## Completion claim

Only after the static one-file artifact passes the standalone bridge witnesses may we claim:

> `version.dll` contains the canonical my-lisp runtime and is portable without an external Lisp runtime DLL.

Only after #31/#24 repeats that artifact inside the actual `Cyberpunk2077.exe` process may we claim the final production mod is proven in-game.
