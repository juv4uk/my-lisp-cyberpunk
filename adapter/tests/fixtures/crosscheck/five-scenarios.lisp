; CP-OBSERVATION-CROSSCHECK-V0 (#32) required test scenarios, in the
; exact order #32's "Required tests" section lists them. Each is a
; hand-authored (memory-record, screen-record) pair; the expected
; classification list this file's final expression must evaluate to is
; (confirmed confirmed partial partial broken) -- verified by
; ObservationCrosscheckFixturesTest.cmake against a real run through
; canonical my-lisp, not asserted here as a comment alone.

; 1. Stable state: both channels valid, agree within tolerance.
(define mem-1 (quote (game-observation/1 (source memory) (fact player-health-ratio) (value 41/50) (validity valid))))
(define scr-1 (quote (game-observation/1 (source screen) (fact player-health-ratio) (value 4/5) (validity valid))))

; 2. Transition: the fact genuinely changes; both channels still agree
;    on the new value.
(define mem-2 (quote (game-observation/1 (source memory) (fact player-health-ratio) (value 1/2) (validity valid))))
(define scr-2 (quote (game-observation/1 (source screen) (fact player-health-ratio) (value 51/100) (validity valid))))

; 3. MEMORY stale-profile injection: memory channel becomes
;    unsupported-build, SCREEN stays valid. Must not be masked as
;    agreement.
(define mem-3 (quote (game-observation/1 (source memory) (fact player-health-ratio) (value ()) (validity unsupported-build))))
(define scr-3 (quote (game-observation/1 (source screen) (fact player-health-ratio) (value 4/5) (validity valid))))

; 4. SCREEN invalid calibration / HUD hidden: screen channel becomes
;    unavailable, MEMORY stays valid. Must not be masked either.
(define mem-4 (quote (game-observation/1 (source memory) (fact player-health-ratio) (value 4/5) (validity valid))))
(define scr-4 (quote (game-observation/1 (source screen) (fact player-health-ratio) (value ()) (validity unavailable))))

; 5. A real discrepancy beyond tolerance between two otherwise-valid
;    channels. Must be reported as broken, not fudged into agreement.
(define mem-5 (quote (game-observation/1 (source memory) (fact player-health-ratio) (value 1/10) (validity valid))))
(define scr-5 (quote (game-observation/1 (source screen) (fact player-health-ratio) (value 9/10) (validity valid))))

(define tolerance 1/20)

(list
  (classify-crosscheck mem-1 scr-1 tolerance)
  (classify-crosscheck mem-2 scr-2 tolerance)
  (classify-crosscheck mem-3 scr-3 tolerance)
  (classify-crosscheck mem-4 scr-4 tolerance)
  (classify-crosscheck mem-5 scr-5 tolerance))
