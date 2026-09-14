; Cyberpunk host-operation registry. This repo owns these local IDs; they are
; deliberately separate from my-lisp Canon semantic IDs.
(host-operations/2
  (cp:0001 (semantic-id none) (surface uk запиши-лог) (arity 0) (effect mutate) (input none) (result nil) (owner cyberpunk-host) (ffi LogPrimitive) (status live) (evidence vertical-slice))
  (cp:0002 (semantic-id none) (surface uk гравець-присутній?) (arity 0) (effect inspect) (input none) (result truth) (owner cyberpunk-host) (ffi PlayerPresentPrimitive) (status live) (evidence player-handle-live))
  (cp:0003 (semantic-id none) (surface uk клас) (arity 1) (effect read) (input game-handle) (result string) (owner cyberpunk-host) (ffi ClassPrimitive) (status built) (evidence scenario-player-class))
)
