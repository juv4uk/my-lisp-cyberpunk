; Fixed-dispatch policy для Running tick (v0 + CP-PLAYER-HANDLE-LIVE).
; C++ доставляє tick і виконує зареєстровані capabilities.
; Цей файл вирішує, чи читати клас гравця після факту присутності гравця.
; Не додавати сюди closures, state або policy, яку має тримати C++.

(cond
  ((гравець-присутній?) (клас гравець))
  (t (quote ())))
