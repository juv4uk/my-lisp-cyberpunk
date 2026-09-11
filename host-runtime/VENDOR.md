# Vendor / sync status

| Module | Status |
|--------|--------|
| `src/lib.rs` | in-tree (path adapted) |
| `Cargo.toml` | in-tree |
| `README.md` / `MIGRATION.md` | in-tree |
| `eval.rs` `reader.rs` `printer.rs` `word.rs` `ffi.rs` | sync via `scripts/sync-from-wsm-my-lisp.sh` until committed |
| `asm/nucleus-win64.s` | sync script |
| `build.rs` | sync script (registry path adapted) |

**Pin:** `wsm-my-lisp` `4381e93f818edd41b6358720a0b4b83a1ed7bfb8`

```bash
bash host-runtime/scripts/sync-from-wsm-my-lisp.sh
```

After the first successful local sync + Windows test green, commit the
copied sources here and delete the sync script (Phase C complete).
