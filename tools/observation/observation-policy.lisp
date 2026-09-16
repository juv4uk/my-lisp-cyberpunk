; observation-policy.lisp — CP-PROBE-MEMORY-V0 / CP-PROBE-SCREEN-V0 (#29/#30)
; decision policy.
;
; This repo's standing rule: host mechanisms only gather raw facts,
; Lisp decides what those facts mean. Until now, memory-probe-v0.ps1
; and screen-probe-v0.ps1 decided `validity` themselves in PowerShell
; branching — that was a policy decision living in the wrong layer.
; This file is that policy, moved to where it belongs. The PowerShell
; scripts now only report `process-found?`/`known-profile?` as raw
; booleans and hand everything else through unmodified.

(define classify-validity
  (lambda (process-found known-profile)
    (cond
      ((eq process-found ()) (quote unavailable))
      ((eq known-profile ()) (quote unsupported-build))
      (t (quote valid)))))

(define make-observation
  (lambda (source game-fingerprint fact value timestamp process-found known-profile provenance)
    (list (quote game-observation/1)
          (list (quote source) source)
          (list (quote game-fingerprint) game-fingerprint)
          (list (quote fact) fact)
          (list (quote value) value)
          (list (quote timestamp) timestamp)
          (list (quote validity) (classify-validity process-found known-profile))
          (list (quote provenance) provenance))))
