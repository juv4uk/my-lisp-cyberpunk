; tasks.my — my-lisp-cyberpunk
; Product-surface repo for my-lisp inside Cyberpunk 2077 (CET/RED4ext analogy).
; Created 2026-09-10 empty; recommendations set 2026-09-10 from cross-repo review.
; Updated 2026-09-11: deep-penetration task ladder after v0 fixed-dispatch.
; Updated 2026-09-11 (2): CP-PLAYER-HANDLE-LIVE runbook + acceptance ready.
; my-lisp remains sole language authority. Runtime embed work (Win64 nucleus,
; reader/eval/ffi) lives in this repo's host-runtime/. Cyberpunk-specific
; RED4ext integration lives in adapter/. This repo must not silently
; reimplement eval.

((kind . tasks)
 (version . 1)
 (goal . "One honest in-game my-lisp surface for Cyberpunk 2077 without inventing a second language.")
 (tasks .
  (("CP-OWNER-SCOPE-V0" .
    ((priority . 10.0)
     (done . t)
     (origin . owner-required)
     (capabilities . (governance product scope))
     (description . "Owner answers: does the first mod-script need only fixed host-dispatch (def+cond+eq+car/cdr + host primitives), or also first-class closures/callbacks like CET Lua?")
     (acceptance . "Written decision in README: v0 is fixed host-dispatch without closures/callbacks.")))
   ("CP-REPO-ROLE" .
    ((priority . 9.5)
     (done . t)
     (origin . owner-required)
     (depends-on . ("CP-OWNER-SCOPE-V0"))
     (description . "Decide whether this repo hosts RED4ext/UI product code or stays a thin pointer to wsm-my-lisp/plugin.")
     (acceptance . "README states role: this repo owns the adapter; wsm-my-lisp owns the runtime DLL.")))
   ("CP-ONE-SHOT-SCRIPT" .
    ((priority . 9.0)
     (done . t)
     (depends-on . ("CP-OWNER-SCOPE-V0"))
     (capabilities . (lisp cyberpunk fixtures))
     (description . "Check in one .my demo script (uk identifiers: телепортуй / дай-зброю / збережи-гру) matching my-lisp cyberpunk fixtures.")
     (acceptance . "scripts/перший-зріз.мій runs under my-lisp CLI oracle and returns (); the adapter expects the same result in-host.")))
   ("CP-NO-SILENT-CLOSURES" .
    ((priority . 8.5)
     (done . t)
     (origin . owner-required)
     (evidence . ("README.md" "adapter/README.md" "adapter/src/Main.cpp"))
     (description . "Policy: do not special-case fake closures for engine callbacks in v0; say not-in-v0 explicitly.")
     (acceptance . "No callback registry in v0 code without a new owner task.")))
   ("CP-LINK-WSM-PLUGIN" .
    ((priority . 8.0)
     (done . t)
     (evidence . ("adapter/README.md" "host-runtime/MIGRATION.md"))
     (description . "Document dependency on wsm-my-lisp plugin/DLL builds; version pin when first in-game load works. Superseded 2026-09-11 by wsm-my-lisp#15 Phase C/D: Rust host runtime migrated into this repo's host-runtime/, wsm-my-lisp/dll deleted (their commit 1e1549a). Dependency is now internal (adapter → ../host-runtime), not cross-repo.")
     (acceptance . "adapter/README.md points at host-runtime build output path, not a sibling wsm-my-lisp checkout — verified end-to-end (source committed, cargo build produces the DLL at that exact path, docs match).")))
   ("CP-LISP-OWNS-ORCHESTRATION-V0" .
    ((priority . 10.0)
     (done . t)
     (origin . owner-required)
     (depends-on . ("CP-OWNER-SCOPE-V0"))
     (capabilities . (fixed-dispatch host-mechanism lisp-policy))
     (description . "Перенести Running tick policy з C++ у scripts/диспетчер.мій: C++ доставляє tick та виконує зареєстровані capabilities, а `.my` запитує факти, обирає дію й послідовність.")
     (acceptance . "Два `.my` сценарії над незмінним adapter виражають різну поведінку для однакового факту; C++ не інтерпретує t/() і не тримає сценарний latch.")))
   ("CP-DISPATCH-BENCHMARKS-V0" .
    ((priority . 8.0)
     (done . t)
     (origin . owner-required)
     (depends-on . ("CP-LISP-OWNS-ORCHESTRATION-V0"))
     (evidence . ("adapter/tests/DispatchBenchmark.cpp" "docs/benchmarks.md"))
     (capabilities . (benchmark fixed-dispatch evidence))
     (description . "Створити Release x64 adapter harness, який вимірює fixed `.my` dispatch окремо від WSM microbench і реального RED4ext frame.")
     (acceptance . "30 серій по 10 000 викликів після warm-up; BENCH_RESULT з median/p95; correctness witness і timing не змішані; live-game вимір іде окремо.")))

   ;; === Глибоке проникнення (після v0) ===

   ("CP-PLAYER-HANDLE-LIVE" .
    ((priority . 9.5)
     (done . t)
     (depends-on . ("CP-LISP-OWNS-ORCHESTRATION-V0"))
     (capabilities . (rtti read-only live-evidence))
     (evidence . ("docs/cp-player-handle-live-runbook.md" "docs/vertical-slice.md"))
     (description . "Живий transcript у грі: (гравець-присутній?) повертає t, прив'язує opaque `гравець`, і результат видно в RED4ext log. Без цього не рухатись далі.")
     (acceptance . "Живий лог 2026-09-11: (quote ()) => (), present=false→true перехід, \"retained player as opaque token=1\", .my-диспетчер сам обрав викликати запиши-лог лише коли гравець присутній. Дедуплікація логу підтверджена (9 рядків, не 6.6MB). Один реальний log-фрагмент з Running + token ≠ 0 + (quote ()) oracle. Див. docs/cp-player-handle-live-runbook.md.")))
   ("CP-PLAYER-CLASSNAME" .
   ((priority . 9.0)
    (done . nil)
     (depends-on . ("CP-PLAYER-HANDLE-LIVE" "CP-BOXED-LIFETIME"))
     (capabilities . (rtti read-only))
     (description . "Перша read-only властивість через opaque handle: (клас гравець) → PlayerPuppet (або точне ім'я з RTTI). C++ тільки виконує GetClassName / аналог, Lisp бачить рядок/символ.")
     (acceptance . "Сценарій повертає очікуване ім'я класу; live transcript є.")))
   ("CP-LOCAL-REPL-V0" .
    ((priority . 9.2)
     (done . nil)
     (depends-on . ("CP-LISP-OWNS-ORCHESTRATION-V0" "CP-CANONICAL-REPL-SESSION"))
     (capabilities . (repl loopback persistent-session game-thread))
     (evidence . ("docs/repl-contract.md" "docs/superpowers/plans/2026-09-13-local-repl.md"))
     (description . "Локальний interactive REPL до єдиної host Lisp-сесії: loopback transport кладе рядок у bounded queue, а Running tick обчислює його в game thread.")
     (acceptance . "Два вирази в одному локальному з'єднанні ділять session state; malformed форма повертає error і наступний валідний вираз працює; live transcript підтверджує listener і result.")))
   ("CP-NEURALDECK-INGAME-REPL" .
    ((priority . 9.3)
     (done . nil)
     (depends-on . ("CP-CANONICAL-REPL-SESSION"))
     (capabilities . (ink-ui hotkey in-process-repl transcript))
     (evidence . ("docs/neuraldeck-repl.md"))
     (description . "Створити NeuralDeck — внутрішньоігровий Cyberpunk-styled REPL: hotkey відкриває ink overlay; UI кладе UTF-8 форму у локальну queue; Running tick виконує її в єдиній canonical session і повертає результат у transcript. TCP не є основним шляхом.")
     (acceptance . "У грі hotkey відкриває NeuralDeck; (визначити x 42) і x ділять одну session; (клас гравець) показує результат; malformed форма не закриває UI і не ламає наступну команду; live screenshot/log witness є.")))
   ("CP-CANONICAL-REPL-SESSION" .
   ((priority . 9.4)
     (done . nil)
     (depends-on . ("CP-LISP-OWNS-ORCHESTRATION-V0"))
     (capabilities . (my-lisp canonical-session embedding semantic-parity))
     (evidence . ("docs/canonical-embed-adoption.md" "my-lisp@178c80bb crates/my-lisp-embed"))
     (description . "Замінити fixed-dispatch evaluator host-runtime на canonical my-lisp session або на доведену embedding-boundary, яку володіє my-lisp. Upstream phase A/B готові: my-lisp-embed доводить persistent define/closure і nullary host mechanisms через C ABI. Наступна межа — typed opaque GameHandle bridge для (клас ...); не додавати локальну реалізацію def/let/closures у Cyberpunk repo.")
     (acceptance . "Через один REPL session `(визначити repl-перевірка 42)` і наступний `repl-перевірка` повертають 42; canonical my-lisp conformance fixture підтверджує semantic parity; adapter не містить другого evaluator.")))
   ("CP-BOXED-LIFETIME" .
    ((priority . 9.1)
     (done . t)
     (depends-on . ("CP-PLAYER-HANDLE-LIVE"))
     (capabilities . (host-runtime boxed-lifetime rational))
     (evidence . ("host-runtime/src/word.rs" "host-runtime/src/ffi.rs"))
     (description . "Розділити persistent handle/state і transient результати одного eval; надати FFI для тимчасового exact Rational, придатного для tick-спостережень.")
     (acceptance . "500 послідовних (позиція-x) через host primitive повертають 5/336; transient boxed values після кожного eval дорівнюють нулю; persistent GameHandle/Rational не інвалідовані.")))
   ("CP-PLAYER-WORLD-POSITION" .
    ((priority . 8.7)
     (done . nil)
     (depends-on . ("CP-PLAYER-CLASSNAME"))
     (capabilities . (rtti read-only vector3))
     (description . "Прочитати WorldPosition / GetWorldPosition гравця і повернути в Lisp як exact/rational або structured value (не float у мові).")
     (acceptance . "Lisp отримує три числа; позиція змінюється при русі гравця (два знімки в одному сеансі).")))
   ("CP-ONE-CHILD-WALK" .
    ((priority . 8.3)
     (done . nil)
     (depends-on . ("CP-PLAYER-WORLD-POSITION"))
     (capabilities . (rtti object-graph))
     (description . "Один крок по object graph: з PlayerPuppet дістати один child/system (наприклад GetQuickSlotsManager або inventory) як новий opaque handle.")
     (acceptance . "Новий token з'являється, клас відрізняється від PlayerPuppet.")))
   ("CP-READ-ONLY-METHOD-CALL" .
    ((priority . 8.0)
     (done . nil)
     (depends-on . ("CP-ONE-CHILD-WALK"))
     (capabilities . (rtti method-call))
     (description . "Один read-only method call через RTTI (ExecuteFunction) і повернення результату в Lisp.")
     (acceptance . "Результат видно в Lisp і в log; стан гри не змінений.")))
   ("CP-RTTI-BRIDGE-DESIGN" .
    ((priority . 7.5)
     (done . nil)
     (depends-on . ("CP-READ-ONLY-METHOD-CALL"))
     (capabilities . (architecture design))
     (description . "Спроектувати generic RTTI bridge замість hardcoded примітивів: (клас v), (читати-властивість v \"name\"), (викликати v \"Method\" args...). Документ + перший skeleton.")
     (acceptance . "ADR або docs/rtti-bridge.md + мінімальний proof-of-concept на 1-2 property.")))
   ("CP-EVENT-HOOK-V1" .
    ((priority . 6.5)
     (done . nil)
     (depends-on . ("CP-RTTI-BRIDGE-DESIGN"))
     (capabilities . (events hooks))
     (description . "Перша реакція на engine event (не callback-closure, а fixed-dispatch + факт у таблиці). Наприклад OnPlayerSpawned / OnDeath / простий game-state change.")
     (acceptance . "Lisp бачить факт події в наступному tick без C++ policy.")))
   ("CP-MUTATING-CAPABILITY-POLICY" .
    ((priority . 5.0)
     (done . nil)
     (origin . owner-required)
     (description . "Власник явно вирішує політику першої mutating capability (телепорт / inventory / save). Read-only спочатку, write тільки після окремого рішення.")
     (acceptance . "Запис у README або tasks.my: що дозволено в v1, що заборонено.")))

   ("CP-LISP-HARDWARE-OPTIMIZER" .
    ((priority . 3.0)
     (done . nil)
     (origin . owner-dream)
     (depends-on . ("CP-LISP-OWNS-ORCHESTRATION-V0"))
     (capabilities . (rtti-read game-settings hardware-profile))
     (description . "Майбутнє: my-lisp сам вирішує/пропонує налаштування графіки Cyberpunk 2077 під конкретне залізо власника (профіль з wsm-os/docs/OWNER-HARDWARE-PROFILE.md — i5-6400, GTX 1050 Ti 4GB, 16GB DDR4-2133), а не C++/людина вручну редагує UserSettings.json. Ручне налаштування (2026-09-10/11) показало: більшість реального запасу продуктивності вже вичерпана статичним налаштуванням presets — справжня цінність Lisp-шару тут була б у ДИНАМІЧНОМУ рішенні (читати поточний FPS/frame time через RTTI, як зробити power-of-two decision про conditional-quality в реальному часі), не в одноразовій заміні полів JSON.")
     (acceptance . "Не визначено — записано як мрія/напрям, не задача з конкретним acceptance. Перший крок, коли до цього дійде: власник вирішує, чи це read-only advisory (Lisp пропонує, людина застосовує) чи write capability (Lisp сам змінює UserSettings.json/live game state) — той самий read-only-перший принцип, що й для player-handle capabilities."))))))
