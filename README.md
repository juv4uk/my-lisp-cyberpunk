# my-lisp-cyberpunk

Статус: RED4ext host adapter живе в [`adapter/`](adapter/). Він зібраний
проти pinned RED4ext SDK, завантажує host-neutral WSM runtime DLL, створює
Lisp-сесію та виконує один read-only вертикальний зріз через RED4ext log.

**Мета:** REPL / скрипти my-lisp у Cyberpunk 2077 (аналогія CET/Lua, шлях RED4ext).

## Host runtime (міграція 2026-09-11, Phase C+D завершені)

**Канонічне й єдине місце** Windows embed runtime: [`host-runtime/`](host-runtime/) —
реальні закомічені джерела (`eval.rs`/`reader.rs`/`printer.rs`/`word.rs`/`ffi.rs`/
`asm/nucleus-win64.s`/`build.rs`), не sync-скрипт.

Раніше: `wsm-my-lisp/dll/` — видалено з їхнього боку (Phase D, коміт `1e1549a`).
Self-hosting SysV nucleus **не** переїжджав — він і далі в `wsm-my-lisp`.

```bash
cd host-runtime && git submodule update --init external/my-lisp
cargo test --target x86_64-pc-windows-msvc
```

Див. [`host-runtime/MIGRATION.md`](host-runtime/MIGRATION.md) /
[`host-runtime/VENDOR.md`](host-runtime/VENDOR.md).

## Дисципліна

- v0 = **fixed host-dispatch**, без closures/callback-реєстрації.
- Цей репозиторій володіє Cyberpunk-specific RED4ext кодом **і** host-runtime DLL.
- Семантика мови — лише **[my-lisp](https://github.com/juv4uk/my-lisp)**.
- Self-hosting Lisp→asm — **[wsm-my-lisp](https://github.com/juv4uk/wsm-my-lisp)** (не Rust eval).
- Компіляція офлайн — **[cml](https://github.com/juv4uk/cml)**.

## Задачі

Див. `tasks.my`.
