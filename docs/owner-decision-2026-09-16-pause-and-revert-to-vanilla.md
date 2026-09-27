# Рішення власника: пауза, гра повернута у ванільний стан

Дата: 2026-09-16

## Рішення

Після трьох днів діагностики NeuralDeck/F10 (кілька справжніх крашів,
кожен виправлений з доказами, і останній нерозв'язаний баг у циклі
показу/приховування попапу) власник вирішив зупинитись і повернути
локальну гру у чистий ванільний стан.

Виконано двічі за цю сесію (спершу видалено все, потім власник попросив
поставити назад лише RED4ext + наш `my-lisp-cyberpunk` плагін для
перевірки мінімального шляху, потім видалено й це):

- прибрано RED4ext (`RED4ext.dll`, `bin\x64\winmm.dll`);
- прибрано Codeware, CET (`cyber_engine_tweaks`);
- прибрано наш плагін (`wsm-my-lisp-cyberpunk-plugin`) і `r6\scripts\NeuralDeck\`.

Гра зараз — повністю ванільна, без жодного мода.

## Що це НЕ означає

- Це не відмова від проєкту `my-lisp-cyberpunk`. Репозиторій на GitHub
  (`github.com/juv4uk/my-lisp-cyberpunk`) не зачеплений: увесь код, історія
  комітів, документи й тести лишаються на місці.
- Це не означає, що NeuralDeck остаточно провалився. Останній відомий
  стан: C++-крашів немає (виправлено), попап показується один раз при
  першому натисканні End, але механізм закриття/повторного відкриття
  через `Close()`/`OnHidden()` ще не до кінця зрозумілий (див.
  [`neuraldeck-close-reentrancy-2026-09-16.md`](neuraldeck-close-reentrancy-2026-09-16.md)
  і чесний список помилок у
  [`claude-session-mistakes-2026-09-15-16.md`](claude-session-mistakes-2026-09-15-16.md)).

## Наступний крок

Не визначено — це свідома пауза, не задача з конкретним acceptance.
Коли власник вирішить повернутись: RED4ext v1.30.0
(`WopsS/RED4ext`) + build `adapter/build/src/Release/` +
`host-runtime/target/release/wsm_my_lisp_cyberpunk_dll.dll` —
мінімальний шлях без Codeware/CET/NeuralDeck, перевірений робочим
цієї ж сесії (fixed-dispatch, `гравець-присутній?`), перед тим, як
знову підключати UI-шар.

## Пов'язані документи

- [`neuraldeck-close-reentrancy-2026-09-16.md`](neuraldeck-close-reentrancy-2026-09-16.md)
- [`neuraldeck-cet-not-installed-2026-09-16.md`](neuraldeck-cet-not-installed-2026-09-16.md)
- [`claude-session-mistakes-2026-09-15-16.md`](claude-session-mistakes-2026-09-15-16.md)
- [`local-paths-and-worklog-2026-09-14.md`](local-paths-and-worklog-2026-09-14.md)
