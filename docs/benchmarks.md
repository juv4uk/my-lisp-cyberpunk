# Бенчмарки fixed-dispatch v0

## Навіщо

Цей репозиторій не міряє «швидкість Lisp» взагалі. `my-lisp` є
семантичним оракулом, `wsm-my-lisp` уже вимірює свій reader/evaluator/FFI,
а CML вимірює offline compilation. Тут має сенс міряти лише механічний
шлях, якого немає в тих репозиторіях:

```text
RED4ext Running tick → fixed `.my` dispatch → host capability → результат
```

Числа не є критерієм семантичної правильності і не мають бути CI gate.
Кожен вимір супроводжується окремим executable witness правильності.

## Три шари, які не можна змішувати

| Шар | Власник | Що вимірюємо | Чого не доводить |
|---|---|---|---|
| WSM ABI | `wsm-my-lisp` | reader, evaluator, FFI `wsm_eval_string` | ціну RED4ext callback і game capability |
| Adapter harness | цей репозиторій | fixed dispatch та виклик host primitive без гри | реальний frame budget у Cyberpunk |
| Live game | цей репозиторій + transcript | повний `Running` tick усередині процесу гри | переносну продуктивність на інші ПК |

Існуюча точка відліку WSM — `wsm_eval_string("(noop)")`; вона лишається
власністю `wsm-my-lisp` і не повинна дублюватися тут як начебто незалежний
результат.

## Adapter harness v0

Майбутній `adapter/tests/DispatchBenchmark.cpp` використовує той самий
динамічний WSM ABI, що й `DispatchWitness.cpp`, але замість RED4ext має
детерміновані stub-capabilities. Кожен запуск перевіряє результат виразу та
лічильник викликів; після цього вимірює Release x64 шлях.

| Назва | `.my` форма | Host факт | Ізолює |
|---|---|---|---|
| `dispatch-empty` | `(quote ())` | немає | мінімальний top-level dispatch |
| `dispatch-fact-false` | `cond` з `гравець-присутній?` | `()` | capability call + хибна гілка |
| `dispatch-fact-true-noop` | `cond` з істинною noop-гілкою | `t` | capability call + істинна гілка без дії |
| `dispatch-fact-true-log` | `cond` → `запиши-лог` | `t` | повний Lisp → host primitive dispatch, без фізичного log I/O |

Сценарії є `.my` файлами, а не C++ рядками. Це зберігає критерій: зміна
сценарію змінює поведінку без перекомпіляції adapter.

## Метод вимірювання

1. Збирати лише `Release`, x64; debug-результати не публікувати як game
   performance.
2. Записати SHA цього репозиторію, SHA WSM DLL, Windows, CPU, частоту
   процесора та параметри запуску.
3. Провести warm-up 1 000 dispatch-викликів.
4. Зібрати 30 незалежних серій по 10 000 викликів. Вивести median, p95 і
   найгіршу серію в ns/dispatch, а також точну кількість викликів capability.
5. Виводити машинний рядок у стилі спільних репо:

   ```text
   BENCH_RESULT\tadapter\tdispatch-fact-true-log\t<median-ns>
   ```

6. Не віднімати «порожній цикл» і не змішувати результат різних машин.
   Вимір має бути простим спостереженням повного заявленого шляху.

## Live-game вимір — окрема фаза

Лише після того, як adapter harness існує і має baseline, виміряти у
Cyberpunk реальний `Running` tick. Тоді збирати тривалість на початку й
наприкінці `DispatchRunningTick`, не логувати кожен tick, а записати
агрегати після фіксованої кількості кадрів:

- median / p95 / p99 у мікросекундах;
- кількість tick-ів;
- кількість викликів кожної capability;
- версію гри, RED4ext, plugin, WSM DLL та активний SHA сценарію.

Порівнювати це треба з budget конкретного кадру (наприклад 60 або 120 FPS),
але не вигадувати універсальний поріг до першого реального transcript.

## Не робимо в v0

- не бенчмаркаємо RTTI, логування на диск чи GPU разом із Lisp dispatch;
- не додаємо telemetry framework, profiler UI або callback registry;
- не оптимізуємо reader/evaluator у C++: це зона `wsm-my-lisp`;
- не змінюємо `.my` семантику заради числа.

Перший крок — `adapter harness v0`. Він має бути виконуваним, але
діагностичним: regression у коректності ламає witness, коливання часу лише
зберігається як evidence для наступного рішення.
