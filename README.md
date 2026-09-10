# my-lisp-cyberpunk

Статус: RED4ext host adapter живе в [`adapter/`](adapter/). Він зібраний
проти pinned RED4ext SDK, завантажує host-neutral WSM runtime DLL, створює
Lisp-сесію та виконує один read-only вертикальний зріз через RED4ext log.

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

Canonical Ukrainian oracle: [`scripts/перший-зріз.my`](scripts/%D0%BF%D0%B5%D1%80%D1%88%D0%B8%D0%B9-%D0%B7%D1%80%D1%96%D0%B7.my).
Він виконується у `my-lisp` і повертає `()` — той самий результат, який
очікує адаптер після `(запиши-лог)`.

## Споріднене

- [wsm-my-lisp](https://github.com/juv4uk/wsm-my-lisp) — asm nucleus, FFI, RED4ext skeleton
- [my-lisp docs/cyberpunk-paradigm-fit.md](https://github.com/juv4uk/my-lisp/blob/main/docs/cyberpunk-paradigm-fit.md)
- [cml dispatch proposal](https://github.com/juv4uk/cml/blob/master/docs/CYBERPUNK-DISPATCH-PROPOSAL-2026-09-10.md)
