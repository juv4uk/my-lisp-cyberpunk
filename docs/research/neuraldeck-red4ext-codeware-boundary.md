# NeuralDeck: RED4ext, Redscript і Codeware

_Перевірено у грі: 2026-09-14._

## Поточний compile gate

Перший варіант `NeuralDeckService` не пройшов Redscript compilation: для
`EInputAction` у CP2077 2.31 немає оператора `==`. Це не змінює архітектуру:
`InputTarget.Key(IK_F10, IACT_Press)` уже виконує потрібний відбір. Callback
тому не порівнює enum повторно і лише перемикає popup. Наступний запуск гри
має бути live-перевіркою саме цього виправлення.

## Висновок

`CRTTISystem::GetFunction` у RED4ext індексує native
`CGlobalFunction`. Він не є механізмом виклику compiled Redscript
`CScriptedFunction`.

Тому такий напрям є хибним:

```text
C++ F10 -> CRTTISystem::GetFunction -> compiled .reds UI function
```

Він не стає робочим зміною spelling, module namespace або `GameInstance`
context: compiled Redscript function не входить до native global registry.

## Підтвердження

- У local RED4ext SDK `CRTTISystem::GetFunction` повертає
  `CGlobalFunction*`, а registry містить native global functions.
- У грі `r6/logs/redscript_rCURRENT.log` підтвердив успішну компіляцію
  `NeuralDeckOverlay.reds` разом з Codeware 1.20.3.
- Live plugin log підтвердив, що F10 механічно доходить до C++ і змінює
  NeuralDeck state, але RTTI не знаходить compiled UI entrypoint.

## Канонічний напрям

```text
Codeware Input/Key (F10 press)
  -> NeuralDeck ScriptableService lifecycle
  -> inkHUDLayer widget
  -> NeuralDeck presentation
```

`ScriptableService` належить цілому запуску гри, незалежно від save, тому є
правильним власником persistent UI presentation. Ink widget створюється і
прикріплюється до `inkHUDLayer` самим Redscript/Codeware шаром. C++ не керує
UI-сценарієм, не обчислює Lisp і не опитує клавіатуру.

Після canonical-session bridge overlay читатиме transcript та відправлятиме
форми в єдину game-thread Lisp session. Він не створює другого evaluator чи
другу semantic authority.

## Джерела

- [RED4ext SDK: native global functions](https://github.com/WopsS/RED4ext.SDK/blob/master/examples/native_globals_redscript/Main.cpp)
- [RED4ext SDK: execute functions](https://github.com/WopsS/RED4ext.SDK/blob/master/examples/execute_functions/Main.cpp)
- [Codeware ScriptableService lifecycle](https://wiki.redmodding.org/redscript/references-and-examples/codeware-callbacks/scriptables-comparison)
- [Ink HUD widgets](https://wiki.redmodding.org/scripting-cyberpunk/how-do-i/inkwidgets)
- [Cyberpunk UI layers](https://wiki.redmodding.org/redscript/references-and-examples/ui-scripting)
- [Codeware popup reference](https://wiki.redmodding.org/redscript/references-and-examples/ui-scripting/popups)
