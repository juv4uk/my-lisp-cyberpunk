; Сценарій-фікстура для CP-ONE-CHILD-WALK
; Один крок по object graph від PlayerPuppet.
; У грі: отримати новий opaque handle (QuickSlotsManager / inventory тощо).
; Поза грою — oracle-заглушка.

(за-умовою
  ((гравець-присутній?)
   (запиши-лог)
   ; (викликати гравець "GetQuickSlotsManager") → новий handle
   ())
  (t ()))
