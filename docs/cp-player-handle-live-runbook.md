# CP-PLAYER-HANDLE-LIVE — Runbook

**Мета:** отримати один живий transcript у грі, який доводить, що
`(гравець-присутній?)` повертає `t`, adapter утримує opaque token і
прив'язує його як `гравець`.

Це **єдиний** критерій прийняття задачі. Без цього transcript
наступні задачі (клас, позиція, child-walk) не стартують.

## Передумови

1. Зібраний `my-lisp-cyberpunk-plugin.dll` (Release x64).
2. Поруч з ним лежить `wsm_my_lisp_cyberpunk_dll.dll` з `wsm-my-lisp`.
3. RED4ext встановлений, Cyberpunk 2077 (перевірено на v2.31).
4. `scripts/диспетчер.мій` лежить у тій самій директорії, що й плагін
   (або в `scripts/` відносно DLL, як зараз).

Див. також `docs/install-red4ext-plugin.md`.

## Що має статися

1. При `Load` — `(quote ())` проходить oracle-перевірку → `()`.
2. Кожен `Running` tick виконує `диспетчер.мій`.
3. Коли гравець з'являється:
   - `(гравець-присутній?)` повертає `t`;
   - C++ робить `Retain` + `wsm_wrap_game_handle` + `wsm_bind("гравець", ...)`;
   - у log з'являється рядок з **nonzero token**;
   - сценарій викликає `(запиши-лог)` (один раз, завдяки dedup у C++).

## Очікувані рядки логу (мінімальний acceptance)

```text
my-lisp-cyberpunk: (quote ()) => ()
my-lisp-cyberpunk: wsm_my_lisp_cyberpunk_dll.dll loaded, session=...
my-lisp-cyberpunk: Lisp host primitive гравець-присутній? invoked, present=true
my-lisp-cyberpunk: retained player as opaque token=<nonzero>
my-lisp-cyberpunk: Lisp host primitive запиши-лог invoked
my-lisp-cyberpunk: Lisp dispatch => ()
```

Достатньо **одного** сеансу, де всі ці рядки присутні (порядок може
трохи відрізнятися, але `token=<nonzero>` і `present=true` обов'язкові).

## Як фіксувати доказ

1. Запустити гру, зайти в світ (щоб PlayerPuppet існував).
2. Скопіювати фрагмент RED4ext log (або весь файл).
3. Вставити в issue або в `docs/evidence/` (наприклад
   `docs/evidence/player-handle-live-YYYY-MM-DD.txt`).
4. Закрити задачу `CP-PLAYER-HANDLE-LIVE` у `tasks.my` (`done . t`)
   і додати посилання на evidence.

## Що НЕ є частиною цієї задачі

- Читання класу / позиції / будь-яких properties.
- Mutating capabilities.
- Closures / event callbacks.
- Зміна `диспетчер.мій` на щось складніше (він уже правильний).

## Поточний диспетчер (на 2026-09-11)

```lisp
(cond
  ((гравець-присутній?) (запиши-лог))
  (t (quote ())))
```

Цього достатньо: Lisp сам вирішує, коли кликати log, C++ лише
виконує примітиви і утримує handle.
