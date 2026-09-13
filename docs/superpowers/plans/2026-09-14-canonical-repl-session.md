# Canonical REPL Session Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Cyberpunk's temporary fixed-dispatch evaluator with one persistent, canonical `my-lisp` session that can serve the local REPL and game capabilities.

**Architecture:** Extend the upstream `my-lisp-embed` C ABI from v3 to v4 with exactly one typed unary mechanism: an opaque host handle of a declared kind maps to a copied UTF-8 result. `my-lisp` validates Lisp arity and handle kind before passing the opaque token to the host. Cyberpunk then adopts that ABI after a C++ witness proves `define`, error recovery, and `(клас гравець)` use one session.

**Tech Stack:** Rust (`my-lisp`, `my-lisp-embed`), C ABI, C++20, RED4ext, Winsock loopback queue.

**Spec:** `docs/canonical-embed-adoption.md`, `docs/repl-contract.md`, `tasks.lisp` (`CP-CANONICAL-REPL-SESSION`).

## Global Constraints

- `my-lisp` is the sole owner of reader, evaluator, Canon, definitions, closures, output, and errors.
- C++ supplies only atomic host mechanisms and executes them on the RED4ext game thread.
- The opaque token may cross only from a validated `HostHandle` into its matching C callback; it must never be printed or constructible from source.
- No callback registry, fake closure, Cyberpunk semantic exception, or second evaluator.
- ABI users must reject a version other than the documented v4.

---

### Task 1: Add canonical typed-unary embed ABI v4

**Files:**
- Modify: `C:/GitHub/my-lisp/crates/my-lisp-embed/include/my_lisp_embed.h`
- Modify: `C:/GitHub/my-lisp/crates/my-lisp-embed/src/lib.rs`

**Interface:**
```c
typedef int32_t (*MyLispEmbedUnaryHandleToUtf8Fn)(
    void *context, uint64_t token, const char **out_utf8);
int32_t my_lisp_embed_register_unary_handle_to_utf8(
    MyLispEmbedSession *session, const char *utf8_surface,
    const char *utf8_expected_kind, MyLispEmbedUnaryHandleToUtf8Fn callback,
    void *context);
```

- [ ] Write a failing Rust test binding `гравець` as `cyberpunk.IScriptable`, registering `клас`, and expecting `(клас гравець)` to return the callback's UTF-8 class name.
- [ ] Run `cargo test -p my-lisp-embed` and observe that the missing v4 API prevents the test from compiling.
- [ ] Implement registration by defining a canonical `Value::host_function`; enforce exactly one argument and the declared handle kind before calling C; copy valid UTF-8 into `Value::String`.
- [ ] Add negative tests for wrong arity, wrong handle kind, invalid UTF-8, and callback failure followed by a valid evaluation in the same session.
- [ ] Run `cargo test -p my-lisp-embed`; commit and push only the ABI files.

### Task 2: Adopt ABI v4 in Cyberpunk host-runtime and adapter

**Files:**
- Modify: `C:/GitHub/my-lisp-cyberpunk/host-runtime/external/my-lisp`
- Modify: `C:/GitHub/my-lisp-cyberpunk/adapter/src/Main.cpp`
- Modify: `C:/GitHub/my-lisp-cyberpunk/adapter/tests/*`

- [ ] Add a failing native witness that makes `(визначити repl-перевірка 42)`, then `repl-перевірка`, then `(клас гравець)` through one canonical session.
- [ ] Load the v4 DLL only after `my_lisp_embed_abi_version() == 4`; register `запиши-лог`, `гравець-присутній?`, bind `гравець`, and register typed `клас`.
- [ ] Route both loopback REPL and game dispatcher to the same canonical session on the game thread.
- [ ] Run adapter CTest, canonical my-lisp tests, and a live game transcript; commit and push the atomic migration.

### Task 3: Retire the fixed evaluator only after evidence

**Files:**
- Modify: `C:/GitHub/my-lisp-cyberpunk/tasks.lisp`
- Modify: `C:/GitHub/my-lisp-cyberpunk/docs/canonical-embed-adoption.md`
- Modify: `C:/GitHub/my-lisp-cyberpunk/docs/repl-contract.md`

- [ ] Preserve the live transcript showing persistent `визначити`, error recovery, and `(клас гравець)`.
- [ ] Remove the fixed evaluator from the active plugin path; retain it only if a documented build-only witness still needs it.
- [ ] Mark `CP-CANONICAL-REPL-SESSION` done only after the canonical live witness.
