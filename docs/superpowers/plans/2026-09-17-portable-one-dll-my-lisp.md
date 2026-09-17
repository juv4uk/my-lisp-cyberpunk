# Portable one-DLL Canonical my-lisp Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a portable Cyberpunk 2077 mod whose only installed runtime artifact is `version.dll`, with the canonical `my-lisp` runtime statically embedded and no external Lisp/runtime sidecars.

**Architecture:** Upstream `juv4uk/my-lisp` keeps one canonical `my-lisp-embed` C ABI but emits `staticlib` in addition to its current `cdylib`/`rlib`. Cyberpunk links that static library into the existing C++ `version.dll`, removes dynamic loading of `my_lisp_embed.dll`, forwards directly to the absolute System32 `version.dll`, and compiles exact my-lisp provenance into the bridge from the pinned gitlink. Existing persistent-Session, reconnect, error-recovery, shutdown and conformance witnesses remain the acceptance surface.

**Tech Stack:** Rust/Cargo (`my-lisp`, `my-lisp-embed`), C ABI v4, MSVC/CMake/C++17, PowerShell, GitHub Actions, Windows `version.dll` proxying, existing LocalReplServer/RequestQueue.

**Spec:** `docs/superpowers/specs/2026-09-17-portable-one-dll-my-lisp-design.md`

## Global Constraints

- Final installed mod runtime contains exactly one mod file: `Cyberpunk 2077/bin/x64/version.dll`.
- No runtime dependency on `my_lisp_embed.dll`, `my-lisp.exe`, `version-original.dll`, provenance sidecar, RED4ext, CET, or Codeware.
- `my_lisp_embed.h` remains the only C embedding contract and ABI v4 remains authoritative unless an independently justified upstream ABI change is required.
- No parser, evaluator, Canon table, or semantic registry is copied into `my-lisp-cyberpunk`.
- Exact my-lisp gitlink SHA and accepted embed ABI remain machine-readable from runtime evidence.
- `DllMain` remains loader-lock-safe; Lisp evaluation stays on the existing bridge-owned worker thread.
- No silent fallback to dynamic `LoadLibrary(my_lisp_embed.dll)` if static linking fails.
- PR #46 consumer conformance remains a CI/test oracle concern; `my-lisp.exe` must not become a shipped runtime dependency.

---

### Task 1: Upstream canonical staticlib contract (`my-lisp#311`)

**Files:**
- Modify: `juv4uk/my-lisp/crates/my-lisp-embed/Cargo.toml`
- Create: `juv4uk/my-lisp/crates/my-lisp-embed/tests/static_link_consumer.rs` or the nearest existing native-consumer test location if the repo already has one
- Create/Modify: Windows CI workflow only if the existing workspace CI does not exercise the static native consumer

**Interfaces:**
- Consumes: existing `include/my_lisp_embed.h` and `my_lisp_embed_*` exports
- Produces: a Windows/MSVC static library from the same `my-lisp-embed` crate and source revision; C ABI remains unchanged

- [ ] **Step 1: Write the failing static-link witness**

The witness must compile/link a tiny native consumer against the generated static library and existing header, then execute:

```c
if (my_lisp_embed_abi_version() != MY_LISP_EMBED_ABI_VERSION) return 10;
MyLispEmbedSession *session = my_lisp_embed_session_new();
if (!session) return 11;
char *first = my_lisp_embed_eval(session, "(визначити portable-proof 42)");
if (!first) return 12;
my_lisp_embed_free_string(first);
char *second = my_lisp_embed_eval(session, "portable-proof");
if (!second || strcmp(second, "42") != 0) return 13;
my_lisp_embed_free_string(second);
my_lisp_embed_session_free(session);
return 0;
```

- [ ] **Step 2: Run the witness and verify RED**

Run the smallest existing Windows/native test lane. Expected failure: no consumable static `my-lisp-embed` artifact is produced from the current crate type list.

- [ ] **Step 3: Add the static artifact without changing semantics**

Change exactly:

```toml
[lib]
crate-type = ["cdylib", "staticlib", "rlib"]
```

Do not add a second header, wrapper API, or Cyberpunk-specific symbol.

- [ ] **Step 4: Run upstream embed tests and static consumer**

Run:

```text
cargo test --release -p my-lisp-embed
cargo build --release -p my-lisp-embed
```

and the real Windows/MSVC static consumer. Expected: existing embed tests GREEN, static consumer exit 0, dynamic artifact still produced.

- [ ] **Step 5: Commit and open upstream PR**

Commit message:

```text
feat(embed): emit canonical staticlib alongside cdylib (#311)
```

PR must reference `my-lisp#311` and state exact tested commit SHA.

---

### Task 2: Re-pin Cyberpunk to the merged upstream staticlib revision

**Files:**
- Modify gitlinks: `runtime/my-lisp`, `host-runtime/external/my-lisp`
- Modify only if necessary: `tools/tests/RuntimeMyLispDependencyContractTest.ps1`

**Interfaces:**
- Consumes: merged upstream commit from Task 1
- Produces: both Cyberpunk my-lisp gitlinks equal the same exact SHA that emits the static library

- [ ] **Step 1: Extend/confirm split-pin RED contract**

The existing dependency contract must require:

```text
runtime/my-lisp SHA == host-runtime/external/my-lisp SHA == selected staticlib-capable upstream SHA
```

No branch name or floating `main` is acceptable.

- [ ] **Step 2: Move both gitlinks to the exact merged upstream SHA**

No `.gitmodules` URL changes and no second checkout.

- [ ] **Step 3: Run current runtime and host verification gates**

Expected: canonical CLI/embed tests, host runtime, and current dynamic #45 bridge witnesses remain GREEN before any consumer linkage change.

- [ ] **Step 4: Commit**

```text
build(my-lisp): pin portable static embed revision
```

---

### Task 3: Build contract emits and discovers the static library from the pinned tree

**Files:**
- Modify: `tools/build-my-lisp-runtime.ps1`
- Test/Modify: existing runtime dependency/build contract PowerShell test

**Interfaces:**
- Consumes: pinned `runtime/my-lisp` from Task 2
- Produces: exact paths for CLI (test oracle), dynamic embed (compatibility test), static embed (bridge link), plus build-time provenance values

- [ ] **Step 1: Write RED for missing static artifact**

The build contract must fail if the static library is absent after `cargo build --release -p my-lisp-embed`.

Do not hard-code an unverified artifact filename before observing Cargo/MSVC output; locate the actual static library produced by the pinned build and fail on zero or multiple candidates.

- [ ] **Step 2: Build from the single pinned source tree**

Retain:

```powershell
cargo test --release -p my-lisp-embed
cargo build --release -p my-lisp-cli --bin my-lisp
cargo build --release -p my-lisp-embed
```

Then resolve the static artifact deterministically from that build.

- [ ] **Step 3: Preserve provenance as build data, not a runtime sidecar contract**

Resolve:

```text
my-lisp-sha=<git -C runtime/my-lisp rev-parse HEAD>
embed-abi=<MY_LISP_EMBED_ABI_VERSION from upstream header>
linkage=static
```

The script may still emit a CI provenance file for tests/debugging, but later runtime startup must not require that file.

- [ ] **Step 4: GREEN the runtime build contract**

Expected: CLI, cdylib and staticlib all originate from one pinned source revision; no network clone/floating branch is used.

- [ ] **Step 5: Commit**

```text
build(bridge): expose pinned static my-lisp artifact
```

---

### Task 4: Statically link canonical my-lisp into `version.dll`

**Files:**
- Modify: `bridge/CMakeLists.txt`
- Modify: `bridge/src/CanonicalReplHost.hpp`
- Modify: `bridge/src/CanonicalReplHost.cpp`
- Test: `bridge/tests/LoadTest.cpp`

**Interfaces:**
- Consumes: static library path and existing `my_lisp_embed.h`
- Produces: `version.dll` whose C++ host calls the same `my_lisp_embed_*` symbols directly, with no `LoadLibrary/GetProcAddress/FreeLibrary` for embed

- [ ] **Step 1: Write RED: bridge must work with adjacent embed DLL removed**

Update staging/harness so `my_lisp_embed.dll` is deliberately absent before `LoadLibrary(version.dll)`. Expected current behavior: canonical REPL fails to appear.

- [ ] **Step 2: Link static library in CMake**

Pass the exact static artifact into CMake through an explicit cache variable, for example:

```cmake
set(MY_LISP_EMBED_STATIC "" CACHE FILEPATH "Pinned canonical my-lisp-embed static library")
if(NOT EXISTS "${MY_LISP_EMBED_STATIC}")
  message(FATAL_ERROR "MY_LISP_EMBED_STATIC must name the pinned canonical static library")
endif()
target_link_libraries(version PRIVATE "${MY_LISP_EMBED_STATIC}" ws2_32)
```

Any additional native Rust/MSVC system libraries must be derived from the actual linker failure/output and documented in CMake; do not add speculative dependencies.

- [ ] **Step 3: Replace dynamic symbol loading with direct ABI calls**

`CanonicalReplHost::Start` becomes direct:

```cpp
m_abiVersion = my_lisp_embed_abi_version();
if (m_abiVersion != MY_LISP_EMBED_ABI_VERSION) return false;
m_session = my_lisp_embed_session_new();
return m_session != nullptr;
```

`Evaluate` directly calls `my_lisp_embed_eval` / `my_lisp_embed_free_string`; `Stop` directly calls `my_lisp_embed_session_free`. Remove embed `HMODULE` and function-pointer members. Preserve one-thread Session ownership.

- [ ] **Step 4: Run existing #45 lifecycle/REPL harness unchanged in meaning**

Expected: Ukrainian definition/closure, reconnect persistence, error recovery, early shutdown and normal shutdown all remain GREEN with no adjacent embed DLL.

- [ ] **Step 5: Inspect Windows imports**

Use runner-provided `dumpbin /DEPENDENTS` or an existing equivalent. Fail if output contains `my_lisp_embed.dll`.

- [ ] **Step 6: Commit**

```text
feat(bridge): statically embed canonical my-lisp runtime (#47)
```

---

### Task 5: Make Windows forwarding truly one-file

**Files:**
- Modify: `bridge/src/BridgeDllMain.cpp`
- Modify: `bridge/tests/LoadTest.cpp`
- Modify: Windows verification staging workflow

**Interfaces:**
- Consumes: Windows System32 `version.dll`
- Produces: proxy forwarding that needs no adjacent `version-original.dll`

- [ ] **Step 1: RED by removing `version-original.dll` from staging**

Run forwarding witness with only built `version.dll` staged. Expected current forwarding call fails.

- [ ] **Step 2: Resolve the real system DLL by absolute System32 path**

Use the Windows system-directory API rather than module-adjacent lookup, conceptually:

```cpp
wchar_t systemDir[MAX_PATH] = {};
UINT len = GetSystemDirectoryW(systemDir, MAX_PATH);
if (len == 0 || len >= MAX_PATH) return nullptr;
std::wstring path(systemDir, len);
path += L"\\version.dll";
g_realVersionDll = LoadLibraryW(path.c_str());
```

Do not use a relative name that could recurse into the proxy itself.

- [ ] **Step 3: Re-run all version export forwarding witnesses**

Expected: forwarding succeeds with no adjacent real DLL copy.

- [ ] **Step 4: Commit**

```text
fix(bridge): forward version API to absolute System32 dll
```

---

### Task 6: Compile exact provenance into the one-file bridge

**Files:**
- Create: generated build header in bridge build directory, e.g. `generated/MyLispBuildProvenance.hpp`
- Modify: `bridge/CMakeLists.txt`
- Modify: `bridge/src/BridgeRuntime.cpp`
- Modify: Windows build/staging workflow
- Test: existing `bridge-load-test` provenance assertions

**Interfaces:**
- Consumes: exact gitlink SHA and upstream ABI resolved by build script
- Produces: runtime evidence `(bridge-runtime-provenance/1 ...)` without reading a sidecar

- [ ] **Step 1: RED: remove provenance sidecar from staging**

Expected current bridge fails startup/provenance acceptance because `ReadCanonicalProvenance` cannot find the adjacent file.

- [ ] **Step 2: Generate provenance header at configure/build time**

CMake receives exact values and writes only build facts, for example:

```cpp
#pragma once
#define CYBERPUNK_MY_LISP_SHA "<40-hex>"
#define CYBERPUNK_MY_LISP_LINKAGE "static"
```

ABI remains checked using `MY_LISP_EMBED_ABI_VERSION` and the directly linked `my_lisp_embed_abi_version()` result; do not duplicate a numeric ABI macro in the generated header.

- [ ] **Step 3: Remove runtime sidecar parsing**

Delete `ReadCanonicalProvenance`/file parsing. `RecordAcceptedCanonicalProvenance` writes exact compiled SHA, accepted ABI and linkage `static` directly into `bridge-observation.lisp`.

- [ ] **Step 4: Re-run canonical reader evidence**

Run existing:

```text
my-lisp.exe --oracle-check bridge-observation.lisp
```

Expected `(outcome valid)`, with exact SHA/ABI/static linkage present.

- [ ] **Step 5: Commit**

```text
feat(bridge): compile pinned my-lisp provenance into mod
```

---

### Task 7: One-file packaging and regression proof

**Files:**
- Modify: `.github/workflows/windows-host-verification.yml` (or exact existing Windows verification workflow path)
- Modify/Create: narrow packaging contract test under `tools/tests/`
- Update: `bridge/README.md` / current deployment doc only after evidence is GREEN

**Interfaces:**
- Consumes: completed static bridge from Tasks 4–6
- Produces: final one-file staging proof and reusable artifact for live-game #31/#24

- [ ] **Step 1: Create clean runtime staging directory**

Copy exactly:

```text
version.dll
```

No other mod runtime file is permitted.

- [ ] **Step 2: Run standalone production bridge witness from clean stage**

It must prove forwarding + canonical REPL + reconnect + error recovery + lifecycle + provenance.

- [ ] **Step 3: Run dependency inspection**

Fail if `version.dll` imports `my_lisp_embed.dll` or any forbidden mod framework DLL. Normal Windows/Rust runtime dependencies must be documented if dynamically required; prefer the MSVC/static Rust artifact behavior actually produced by the build rather than assumptions.

- [ ] **Step 4: Run #46 consumer conformance against the one-file bridge**

The test-only canonical `my-lisp.exe` may remain outside runtime staging. Expected: corpus parity still GREEN.

- [ ] **Step 5: Preserve legacy regression gates**

Run current host runtime / registry / dispatch tests. Their success is regression evidence only; they do not become production runtime dependencies.

- [ ] **Step 6: Update deployment documentation**

Document only after GREEN:

```text
Install: copy version.dll to Cyberpunk 2077/bin/x64/
Runtime dependencies shipped by this mod: none besides version.dll itself.
```

Keep the claim bounded until actual `Cyberpunk2077.exe` witness #31/#24 succeeds.

- [ ] **Step 7: Commit and open/update PR**

```text
feat(portable): ship canonical Lisp as one Cyberpunk DLL (#47)
```

PR must link #47 and upstream #311, include exact upstream SHA, CI runs, dependency inspection and list of runtime stage files.
