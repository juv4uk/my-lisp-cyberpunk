# Перший вертикальний зріз: `(запиши-лог)` + `(гравець-присутній?)`

## Статус

`(запиши-лог)` живим запуском у грі підтверджено тричі (2026-09-10,
RED4ext v1.30.0, Cyberpunk 2077 v2.31) — transcript в закритому issue
`my-lisp-cyberpunk#1`. `(гравець-присутній?)` — друга, теж read-only
capability, зібрана, ще не перевірена живим запуском.
Коли вона вперше бачить гравця, адаптер прив'язує Lisp-ім'я `гравець`
до `#<game-handle>` без розкриття адреси REDengine.

## Що саме доводить зріз

Після завантаження RED4ext викликає `Main(..., Load, ...)`. Адаптер:

1. знаходить свою директорію;
2. завантажує `wsm_my_lisp_cyberpunk_dll.dll` через `LoadLibraryW`;
3. створює Lisp-сесію через `wsm_session_init`;
4. реєструє host-примітиву `запиши-лог`;
5. реєструє host-примітиву `гравець-присутній?`;
6. обчислює `(quote ())`, звіряє результат з oracle (`()`), fails closed
   при розбіжності;
7. доставляє кожен `Running` tick в один fixed-dispatch `.my` вираз з
   `scripts/диспетчер.мій`; C++ не викликає `(гравець-присутній?)`, не
   порівнює `t`/`()` і не вирішує, коли зупинити сценарій;
8. якщо Lisp-сценарій викликає `(гравець-присутній?)` і отримує `t`, host
   копіює `RED4ext::Handle<IScriptable>` у власну
   C++-таблицю, передає Lisp тільки session-local token і прив'язує його
   під іменем `гравець`;
9. записує лише спостережуваний результат Lisp-dispatch у RED4ext log,
   без інтерпретації його як команди для C++.

`запиши-лог` не отримує жодних RTTI-посилань і не має доступу до
save/inventory/player state. `гравець-присутній?` торкається RTTI вперше
(`RED4ext::ExecuteGlobalFunction("GetPlayer;GameInstance", ...)`, той самий
патерн, що `RED4ext.SDK`'s власний `examples/accessing_properties`'s
`IsPlayerCrouched`) — але лише перевіряє факт наявності гравця (`bool`),
нічого не читає з самого player-об'єкта (не позицію, не здоров'я, нічого)
і не розкриває отриманий `RED4ext::Handle` Lisp-коду як адресу.

## Очікувані рядки логу

```text
my-lisp-cyberpunk: Lisp host primitive запиши-лог invoked
my-lisp-cyberpunk: (quote ()) => ()
my-lisp-cyberpunk: Lisp host primitive гравець-присутній? invoked, present=<true|false>
my-lisp-cyberpunk: retained player as opaque token=<nonzero>
my-lisp-cyberpunk: Lisp dispatch => ()
my-lisp-cyberpunk: wsm_my_lisp_cyberpunk_dll.dll loaded, session=...
```

`(quote ())` і створення сесії відбуваються під час `Load`. Решта рядків
можуть з'являтися лише після `Running` tick і залежать від обраного `.my`
сценарію та реального стану гри. Значення адреси після `session=` не є
частиною фікстури.

## Виконуваний доказ інверсії керування

`adapter/tests/DispatchWitness.cpp` запускає той самий WSM ABI з однаковим
host-фактом «гравець присутній» для двох `.my` сценаріїв:

- `scripts/сценарій-лог.мій` викликає `запиши-лог` рівно один раз;
- `scripts/сценарій-тиша.мій` не викликає його жодного разу.

C++ witness не змінюється між запусками. Це локальний executable proof
fixed-dispatch; він не замінює потрібний live transcript у грі.

## Вже перевірено поза грою

- `scripts/перший-зріз.мій` виконано через reference `my-lisp`; результат — `()`.
- `cmake --build adapter/build --config Release` успішно зібрав
  `my-lisp-cyberpunk-plugin.dll` з обома примітивами.
- `dumpbin /exports` підтвердив три необхідні RED4ext exports: `Main`,
  `Query`, `Supports`.

## Ще не перевірено живим запуском

- `(гравець-присутній?)` — зібрано, ще не запущено в грі з оновленим
  плагіном.

## Відомі межі

- `GameHandleTable` у C++ adapter тримає копії refcounted
  `RED4ext::Handle<IScriptable>` і передає `wsm_wrap_game_handle` лише
  ненульовий token. Жоден raw engine pointer не переходить FFI-межу.
  Таблиця очищується при `Unload`; handle не може жити довше за сесію гри.
- Перший live transcript для `гравець`/`#<game-handle>` ще потрібен. До
  нього це зібраний, але не підтверджений у грі read-only шлях.
- Усі числа tagged-word ABI надходять з `wsm-target-contract` (v4,
  ратифіковано). `t`/`()` — `Tag::True`/`Tag::Nil`, обидва вже стабільні
  частини контракту з версії 1.
