# Cyberpunk Lisp Plugin Loader Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Load deterministic `.lisp` plugins into the one canonical in-process Cyberpunk Session and expose exact path/hash/status evidence.

**Architecture:** Reuse the existing one-file bridge worker thread and `CanonicalReplHost::Evaluate()`. A focused native loader performs bounded filesystem discovery and SHA-256 only; it evaluates sources sequentially in the existing Session before the REPL server opens and appends a canonical-readable `plugin-load-report/1`.

**Tech Stack:** C++17, Windows filesystem APIs/std::filesystem, Windows CNG SHA-256 (bcrypt), existing static `my-lisp-embed`, existing bridge LoadTest and Windows verification workflow.

**Spec:** `docs/superpowers/specs/2026-09-18-cyberpunk-lisp-plugin-loader-design.md`

## Global Constraints

- No second evaluator or Session.
- No RED4ext/CET/Codeware dependency.
- Plugin loading runs on the existing Session-owner thread.
- `init.lisp` precedes lexically sorted direct `plugins/*.lisp`.
- Broken plugin cannot destroy Session or prevent later plugins.
- Paths, hashes and statuses are runtime evidence, not semantic authority.
- Reload is out of scope and owned by #38.

---

### Task 1: RED in the real bridge harness

**Files:**
- Modify: `bridge/tests/LoadTest.cpp`

- [ ] Create `my-lisp/init.lisp` and three `plugins/*.lisp` fixtures beside the staged DLL after the early-shutdown witness and before the main `LoadLibrary`.
- [ ] Require plugin-created definitions through the existing REPL connection.
- [ ] Require `plugin-load-report/1` with init/a/b/c order, loaded/error statuses and known SHA-256 values.
- [ ] Run Windows host verification. Expected RED: plugin definitions/report are absent.
- [ ] Commit only the failing witness.

### Task 2: Mechanism-only loader

**Files:**
- Create: `bridge/src/LispPluginLoader.hpp`
- Create: `bridge/src/LispPluginLoader.cpp`
- Modify: `bridge/CMakeLists.txt`
- Modify: `bridge/src/BridgeRuntime.cpp`

**Interfaces:**
- Consumes: `CanonicalReplHost&`, owner `HMODULE`
- Produces: deterministic load records and `plugin-load-report/1`

- [ ] Discover optional adjacent `my-lisp/init.lisp` then direct sorted `my-lisp/plugins/*.lisp`.
- [ ] Reject symlink/non-regular entries and non-`.lisp` extensions.
- [ ] Read exact bytes and compute SHA-256 with Windows CNG; link `bcrypt`.
- [ ] Evaluate each readable source through `CanonicalReplHost::Evaluate()` on the bridge worker thread.
- [ ] Classify only the existing embed transport marker `error:` as failed evaluation; continue after failure.
- [ ] Append one escaped canonical Lisp report to the existing observation file.
- [ ] If report cannot be written, fail startup rather than claim unobservable state.
- [ ] Run unchanged RED witness to GREEN and commit.

### Task 3: Canonical-evidence and regression gate

**Files:**
- Modify only if necessary: `.github/workflows/windows-host-verification.yml`

- [ ] Run existing canonical `my-lisp --oracle-check bridge-observation.lisp`; expected `outcome valid`.
- [ ] Re-run one-file PE dependency inspection; bcrypt is a Windows system dependency and no third-party framework may appear.
- [ ] Re-run persistent Session/reconnect/error/shutdown/provenance witness.
- [ ] Re-run consumer conformance and historical adapter regression gates.
- [ ] Update #35/PR with exact RED and GREEN run IDs; do not close #35 unless every acceptance item except dedicated reload semantics is satisfied.
