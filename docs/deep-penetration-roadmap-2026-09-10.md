# Глибоке проникнення в REDengine 4: роадмап і три термінові фікси

Власника аналіз (2026-09-10), збережено дослівно як référence-документ.

## Статус після трьох фіксів

Оригінальний аналіз нижче збережений без переписування. Після нього код
рухався, тому його три проблеми мають різний актуальний стан:

| Проблема | Стан | Доказ |
|---|---|---|
| RTTI під час `Load` | Виправлено | `PlayerPresentPrimitive` запускається лише з `OnUpdate` у `EGameStateType::Running`; коміт `ab716c1` |
| Неправдивий `RUNTIME_VERSION_INDEPENDENT` | Виправлено | `Query()` оголошує `RED4EXT_V1_RUNTIME_VERSION_LATEST`; коміт `60ee238` |
| Ownership `GameHandle` | Виправлено | `GameHandleTable` утримує живі `RED4ext::Handle<>`; Lisp бачить тільки token/index; коміт `2f44b09` |

Перший read-only зріз `(запиши-лог)` перевірено живим запуском у грі;
`(гравець-присутній?)` зібрано, але ще не має окремого live transcript.
Отже наступна реалізаційна робота починається не з нової mutating
capability, а з ownership-моделі для першого opaque player handle.

## Загальна оцінка

Напрям правильний. Технічний ланцюжок:

```text
my-lisp (семантика)
   ↓
wsm-my-lisp (reader/eval + Word ABI + Boxed handles)
   ↓
my-lisp-cyberpunk.dll (adapter)
   ↓
RED4ext / RED4ext.SDK (CRTTISystem, ExecuteGlobalFunction, ExecuteFunction,
                        CClass/properties, GameStates, hooks)
   ↓
REDengine 4 (PlayerPuppet / systems / inventory / world / entities / events)
```

Фактична глибина зараз ~3/10 з 10 рівнів:

| Рівень | Що означає | Стан |
|---|---|---|
| 0 | DLL потрапляє в процес гри | ✅ |
| 1 | Lisp runtime живе всередині Cyberpunk | ✅ |
| 2 | Lisp → C++ host → Lisp | ✅ |
| 3 | Звернення до REDengine RTTI | 🟡 код є (гравець-присутній?) |
| 4 | Отримати реальний `PlayerPuppet` | 🟡 майже |
| 5 | Читати position/health/state | ❌ |
| 6 | Ходити object graph | ❌ |
| 7 | Реагувати на events/hooks | ❌ |
| 8 | Керовано змінювати game state | ❌ |
| 9 | Generic RTTI/reflection bridge з Lisp | ❌ |
| 10 | Низькорівневі native hooks/detours | ❌ |

## Три термінові проблеми (важливіші за нові capability)

### 1. RTTI-виклик у неправильному lifecycle-місці

`PlayerPresentPrimitive` викликається через `evalString()` прямо під час
`EMainReason::Load`. Офіційна документація RED4ext прямо попереджає: у
`Main(...Load...)` game memory ще не готова для RTTI/call functions —
потрібні custom game states, коли scripting system уже доступна
(docs.red4ext.com/mod-developers/creating-a-plugin).

**Дія**: перенести game-facing Lisp-виклики з `Load` у правильний game
state listener, перш ніж читати щось складніше за truthy-check.

### 2. Ownership моделі GameHandle не вирішена

`BoxedValue::GameHandle(*mut c_void)` зберігає лише сирий pointer;
validity lifetime — відповідальність adapter-а, документація це чесно
визнає. Але справжній `RED4ext::Handle<IScriptable>` має власну
reference-counted семантику життя — збереження внутрішнього сирого pointer
після знищення локального `Handle` потенційно залишає dangling reference.

**Пропозиція**: adapter володіє справжнім `RED4ext::Handle<>` (в C++
сторону, живе в своїй таблиці), Lisp отримує лише непрозорий token/index у
цю таблицю — не адресу engine-об'єкта напряму.

### 3. `Query()` бреше про свій runtime scope

`Query()` досі заявляє `RUNTIME_VERSION_INDEPENDENT` з коментарем "adapter
не торкається RTTI" — це вже неправда після `гравець-присутній?`. RED4ext
рекомендує `RUNTIME_LATEST` для плагіна, прив'язаного до game API.

**Дія**: виправити до розширення object access.

## Наступна вісь (не `дай-зброю`/`телепортуй`/callbacks)

```text
GetPlayer → opaque Player handle → GetClassName → PlayerPuppet
  → read WorldPosition → read simple state → walk one child object
  → call one read-only method
```

Лише після цього: events/hooks → persistent subscriptions → mutating
capabilities.

Довгостроковий напрям — не сотні hardcoded C++ primitives
(`здоров'я-гравця`, `позиція-гравця`, `машина-гравця`), а **керований RTTI
bridge**:

```lisp
(визначити v (гравець))
(клас v)                          ; => PlayerPuppet
(читати-властивість v "...")      ; => ...
(викликати v "GetQuickSlotsManager") ; => <game-handle>
(клас ...)                        ; => QuickSlotsManager
```

REDengine RTTI → symbolic object graph → my-lisp → query/inference/agents.
Це відрізняється від "звичайного мод-набору команд" — Lisp досліджує живий
об'єктний світ гри як дані, а не викликає фіксований список команд.

## Конкретні наступні кроки (додано 2026-09-11)

Порядок задач у `tasks.my` (глибоке проникнення):

1. **CP-PLAYER-HANDLE-LIVE** (пріоритет 9.5)  
   Живий transcript: `(гравець-присутній?)` → opaque `гравець` + token ≠ 0 у log.

2. **CP-PLAYER-CLASSNAME** (9.0)  
   Перша властивість: `(клас гравець)` → `PlayerPuppet`.

3. **CP-PLAYER-WORLD-POSITION** (8.7)  
   Прочитати WorldPosition / GetWorldPosition, повернути в Lisp.

4. **CP-ONE-CHILD-WALK** (8.3)  
   Один крок по object graph → новий opaque handle.

5. **CP-READ-ONLY-METHOD-CALL** (8.0)  
   Один read-only ExecuteFunction → результат у Lisp.

6. **CP-RTTI-BRIDGE-DESIGN** (7.5)  
   Generic bridge замість hardcoded примітивів.

7. **CP-EVENT-HOOK-V1** (6.5)  
   Перша реакція на engine event через fixed-dispatch.

8. **CP-MUTATING-CAPABILITY-POLICY** (5.0, owner-required)  
   Явне рішення власника про першу write-capability.

Сценарії-фікстури підготовлені в `scripts/`:
- `сценарій-гравець-клас.мій`
- `сценарій-позиція.мій`
- `сценарій-нащадок.мій`

Критерій готовності до наступного рівня: live transcript + oracle-перевірка в CLI.

## Роль інших репо в цій фазі

- `my-lisp` — семантика opaque handle і capability boundary (вже готова)
- `wsm-target-contract` — ратифікував `GameHandle` як session-local `Boxed`
- `wsm-my-lisp` — транспорт handle через FFI (wrap/unwrap, готово)
- `cml` — AOT/codegen шлях, **не веде глибше в гру** — оптимізація
  майбутнього розгортання, не двері в REDengine

## Критерій "справжнього проникнення"

Правильний lifecycle → отримати Player → прочитати реальну
властивість/позицію → повернути її в my-lisp, живим запуском у грі. Якщо
це проходить: "ми не інтегруємо Lisp з Cyberpunk — ми починаємо будувати
Lisp-інтерфейс до внутрішньої моделі REDengine 4."
