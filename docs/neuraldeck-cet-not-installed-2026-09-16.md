# NeuralDeck: CET не встановлено — ймовірна причина мовчання LogChannel

Дата: 2026-09-16

## Відповідь на відкрите питання

[`neuraldeck-logchannel-verification-gap-2026-09-16.md`](neuraldeck-logchannel-verification-gap-2026-09-16.md)
ставив питання власнику: чи встановлений Cyber Engine Tweaks (CET)?

Перевірено безпосередньо в ігровій папці
(`D:\games\Cyberpunk 2077 v.2.31 (2020)\Cyberpunk 2077`):

- немає жодного `.asi`-файлу в `bin\x64\`;
- немає жодного файлу/папки з "cet"/"cyber_engine_tweaks" у назві.

**CET не встановлено.** Це підтверджує гіпотезу 2 з попереднього документа:
`LogChannel` викликається успішно, але не пише в жоден файл, який ми
перевіряємо, бо немає CET-консолі, куди цей канал типово виводить.

## Чому не додано нативний C++ канал підтвердження замість цього

Попередній документ пропонував додати нативний виклик з боку C++ як
альтернативний канал підтвердження. Це прямо суперечить архітектурному
рішенню, вже ратифікованому і закомiченому власником у `cd91d63`
("route NeuralDeck through Codeware callbacks") та закріпленому негативним
тестом у `adapter/tests/NeuralDeckRedscriptContractTest.cmake`:

```
foreach(FORBIDDEN "GetAsyncKeyState" "ToggleFromNative" "NeuralDeckBridge")
```

Тобто: Codeware/Redscript одноосібно володіє і вводом, і показом попапу;
C++ не має жодного шляху назад у цей механізм. Спроба додати діагностичний
`GetAsyncKeyState`-опитувач у C++ (навіть лише для логування, без впливу на
поведінку) провалила цей тест і була відкинута.

## Наступний крок

Не змінювати код. Встановити CET і повторити один контрольований запуск з
натисканням End — якщо `LogChannel`-рядки (`"NeuralDeck registered Codeware
End callback"`, `"NeuralDeck received Codeware End press"`, `"NeuralDeck
ToggleOverlay showed popup"`) з'являться в CET-консолі, гіпотеза 2
підтверджена і розрив закритий без жодної зміни коду.

## Пов'язані документи

- [`neuraldeck-logchannel-verification-gap-2026-09-16.md`](neuraldeck-logchannel-verification-gap-2026-09-16.md)
- [`neuraldeck-f10-retrospective.md`](neuraldeck-f10-retrospective.md)
