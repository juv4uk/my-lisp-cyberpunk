# my-lisp-cyberpunk

Статус: RED4ext host adapter живе в [`adapter/`](adapter/). Він завантажує
host-neutral WSM runtime DLL, створює Lisp-сесію та виконує один
read-only вертикальний зріз через RED4ext log.

**Мета:** REPL / скрипти my-lisp у Cyberpunk 2077 (аналогія CET/Lua, шлях RED4ext).

## Дисципліна

- Власник визначив v0 як **fixed host-dispatch**, без closures та callback-реєстрації.
- Цей репозиторій є власником Cyberpunk-specific RED4ext/UI коду; він не дублює Lisp runtime.
- Перший вертикальний зріз викликає українську примітиву `(запиши-лог)` і пише результат у RED4ext log. Зміни збереження, інвентарю, телепортація та callbacks не входять у v0.
- Семантика мови — лише **[my-lisp](https://github.com/juv4uk/my-lisp)**.
- In-process eval / DLL лишаються у **[wsm-my-lisp](https://github.com/juv4uk/wsm-my-lisp)** (`dll/`).
- Компіляція офлайн — **[cml](https://github.com/juv4uk/cml)**; **не** runtime `eval_string` у cml.

## Задачі

Див. `tasks.my`. Крос-репо рекомендації: cml `evidence/cyberpunk/CROSS-REPO-TASK-RECOMMENDATIONS-2026-09-10.my`.

## Споріднене

- [wsm-my-lisp](https://github.com/juv4uk/wsm-my-lisp) — asm nucleus, FFI, RED4ext skeleton
- [my-lisp docs/cyberpunk-paradigm-fit.md](https://github.com/juv4uk/my-lisp/blob/main/docs/cyberpunk-paradigm-fit.md)
- [cml dispatch proposal](https://github.com/juv4uk/cml/blob/master/docs/CYBERPUNK-DISPATCH-PROPOSAL-2026-09-10.md)
