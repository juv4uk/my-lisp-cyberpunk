; crosscheck.lisp — CP-OBSERVATION-CROSSCHECK-V0 (#32) decision policy.
;
; Compares one MEMORY (#29) and one SCREEN (#30) game-observation/1
; record for the same live moment and fact, and classifies the pair:
;
;   confirmed — both channels valid, values agree within tolerance
;   partial   — exactly one channel valid; the other's failure is
;               reported, never silently treated as agreement
;   broken    — both channels invalid, or both valid but disagree
;               beyond tolerance (a real discrepancy is reported as
;               broken, not fudged into agreement)
;
; This is deliberately the ONLY place a discrepancy gets judged. It
; never adjusts, averages, or hides an invalid channel's failure --
; both source records are carried through unmodified in the result, so
; the mechanism-side cause can be investigated instead of guessed at
; from the classification symbol alone. Per #32's own acceptance
; criteria: "Виявлена розбіжність не виправляється hardcoded fudge у
; Lisp; причина досліджується на mechanism side."

(define find-field
  (lambda (fields name)
    (cond
      ((atom fields) ())
      ((eq (car (car fields)) name) (car (cdr (car fields))))
      (t (find-field (cdr fields) name)))))

(define obs-field
  (lambda (record name) (find-field (cdr record) name)))

(define obs-valid?
  (lambda (record) (eq (obs-field record (quote validity)) (quote valid))))

(define abs-diff
  (lambda (a b) (cond ((< a b) (- b a)) (t (- a b)))))

(define within-tolerance?
  (lambda (a b tolerance) (cond ((< (abs-diff a b) tolerance) t) (t ()))))

(define classify-crosscheck
  (lambda (memory-rec screen-rec tolerance)
    (cond
      ((obs-valid? memory-rec)
       (cond
         ((obs-valid? screen-rec)
          (cond
            ((within-tolerance? (obs-field memory-rec (quote value))
                                 (obs-field screen-rec (quote value))
                                 tolerance)
             (quote confirmed))
            (t (quote broken))))
         (t (quote partial))))
      ((obs-valid? screen-rec) (quote partial))
      (t (quote broken)))))

(define make-crosscheck
  (lambda (memory-rec screen-rec tolerance)
    (list (quote observation-crosscheck/1)
          (list (quote fact) (obs-field memory-rec (quote fact)))
          (list (quote classification) (classify-crosscheck memory-rec screen-rec tolerance))
          (list (quote tolerance) tolerance)
          (list (quote memory) memory-rec)
          (list (quote screen) screen-rec))))
