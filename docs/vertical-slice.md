# Перший вертикальний зріз: `(запиши-лог)` + `(гравець-присутній?)`

## Статус

`(запиши-лог)` живим запуском у грі підтверджено тричі (2026-09-10,
RED4ext v1.30.0, Cyberpunk 2077 v2.31) — transcript в закритому issue
`my-lisp-cyberpunk#1`. `(гравець-присутній?)` — друга, теж read-only
capability, зібрана, ще не перевірена живим запуском.

## Що саме доводить зріз

Після завантаження RED4ext викликає `Main(..., Load, ...)`. Адаптер:

1. знаходить свою директорію;
2. завантажує `wsm_my_lisp_cyberpunk_dll.dll` через `LoadLibraryW`;
3. створює Lisp-сесію через `wsm_session_init`;
4. реєструє host-примітиву `запиши-лог`;
5. реєструє host-примітиву `гравець-присутній?`;
6. обчислює `(запиши-лог)`, звіряє результат з oracle (`()`), fails closed
   при розбіжності;
7. обчислює `(гравець-присутній?)`, звіряє результат проти `t`/`()` (обидва
   валідні — гравець або є, або ще нема, жодне не помилка), fails closed
   лише на неочікуваний результат;
8. записує обидва виклики й результати у RED4ext log.

`запиши-лог` не отримує жодних RTTI-посилань і не має доступу до
save/inventory/player state. `гравець-присутній?` торкається RTTI вперше
(`RED4ext::ExecuteGlobalFunction("GetPlayer;GameInstance", ...)`, той самий
патерн, що `RED4ext.SDK`'s власний `examples/accessing_properties`'s
`IsPlayerCrouched`) — але лише перевіряє факт наявності гравця (`bool`),
нічого не читає з самого player-об'єкта (не позицію, не здоров'я, нічого)
і не утримує отриманий `RED4ext::Handle` після виклику.

## Очікувані рядки логу

```text
my-lisp-cyberpunk: Lisp host primitive запиши-лог invoked
my-lisp-cyberpunk: (запиши-лог) => ()
my-lisp-cyberpunk: Lisp host primitive гравець-присутній? invoked, present=<true|false>
my-lisp-cyberpunk: (гравець-присутній?) => <t|()>
my-lisp-cyberpunk: wsm_my_lisp_cyberpunk_dll.dll loaded, session=...
```

Порядок перших чотирьох рядків фіксований, останній вказує на успішне
створення сесії. Значення адреси після `session=` не є частиною фікстури.
`гравець-присутній?`'s `present=`/результат залежить від реального стану
гри в момент `EMainReason::Load` (ймовірно `false`/`()`, бо Load
спрацьовує до появи гравця) — обидва значення валідні, жодне не є
провалом фікстури.

## Вже перевірено поза грою

- `scripts/перший-зріз.my` виконано через reference `my-lisp`; результат — `()`.
- `cmake --build adapter/build --config Release` успішно зібрав
  `my-lisp-cyberpunk-plugin.dll` з обома примітивами.
- `dumpbin /exports` підтвердив три необхідні RED4ext exports: `Main`,
  `Query`, `Supports`.

## Ще не перевірено живим запуском

- `(гравець-присутній?)` — зібрано, ще не запущено в грі з оновленим
  плагіном.

## Відомі межі

- `wsm_wrap_game_handle`/`wsm_unwrap_game_handle` (wsm-my-lisp commit
  `48a63ed`) готові, але `гравець-присутній?` їх свідомо не використовує —
  handle відкидається одразу після перевірки truthy/falsy. Boxed
  game-handle зберігання — наступний, ще не зроблений крок для capability,
  якій потрібно утримувати handle між викликами.
- **Ownership-контракт для майбутнього handle-зберігання** (wsm-my-lisp
  commit `09f3951`, docs/deep-penetration-roadmap-2026-09-10.md): коли
  з'явиться перша capability, що утримує `RED4ext::Handle<T>` між
  викликами, адаптер **не має** передавати `wsm_wrap_game_handle` сирий
  `T*`, здобутий з локального `Handle<T>` — цей `Handle<T>` виходить зі
  скоупу й декрементує refcount, залишаючи збережений pointer dangling.
  Замість цього: адаптер тримає власну таблицю refcount-живих `Handle<T>`
  (індексовану C++ структуру), і передає в `wsm_wrap_game_handle` лише
  opaque token/index у цю таблицю (напр. індекс, кастований у
  `*mut c_void`) — не адресу самого engine-об'єкта. Ця таблиця ще не
  написана — жодна поточна capability її не потребує.
- Усі числа tagged-word ABI надходять з `wsm-target-contract` (v4,
  ратифіковано). `t`/`()` — `Tag::True`/`Tag::Nil`, обидва вже стабільні
  частини контракту з версії 1.
