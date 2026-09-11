# host-runtime — Windows WSM embed for Cyberpunk

**Canonical home of the host-embeddable runtime (2026-09-11 migration Phase B).**

Previously: `wsm-my-lisp/dll/`. That path is a **compatibility mirror** until
the adapter/CI switch fully here. New host/FFI work lands **here**.

## Authority

| Layer | Owner |
|-------|--------|
| Language semantics | my-lisp language-contract |
| ABI tags/words | wsm-target-contract |
| Self-hosted SysV nucleus growth | wsm-my-lisp `asm/` + `harness/` |
| **This crate** | **my-lisp-cyberpunk** — Windows session / FFI / reader / eval for in-game embed |

`wsm-my-lisp` stays Lisp-first; it does not grow new Lisp capabilities in Rust.
This directory is the allowed Cyberpunk embedding surface.

## Build

```bash
cd host-runtime
git submodule update --init external/my-lisp   # for build.rs canon spellings
cargo test --target x86_64-pc-windows-msvc
```

Cdylib export name remains `wsm_my_lisp_cyberpunk_dll` so existing
`LoadLibraryW` next to the RED4ext plugin keeps working.

## Layout

- `src/` — session, reader, eval, FFI
- `asm/nucleus-win64.s` — Win64 5 primitives (vendored; SysV authority stays in wsm-my-lisp)
- `external/my-lisp` — submodule for `semantic-registry.wsm` at build time

See [MIGRATION.md](MIGRATION.md).
