# Vendor / sync status

**Phase C complete (2026-09-11):** sources copied and committed in this
tree; `scripts/sync-from-wsm-my-lisp.sh` deleted. This tree is now the sole
source for the host-runtime crate — no longer synced from `wsm-my-lisp`.

| Module | Status |
|--------|--------|
| `src/lib.rs` `src/eval.rs` `src/reader.rs` `src/printer.rs` `src/word.rs` `src/ffi.rs` | committed here |
| `Cargo.toml` / `Cargo.lock` | committed here |
| `README.md` / `MIGRATION.md` | committed here |
| `asm/nucleus-win64.s` | committed here |
| `build.rs` | committed here (registry path: `external/my-lisp/lib/surface/semantic-registry.wsm`) |
| `external/my-lisp` | git submodule, pinned `ccacc68ef6729a0973bdd1fc41906fc160127f89` (same commit `wsm-my-lisp/dll` had pinned at migration time, `4381e93`) |

**Verified:** `cargo test --target x86_64-pc-windows-msvc` — 55/55 green
(2026-09-11, this tree, after submodule init).

Future fixes/features to this runtime happen here, not in `wsm-my-lisp`
(which is frozen to bugfix-only per `wsm-my-lisp#15`, Phase A). If
`wsm-my-lisp/dll` is later removed or stubbed (Phase D), this file's
mirror-status note becomes moot — this was always meant to become the sole
copy, not a permanent duplicate.
