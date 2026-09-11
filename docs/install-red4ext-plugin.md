# Встановлення/оновлення RED4ext-плагіна на локальну копію гри

Статус: виконано й перевірено багато разів живими запусками (2026-09-10/11).
Цей документ — актуальний checklist для оновлення плагіна після нового
build, не гіпотетичний план.

## Що потрібно скопіювати після кожної перезбірки

RED4ext і сама гра встановлені один раз (`red4ext/RED4ext.dll` +
`bin/x64/winmm.dll`) і не потребують повторної дії. Після кожного
`cmake --build adapter/build --config Release` треба оновити **три**
речі в `<game_dir>/red4ext/plugins/wsm-my-lisp-cyberpunk-plugin/`, не
лише dll-файли:

1. `my-lisp-cyberpunk-plugin.dll` (з `adapter/build/src/Release/`)
2. `wsm_my_lisp_cyberpunk_dll.dll` — **тепер з `host-runtime/target/x86_64-pc-windows-msvc/{debug,release}/`**, не з сусіднього `wsm-my-lisp` checkout (host-runtime мігрував сюди 2026-09-11, `wsm-my-lisp#15` Phase C; див. `host-runtime/MIGRATION.md`)
3. **`scripts/` — уся папка**, не лише окремий файл (з `my-lisp-cyberpunk/scripts/*.мій`)
   — забутий крок реально викликав `could not load scripts/диспетчер.мій`
   у живому лозі (2026-09-11), не гіпотетичний ризик.

```bash
PLUGDIR="<game_dir>/red4ext/plugins/wsm-my-lisp-cyberpunk-plugin"
cp adapter/build/src/Release/my-lisp-cyberpunk-plugin.dll "$PLUGDIR/"
cp host-runtime/target/x86_64-pc-windows-msvc/debug/wsm_my_lisp_cyberpunk_dll.dll "$PLUGDIR/"
mkdir -p "$PLUGDIR/scripts"
cp scripts/*.мій "$PLUGDIR/scripts/"
```

DLL-файли, зайняті поточним процесом гри, неможливо перезаписати — гру
треба закрити перед оновленням; зміна в `scripts/` теж не підхоплюється
без перезапуску гри (диспетчер читається один раз при `Load`).

## Перевірка після запуску

```bash
cat "<game_dir>/red4ext/logs/my-lisp-cyberpunk-plugin-*.log"
```

Очікувана послідовність (з `docs/vertical-slice.md`): `(запиши-лог) => ()`
одразу при завантаженні, далі `гравець-присутній?` через опитування щокадру
в `Running`-стані, доки не з'явиться `t`.

## Відомі ризики репаку

- Репак/Hydra-launcher копія, не офіційна — поведінка антипіратського
  захисту з зовнішніми DLL у процесі раніше не гарантувалась, але на
  практиці RED4ext + плагін уже кілька разів успішно завантажувались і
  гра стабільно запускалась.
- Перед експериментами з capability, що змінюють стан (поза поточним v0
  read-only scope), варто зробити бекап збережень
  (`%USERPROFILE%\Saved Games\CD Projekt Red\Cyberpunk 2077\`).
