# CML AOT dispatch contract v0

## Призначення

CML може прибрати reader/evaluator з гарячого шляху `Running` tick лише для
заздалегідь скомпільованого fixed-dispatch сценарію. Це оптимізація виконання,
не новий Lisp, не REPL і не друга semantic authority.

NeuralDeck та loopback REPL і надалі надсилають довільні форми в одну
канонічну persistent Lisp session. AOT виконує тільки конкретний artifact,
зібраний із `scripts/dispatcher.lisp`.

## Нинішня межа

Поточний C backend CML генерує standalone C-програму: вона містить `int main`,
власний heap/runtime і завершує процес через `exit` на runtime error. Такий
artifact не можна викликати з DLL усередині Cyberpunk 2077.

Adapter не має копіювати цей runtime, патчити згенерований C або викликати
його `main`. Це створило б другий evaluator та дозволило б Lisp-помилці
завершити процес гри.

## Потрібний контракт CML host target

CML має згенерувати один C ABI export для host-owned artifact:

```c
int cml_cyberpunk_dispatch(
    const CmlCyberpunkHostV1* host,
    void* context,
    CmlCyberpunkResultV1* out);
```

Він виконує скомпільовану `.lisp` policy один раз і повертає статус, а не
викликає `main` або `exit`. `out` не передає C++ внутрішні Lisp values: він
містить лише result-kind і UTF-8 presentation для diagnostic log.

`CmlCyberpunkHostV1` є table механізмів. У ній CML звертається тільки до
зареєстрованих Cyberpunk operation identities з
[`adapter/host-operations.lisp`](../adapter/host-operations.lisp):

| Identity | Surface | Механізм host |
|---|---|---|
| `cp:0001` | `запиши-лог` | записати diagnostic log |
| `cp:0002` | `гравець-присутній?` | повернути факт присутності |
| `cp:0003` | `клас` | прочитати class name opaque game handle |

C++ реалізує ці callbacks, тримає `GameHandleTable` і не інтерпретує Lisp
результат як policy. CML визначає порядок і умови capability calls через
скомпільований `.lisp` artifact.

## Обов'язкові властивості v0

- Artifact не містить `main`, `exit`, глобального interpreter state або
  відкритого callback registry.
- Усі resource/runtime failures повертаються через status/result; adapter
  записує diagnostic і продовжує гру.
- CML target читає operation identity з registry/projection, а не робить
  український display spelling ABI-ідентичністю.
- AOT artifact не приймає довільний текст під час tick і не змінює state
  канонічної REPL session.
- Зміна `.lisp` policy потребує нового AOT artifact, але не зміни C++ adapter.

## Executable witness

Один і той самий C++ host запускає два CML-згенеровані artifacts:

1. `scenario-log.lisp` викликає `cp:0001` для факту player-present;
2. `scenario-silent.lisp` повертає `()` для того самого факту.

Witness доводить різну поведінку без зміни host коду. Він також перевіряє, що
error status artifact не завершує harness.

## Порядок інтеграції

1. CML вводить host target, header і Linux/Windows witness без standalone
   `main`.
2. Cyberpunk додає artifact до CMake як optional build input і компілює
   adapter проти header CML.
3. Adapter отримує explicit dispatch mode: `wsm` за замовчуванням;
   `cml-aot` тільки коли перевірений artifact та ABI revision збігаються.
4. Обидва режими виконують ту саму registry-defined capability surface;
   local REPL і NeuralDeck завжди залишаються на canonical session.

До завершення пункту 1 `DispatchRunningTick` продовжує використовувати
канонічний WSM runtime.
