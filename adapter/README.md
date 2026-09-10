# RED4ext adapter

Це Cyberpunk-specific host adapter. Він не містить reader, evaluator,
семантику Lisp або машинне кодування значень: усе це лишається в
[`wsm-my-lisp`](https://github.com/juv4uk/wsm-my-lisp).

Сам факт завантаження плагіна в процес гри є новою інтеграційною дією.
Його поведінка при цьому повністю reversible:
він реєструє українську примітиву `(запиши-лог)`, виконує її один раз і
пише результат `()` у RED4ext log. Збереження, інвентар, позиція гравця
та RTTI-об'єкти не змінюються.

## Межа відповідальності

- `my-lisp` визначає значення програми й фікстури.
- `wsm-my-lisp` надає host-neutral DLL і її FFI.
- цей каталог тримає RED4ext SDK та код, що розмовляє з процесом гри.

## Збірка

```powershell
git submodule update --init adapter/deps/red4ext.sdk
cmake -S adapter -B adapter/build -G "Visual Studio 17 2022" -A x64
cmake --build adapter/build --config Release
```

Для фактичного тесту потрібні разом:

1. `my-lisp-cyberpunk-plugin.dll`;
2. `wsm_my_lisp_cyberpunk_dll.dll`, зібрана у `wsm-my-lisp`;
3. RED4ext і тестова копія Cyberpunk 2077.

Перший успішний запуск треба зафіксувати в `docs/` реальним фрагментом
логу, а не вважати доказом сам факт успішної збірки.
