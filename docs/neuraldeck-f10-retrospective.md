# NeuralDeck F10: ретроспектива помилок

Дата: 2026-09-14

## Чому створено цей документ

Перші спроби відкрити NeuralDeck через F10 дали користувачу кілька запусків
гри без видимого результату. Це неприйнятно: успішна C++ збірка або Redscript
компіляція не є доказом, що гаряча клавіша доставлена в in-game UI.

Цей документ фіксує тільки встановлені факти, помилкові рішення та нові
обов'язкові гейти. Він не оголошує F10 робочим.

## Помилки

| Помилка | Чому була хибною | Факт, що її спростував | Виправлення процесу |
| --- | --- | --- | --- |
| C++ намагався викликати compiled Redscript UI через `CRTTISystem::GetFunction`. | Цей registry придатний для native `CGlobalFunction` і native class methods; compiled Redscript presentation не є таким global function. | Функція UI не була знайдена у native registry. | C++ більше не викликає Redscript presentation напряму. Межа — typed event у game UI queue. |
| Діагностика використала `LogChannel` без оголошення native signature. | Redscript не знав символ і відкинув весь NeuralDeck сценарій. | `redscript_rCURRENT.log` містив `UNRESOLVED_FN` і повідомляв, що винен NeuralDeck. | Будь-який новий Redscript API спершу перевіряється у встановлених declarations або додається разом із коректною declaration. |
| Після виправлення компіляції було зроблено висновок, що F10 має працювати. | Компіляція доводить лише валідність source, не delivery клавіші та не popup lifecycle. | Лог містив `Compilation complete`, але користувач не бачив UI. | Live claim можливий лише після transcript: F10 received/queued, UI receiver executed, popup attached. |
| Було перенесено hotkey у `ScriptableSystem`. | Codeware автоматично створює concrete `ScriptableService`; довільний `ScriptableSystem` не є заміною service container. | `ScriptableServiceContainer::OnInitializeScripts` перебирає лише класи-похідні `ScriptableService`. | NeuralDeck service залишається `ScriptableService`; session system не використовується як припущена точка входу. |
| Codeware `Input/Key` вважався еквівалентом OS keyboard input. | Він dispatch-ить тільки raw input, який уже потрапив у `InkSystem::ProcessInputEvents`. | `RawInputHook` явно працює від `InkSystem::ProcessInputEvents`; попередній C++ edge detector бачив F10 незалежно від Ink. | F10 є host input fact: C++ фіксує edge, UI отримує typed event. |
| Гра запускалася до того, як runtime route був доведений. | Це переклало невизначеність розробки на користувача. | Кілька запусків не дали нової локалізованої причини, лише «не працює». | Новий game launch дозволений лише після build, tests, source/ABI review та заздалегідь визначеного evidence record. |

## Підтверджений контракт

Codeware native source підтверджує:

1. concrete `ScriptableService` створюються під час script initialization;
2. `OnInitialize` викликається після створення game instance;
3. `Input/Key` активується через `InkSystem::ProcessInputEvents`;
4. `CustomPopup.Open` доставляє `ShowCustomPopupEvent` через `UISystem.QueueEvent`;
5. Codeware receiver у `PopupsManager` показує popup.

RED4ext SDK підтверджує:

1. C++ отримує `UISystem` через `ScriptGameInstance.GetUISystem`;
2. native `UISystem` method можна викликати через RTTI `CClass::GetFunction` і `ExecuteFunction`;
3. scripted event object можна створити через `CClass::CreateInstance` і передати як `Handle<IScriptable>`.

Звідси випливає поточний міст:

```text
Windows F10 edge
  -> C++ host adapter
  -> NeuralDeckToggleEvent
  -> UISystem.QueueEvent
  -> PopupsManager receiver
  -> NeuralDeckService
  -> Codeware CustomPopup
```

Цей міст не передає Lisp semantics у C++, не створює другого evaluator і не
перетворює C++ на власника UI policy. C++ повідомляє факт F10; Redscript
вирішує presentation lifecycle.

## Обов'язковий гейт перед наступним запуском

Перед live test мають бути виконані всі пункти:

1. C++ Release build проходить.
2. Усі CTest witnesses проходять.
3. `git diff --check` чистий.
4. Redscript source перевірений проти встановлених Codeware declarations.
5. Plugin log має розрізняти щонайменше `F10 edge`, `event queued` та
   `event rejected` з конкретною причиною.
6. План тесту містить один очікуваний результат і точний log path для
   підтвердження.
7. DLL і `.reds` не копіюються, поки `Cyberpunk2077.exe` працює.

Після цього дозволений один запуск. Якщо witness не пройшов, гра не
перезапускається повторно, доки журнал не локалізує нову причину й вона не
буде виправлена локально.

## Відкритий live witness

Поточна реалізація compiled і пройшла CTest. Вона ще не має live evidence,
що runtime RTTI містить `UISystem.QueueEvent` і що `PopupsManager` отримує
`NeuralDeckToggleEvent`. До появи такого transcript статус F10 — **in
progress**, а не done.
