; Fixed-dispatch policy для Running tick (v0 + CP-PLAYER-HANDLE-LIVE).
; C++ доставляє tick і виконує зареєстровані capabilities.
; Цей файл вирішує, чи кликати (запиши-лог) після факту присутності гравця.
; Не додавати сюди closures, state або policy, яку має тримати C++.

(cond
  ((гравець-присутній?) (запиши-лог))
  (t (quote ())))
