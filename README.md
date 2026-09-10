# my-lisp-cyberpunk

Статус: створено власником 2026-09-10; код продукту ще не тут.

**Мета:** REPL / скрипти my-lisp у Cyberpunk 2077 (аналогія CET/Lua, шлях RED4ext).

## Дисципліна

- Перші продуктові рішення — **власника**, не агента (`tasks.my`: `CP-OWNER-SCOPE-V0`).
- Семантика мови — лише **[my-lisp](https://github.com/juv4uk/my-lisp)**.
- In-process eval / DLL / plugin зараз у **[wsm-my-lisp](https://github.com/juv4uk/wsm-my-lisp)** (`dll/`, `plugin/`).
- Компіляція офлайн — **[cml](https://github.com/juv4uk/cml)**; **не** runtime `eval_string` у cml.

## Відкриті питання власнику

1. Перший мод-скрипт: лише fixed dispatch (cond+def), чи одразу closures/callbacks?
2. Це репо — продукт (plugin+UI), чи тонка обгортка над wsm-my-lisp?

## Задачі

Див. `tasks.my`. Крос-репо рекомендації: cml `evidence/cyberpunk/CROSS-REPO-TASK-RECOMMENDATIONS-2026-09-10.my`.

## Споріднене

- [wsm-my-lisp](https://github.com/juv4uk/wsm-my-lisp) — asm nucleus, FFI, RED4ext skeleton
- [my-lisp docs/cyberpunk-paradigm-fit.md](https://github.com/juv4uk/my-lisp/blob/main/docs/cyberpunk-paradigm-fit.md)
- [cml dispatch proposal](https://github.com/juv4uk/cml/blob/master/docs/CYBERPUNK-DISPATCH-PROPOSAL-2026-09-10.md)
