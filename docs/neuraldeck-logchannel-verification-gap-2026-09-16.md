# NeuralDeck: LogChannel не доводить нічого — розрив у методі верифікації

Дата: 2026-09-16

## Статус

Це аналіз і пропозиція, отримані читанням живих логів гри напряму
(`D:\games\Cyberpunk 2077 v.2.31 (2020)\Cyberpunk 2077`), а не новий код і
не новий запуск. Він не оголошує F10/End робочим і не змінює
`docs/neuraldeck-f10-retrospective.md` — доповнює його одним конкретним,
досі непоміченим розривом.

## Добра новина спершу

Останній наявний ігровий запуск (`14:57:20`–`14:59:58`, 15 вересня) —
**без крешу**. Це реальний прогрес відносно попереднього
`EXCEPTION_ACCESS_VIOLATION` (`docs/neuraldeck-f10-crash-2026-09-15.md`) і
відносно ще давнішого watchdog timeout (`220fbdb`, крешрепорт
`Cyberpunk2077-20260915-135153-7228-10112` — останній файл у
`ReportQueue`, новіших немає).

Підтверджено з `red4ext/logs/red4ext-2026-09-15-14-57-20.log`:

```
RED4ext (v1.30.0) is initializing...
Codeware (version: 1.20.3) has been loaded
my-lisp-cyberpunk (version: 0.1.0) has been loaded
scc invoked successfully, 25278 source refs were registered
```

І з `r6/logs/redscript_rCURRENT.log`: `NeuralDeckOverlay.reds` скомпільовано
без жодної помилки (`Compilation complete`).

## Розрив

Задеплоєний `r6/scripts/NeuralDeck/NeuralDeckOverlay.reds` (не той самий,
що в `main` — тут хоткей вже перенесено з `AddTarget`-фільтра в ручну
перевірку всередині `OnNeuralDeckKey`, і клавіша — `IK_End`, не `IK_F10`)
містить три `LogChannel(n"DEBUG", ...)` виклики, які мали б підтвердити
кожен крок:

```
"NeuralDeck registered Codeware End callback"   -- OnLoad()
"NeuralDeck received Codeware End press"        -- OnNeuralDeckKey()
"NeuralDeck ToggleOverlay showed popup"          -- ToggleOverlay()
```

Жоден з них **не з'явився в жодному з наявних лог-файлів** за час сесії
`14:57`–`14:59`:

- `r6/logs/redscript_rCURRENT.log` — лише компіляторні повідомлення
  (`Compiling files...`, `Compilation complete`), жодного рантайм-виводу.
- `red4ext/logs/red4ext-2026-09-15-14-57-20.log` — лише RED4ext-специфічні
  події (завантаження плагінів, `scc invoked`).
- `red4ext/plugins/Codeware/Codeware-2026-09-15-14-57-23.log` — лише
  внутрішня ініціалізація Codeware (`ResourcePathRegistry`), нуль згадок
  NeuralDeck.
- `red4ext/logs/my-lisp-cyberpunk-plugin-2026-09-15-14-57-25.log` — це
  C++-адаптер, який логує через RED4ext logger; тут є `NeuralDeck
  hotkey=End (Codeware)`, але це просто рядок C++-коду (`entered Running
  state`), не підтвердження того, що Redscript-подія реально дійшла.

Отже весь гейт верифікації з `neuraldeck-f10-retrospective.md` (§"Обов'язковий
гейт", пункт 5: "Redscript log має показати `registered Codeware F10
callback`...") спирається на канал, який **структурно нікуди видимо не
пише** в цій конфігурації — незалежно від того, чи сама кнопка працює.

## Дві окремі гіпотези — не плутати

1. **`NeuralDeckService.OnLoad()` взагалі не викликається** (Codeware не
   підхопив сервіс під час script initialization) — тоді жодна клавіша не
   допоможе, бо реєстрація хотdespoiler-колбека ніколи не відбувається.
2. **`OnLoad()` викликається нормально, але `LogChannel` у цій грі/збірці
   не пише в жоден файл, який ми перевіряємо** — типова причина: немає
   CET (Cyber Engine Tweaks), який зазвичай дає живу консоль для
   `LogChannel`/`Log()` виводу в модах на цьому движку. Без CET рушій може
   просто приймати виклик і нікуди його не показувати.

Наявні логи **не розрізняють ці дві гіпотези** — обидві виглядають
однаково "тихо" ззовні.

## Пропозиція

Не покладатися на `LogChannel` як єдиний канал підтвердження, бо:
- він недоведений як видимий у цій конфігурації;
- він живе в Redscript, а C++-логування через RED4ext logger уже емпірично
  підтверджено робочим (бачимо реальні рядки в `my-lisp-cyberpunk-plugin-*.log`
  щокадру для `PlayerPresentPrimitive` та інших подій).

Конкретний наступний крок (один, вузько сфокусований, дискримінує обидві
гіпотези одразу):

1. Додати один нативний виклик з `OnNeuralDeckKey` (і/або `OnLoad`) у
   вже наявний C++-адаптер (той самий шлях, яким `NeuralDeck`-скрипти вже
   викликають host-операції через `host-operations.lisp`/`adapter/src` —
   дивись `adapter/host-operations.lisp` на наявність придатної примітиви,
   або додати нову вузьку `neuraldeck-key-received` host-операцію, якщо
   такої нема).
2. Ця нативна примітива просто логує один рядок через **вже підтверджений
   робочим** RED4ext logger (`my-lisp-cyberpunk-plugin-*.log`), а не через
   `LogChannel`.
3. Один контрольований запуск: натиснути End, перевірити **тільки** цей
   новий рядок у `my-lisp-cyberpunk-plugin-*.log`. Якщо він з'явиться —
   гіпотеза 1 (сервіс не підхопився) спростована, залишається гіпотеза 2
   (CET/видимість) або далі по ланцюжку (popup). Якщо не з'явиться —
   підтверджена гіпотеза 1, і проблема справді в `OnLoad`/discovery, не в
   клавіші.

## Відкриті питання власнику

1. Чи встановлений CET (Cyber Engine Tweaks)? Якщо ні — це ймовірна причина
   мовчання `LogChannel`, і встановлення CET саме по собі може закрити
   розрив без жодної зміни коду.
2. Чи End реально був натиснутий під час сесії `14:57`–`14:59`, чи це був
   просто тестовий запуск без взаємодії?

## Пов'язані документи

- [`neuraldeck-f10-crash-2026-09-15.md`](neuraldeck-f10-crash-2026-09-15.md) — root cause access-violation крешу (виправлено).
- [`neuraldeck-f10-retrospective.md`](neuraldeck-f10-retrospective.md) — повна історія помилок і поточний гейт верифікації.
- [`local-paths-and-worklog-2026-09-14.md`](local-paths-and-worklog-2026-09-14.md) — шляхи гри й логів, використані для цього аналізу.
