# Аудит документації Cyberpunk-мода

_Оновлено: 2026-09-14. Цей документ є картою прочитаних контрактів, а не
копією чужих джерел._

## Локальна бібліотека першоджерел

| Джерело | Стан | Що прочитано для мода | Висновок для коду |
| --- | --- | --- | --- |
| RED4ext Wiki | повний локальний набір: 15 Markdown-файлів | plugin lifecycle, game states, logging, native functions/classes, install/package flow | `Load`/`Unload` не торкаються game memory; game work належить custom state; кожен capability має лог і witness. |
| RED4ext SDK | локальний snapshot: 10 329 relevant source/docs files | GameState API, RTTI registry, native global/class registration, execute-function examples | `GetFunction` registry належить native globals; він не є Redscript UI dispatcher. |
| Redscript | повний локальний README та type-docs | compiler lifecycle, types, `ref`/`wref`, `CName`, `Variant`, source layout | `.reds` компілюється на старті в `final.redscripts.modded`; compile log — перший gate перед gameplay test. |
| Codeware | локальний snapshot: 7 586 source/docs files | complete public wiki sections: lifecycle, callbacks, player, UI, resources, localization, reflection, utilities | `ScriptableService` володіє NeuralDeck UI; `InputTarget.Key` замінює polling; Reflection — кандидат для read-only RTTI ladder. |
| Cyberpunk Modding Wiki | актуальна звірка | core frameworks, folders, logs, Ink widgets, popup patterns, troubleshooting | `.reds` належать `r6/scripts`; невдала компіляція означає, що UI не може тестуватись у грі до виправлення логу. |
| NativeDB | навігаційна authority | типи та функції REDengine для конкретних read-only capabilities | жодне RTTI-ім'я не додається в host operation без посилання на NativeDB і live witness. |

## Виведені правила

### Життєвий цикл

1. RED4ext `Load` реєструє plugin structures, але не читає game memory.
2. `Running` викликається кожний frame; він залишається місцем game-thread
   evaluation та bounded queue drain, але не місцем input polling чи UI policy.
3. Codeware `ScriptableService` живе весь запуск, а game objects не можна
   зберігати через межу session. NeuralDeck знищує session UI ref в
   `OnUninitialize`.
4. Для UI, що потребує player/HUD, точка готовності — game session /
   `CustomPopupManager.IsInitialized()`, а не plugin load.

### UI та ввід

1. F10 приходить через `CallbackSystem`:
   `InputTarget.Key(EInputKey.IK_F10, EInputAction.IACT_Press)`.
2. Відбір action уже зроблено target-ом. Не порівнювати `EInputAction` через
   `==`: у поточній Redscript surface для enum немає такого overload.
3. NeuralDeck використовує `CustomPopup` з `IsBlocking() = false` та
   `UseCursor() = false`; не успадковується від `InGamePopup`, бо той вмикає
   time dilation і modal context.
4. `inkHUDLayer` і Codeware popup manager — два допустимі шляхи parenting.
   Наша перша панель використовує manager, щоб мати нормальний attach/detach
   lifecycle і не залишати stale widget після session end.
5. UI показує data. Він не оцінює Lisp і не керує capability policy.

### Семантика й engine bridge

1. my-lisp залишається єдиним owner semantic session і orchestration.
2. C++/RED4ext дає атомарні capabilities; Redscript/Codeware дає presentation
   та engine-side lifecycle; обидва не додають Lisp semantics.
3. Opaque handle перетинає C++/Lisp межу як session token, ніколи raw pointer.
4. Codeware `Reflection` дозволяє inspect property та invoke method, але
   generic bridge з'явиться лише після read-only effect contract, NativeDB
   lookup і live witness на кожній новій capability.

### Діагностика та доставка

1. Порядок перевірки: `redscript_rCURRENT.log` → RED4ext load log → plugin
   log → only then in-game interaction.
2. `r6/scripts` містить Redscript; `red4ext/plugins/<mod>` містить DLL payload.
3. Release packaging повинен класти DLL, runtime DLL, scripts, version manifest
   та tested game/framework versions в один reproducible archive.
4. Перед заміною DLL або `.reds` гра мусить бути завершена; інакше live
   process тестує попередній compiled/cache state.

## Застосування до поточної роботи

Відхилений дизайн: `C++ F10 → CRTTISystem::GetFunction → compiled Redscript`.
Він не працює, бо native global registry не містить compiled `CScriptedFunction`.

Прийнятий дизайн: `Codeware Input/Key → NeuralDeckService → CustomPopup → Ink`.
Після canonical my-lisp embedding цей самий UI надсилатиме форму у bounded
game-thread queue та показуватиме transcript.

## Наступне читання перед кожним етапом

| Етап | Обов'язкові джерела |
| --- | --- |
| Canonical REPL session | `my-lisp` embed ABI, `wsm-target-contract`, host FFI tests |
| Text input і transcript | Codeware UI TextInput source, InkPlayground, Redscript callback docs |
| Player position / child walk | NativeDB entry, RED4ext execute-function example, existing handle lifetime docs |
| Generic RTTI bridge | Codeware Reflection source, NativeDB function signatures, explicit read-only effect matrix |
| Release | RED4ext install/packaging docs, dependency version matrix, clean-machine smoke test |
