# Portable one-DLL canonical my-lisp design

Date: 2026-09-17

Issues: `my-lisp-cyberpunk#47`, upstream `my-lisp#311`

## Goal

The production vanilla Cyberpunk mod is deployable as one runtime artifact:

```text
Cyberpunk 2077/bin/x64/version.dll
```

That DLL contains the canonical `my-lisp` evaluator/runtime statically. A player does not install or copy `my_lisp_embed.dll`, `my-lisp.exe`, `version-original.dll`, Rust/Cargo/Git, CET, RED4ext, or Codeware.

Portability changes linkage, forwarding, and provenance packaging only. It must not create a second evaluator, parser, semantic registry, language contract, or Cyberpunk-owned copy of Lisp semantics.

## Current proven boundary

PR #45 proves the semantic/lifecycle shape with dynamic embedding:

```text
Cyberpunk/standalone process
        |
        v
version.dll
  |- Windows version.dll forwarding via adjacent version-original.dll
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

The one-DLL work preserves those claims while removing all adjacent runtime dependencies.

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

### Forwarding: load the real system `version.dll` directly

The current proxy requires an adjacent copy named `version-original.dll`. A true one-file mod cannot keep that dependency.

The proxy will resolve the genuine Windows library by an **absolute System32 path** obtained from Windows itself (for example `GetSystemDirectoryW` + `version.dll`, optionally with the narrowest suitable `LoadLibraryExW` flags). It must never resolve through the normal application-directory search path, which would recurse back into the proxy.

The system DLL remains Windows-owned; it is never copied or packaged by the mod.

### Provenance: embed build facts into `version.dll`

The current bridge reads an adjacent `cyberpunk-my-lisp-provenance.txt`. A one-file runtime cannot require that file.

During build, the exact `runtime/my-lisp` gitlink SHA is injected into the bridge as generated build metadata/compile definition. The ABI remains checked against the existing upstream header and the actual linked `my_lisp_embed_abi_version()` result.

The generated provenance file may still exist as a **CI/build artifact**, but runtime startup must not read it.

Target runtime:

```text
Cyberpunk2077.exe
        |
        v
version.dll
  |- forwards to absolute %SystemRoot%/System32/version.dll
  |- bridge lifecycle
  |- embedded build provenance (exact my-lisp SHA)
  |- existing LocalReplServer / RequestQueue
  `- statically linked my-lisp-embed
       |- canonical parser
       |- canonical evaluator
       |- canonical core library
       `- one persistent Session
```

## Why not the alternatives

### Bundled multi-file runtime

Keeping `version.dll + my_lisp_embed.dll`, or `version.dll + version-original.dll`, is already workable but does not satisfy the owner's portability requirement. These remain useful only as historical/transition evidence.

### Rewrite the whole bridge in Rust

A Rust `cdylib` could also contain `my-lisp` directly, but replacing the already-proven C++ `version.dll` forwarding/lifecycle path would enlarge scope without improving semantic authority. The bridge is already mechanism-only; static linking plus direct System32 forwarding is the smallest architectural change.

## Authority and provenance

The source of truth stays the pinned `runtime/my-lisp` gitlink.

Build evidence must retain:

```text
my-lisp-sha=<40-hex gitlink SHA>
embed-abi=<numeric ABI from upstream header/runtime contract>
linkage=static
```

No independent version number or semantic manifest is introduced in Cyberpunk.

The exact SHA is compiled into the bridge from the gitlink at build time. At runtime the bridge records that compiled SHA together with the ABI actually returned by the statically linked embed implementation.

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
        +-- extract exact gitlink SHA for build metadata
        v
bridge/CMake
        |
        +-- compile bridge C++
        +-- inject exact my-lisp SHA build metadata
        +-- link my-lisp-embed static library
        +-- link required Windows/Rust native dependencies
        v
version.dll
```

Artifact discovery must be based on actual Cargo output/build evidence, not a permanently guessed filename if Rust's MSVC output naming differs between crate types.

## Runtime flow

`DllMain` remains minimal and does not evaluate Lisp under loader lock.

The existing bridge-owned worker thread remains the Session owner. On that thread:

1. call the statically linked `my_lisp_embed_abi_version()` directly;
2. verify it equals the upstream header constant;
3. record the compiled exact my-lisp SHA + accepted ABI as runtime evidence;
4. call `my_lisp_embed_session_new()`;
5. start the existing loopback REPL;
6. serialize all `my_lisp_embed_eval()` calls on that owner thread;
7. on controlled shutdown, stop transport then call `my_lisp_embed_session_free()` exactly once.

The removal of `LoadLibrary/GetProcAddress/FreeLibrary` for the embed runtime must not change Session ownership or shutdown ordering.

The version-proxy forwarding path separately loads only the absolute Windows System32 `version.dll`; it must never load an adjacent `version-original.dll`.

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

The existing bridge currently fails its full runtime contract if any of these adjacent files are absent:

```text
my_lisp_embed.dll
version-original.dll
cyberpunk-my-lisp-provenance.txt
```

That is the correct pre-change RED for the one-file requirement.

### Cyberpunk GREEN

Run the same bridge harness in a clean staging directory containing **only `version.dll` as the mod runtime artifact**. It must still prove:

- `LoadLibrary(version.dll)` succeeds;
- forwarding reaches the genuine System32 `version.dll` without an adjacent copy;
- REPL appears on loopback with no adjacent `my_lisp_embed.dll`;
- canonical Ukrainian definition/closure persists across reconnects;
- language error does not destroy the Session;
- controlled early and normal shutdown succeed;
- runtime evidence reports exact compiled SHA + actual ABI + static linkage;
- canonical `--oracle-check` accepts generated runtime evidence.

Additionally inspect `version.dll` dependencies using an existing Windows tool (`dumpbin /DEPENDENTS` or equivalent runner-provided mechanism) and fail if `my_lisp_embed.dll` remains a dynamic dependency.

A filesystem assertion must fail if the staged runtime package contains `my_lisp_embed.dll`, `version-original.dll`, or `my-lisp.exe`.

## Packaging contract

The final production runtime package for the mod contains exactly:

```text
version.dll
```

Runtime-generated observation/log files are outputs, not installation dependencies.

Build/debug/provenance files may exist in CI artifacts, but they are not installed into the game directory and are not required for startup.

The genuine Windows `version.dll` remains in `%SystemRoot%\System32` and is loaded by absolute path.

## Compatibility

- `my-lisp-embed` keeps `cdylib` for existing consumers.
- `rlib` remains available.
- C ABI/header remain unchanged for this feature.
- PR #45's dynamic witness remains useful as transition/history, but the final Cyberpunk runtime witness must exercise static linkage and direct System32 forwarding.
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
- startup still requires `version-original.dll` or a provenance sidecar;
- the absolute System32 forwarding target cannot be resolved;
- the canonical Session/reconnect/error/lifecycle witnesses regress.

A failed static-link experiment must not be hidden by falling back silently to dynamic `LoadLibrary(my_lisp_embed.dll)` or an adjacent `version-original.dll`.

## Completion claim

Only after the static one-file artifact passes the standalone bridge witnesses may we claim:

> `version.dll` contains the canonical my-lisp runtime and is portable without an external Lisp/runtime/forwarding sidecar.

Only after #31/#24 repeats that artifact inside the actual `Cyberpunk2077.exe` process may we claim the final production mod is proven in-game.
