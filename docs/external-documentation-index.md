# Зовнішня документація: робочий індекс

Цей файл є локальною точкою входу до зовнішніх першоджерел, потрібних для
`my-lisp-cyberpunk`. Він не копіює сторонню документацію повністю: джерела
залишаються у своїх репозиторіях, зі своїми ліцензіями, історією та оновленнями.

## Authority map

| Джерело | Володіє | Коли читати |
| --- | --- | --- |
| [my-lisp](https://github.com/juv4uk/my-lisp) | семантикою мови | перед будь-якою зміною форми, значення або REPL contract |
| [wsm-target-contract](https://github.com/juv4uk/wsm-target-contract) | word/ABI representation | перед FFI, handles і boxed values |
| [RED4ext SDK](https://github.com/WopsS/RED4ext.SDK) | native plugin API та RTTI | для C++ adapter і game lifecycle |
| [Redscript](https://github.com/jac3km4/redscript) | compiled game scripting | для `.reds` кодів та compile diagnostics |
| [Codeware](https://github.com/psiberx/cp2077-codeware) | UI primitives, services, input | для NeuralDeck ink presentation |
| [Cyberpunk Modding Wiki](https://wiki.redmodding.org/cyberpunk-2077-modding) | community-verified workflows | для UI, lifecycle, compatibility та troubleshooting |
| [NativeDB](https://nativedb.red4ext.com/) | RTTI type/function discovery | перед новим engine capability |

## RED4ext

- [Plugin lifecycle](https://github.com/CDPR-Modding-Documentation/Red4Ext-Wiki/blob/main/mod-developers/creating-a-plugin.md): RTTI/game memory не можна використовувати в `Load`; game-facing робота починається з custom game state.
- [Native function registration](https://github.com/CDPR-Modding-Documentation/Red4Ext-Wiki/blob/main/mod-developers/adding-a-native-function.md): native capability реєструється як `CGlobalFunction` або class function через register callbacks.
- [Execute functions example](https://github.com/WopsS/RED4ext.SDK/blob/master/examples/execute_functions/Main.cpp): capability execution на existing game systems.
- [Native globals example](https://github.com/WopsS/RED4ext.SDK/blob/master/examples/native_globals_redscript/Main.cpp): GameInstance/player lookup і native globals.

### Застосування до NeuralDeck

`CRTTISystem::GetFunction` є registry native `CGlobalFunction`; не використовувати
його для виклику compiled Redscript UI. C++ передає host facts і capabilities,
не є controller Redscript presentation.

## Redscript і ink UI

- [Redscript repository](https://github.com/jac3km4/redscript): компілятор та інсталяція.
- [UI scripting](https://wiki.redmodding.org/redscript/references-and-examples/ui-scripting): `inkHUDLayer`, traversal та lifecycle UI.
- [Ink widgets](https://wiki.redmodding.org/scripting-cyberpunk/how-do-i/inkwidgets): створення/parenting `inkWidget`, debugging через Ink Inspector.
- [Popups](https://wiki.redmodding.org/redscript/references-and-examples/ui-scripting/popups): готові popup patterns і Codeware UI.
- [Modules](https://wiki.redmodding.org/redscript/language/language-features/modules): runtime identity module members.

## Codeware

- [Codeware repository](https://github.com/psiberx/cp2077-codeware): source і release compatibility.
- [Scriptables comparison](https://wiki.redmodding.org/redscript/references-and-examples/codeware-callbacks/scriptables-comparison): `ScriptableService` живе весь запуск гри; `ScriptableSystem` належить save session.
- [InkPlayground](https://github.com/psiberx/cp2077-playground): executable UI examples.

### Застосування до NeuralDeck

NeuralDeck presentation належить Codeware `ScriptableService`: він реєструє
`Input/Key` для `IK_F10` + `IACT_Press`, створює та прикріплює widget до HUD у
власному lifecycle. C++ не опитує клавіатуру та не викликає compiled Redscript.
UI не створює Lisp interpreter і не вирішує game policy.

## Local game evidence

| Артефакт | Призначення |
| --- | --- |
| `r6/logs/redscript_rCURRENT.log` | Redscript compilation і помилки `.reds` |
| `red4ext/logs/my-lisp-cyberpunk-plugin-*.log` | adapter, Lisp session, F10 events |
| `red4ext/plugins/Codeware/Codeware-*.log` | Codeware startup і compatibility |
| `docs/research/neuraldeck-red4ext-codeware-boundary.md` | зафіксований висновок про межу native RTTI / compiled Redscript |

## Правило оновлення

Перед новим етапом інтеграції додай сюди джерело, версію гри/фреймворку,
конкретний verified finding і посилання на executable witness або live log.

Повний аудит поточного покриття та витягнуті правила: 
[`documentation-audit.md`](research/documentation-audit.md).
