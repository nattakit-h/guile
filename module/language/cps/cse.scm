;;; Continuation-passing style (CPS) intermediate language (IL)

;; Copyright (C) 2013-2020 Free Software Foundation, Inc.

;;;; This library is free software; you can redistribute it and/or
;;;; modify it under the terms of the GNU Lesser General Public
;;;; License as published by the Free Software Foundation; either
;;;; version 3 of the License, or (at your option) any later version.
;;;;
;;;; This library is distributed in the hope that it will be useful,
;;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;;;; Lesser General Public License for more details.
;;;;
;;;; You should have received a copy of the GNU Lesser General Public
;;;; License along with this library; if not, write to the Free Software
;;;; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

;;; Commentary:
;;;
;;; Common subexpression elimination for CPS.
;;;
;;; Code:

(define-module (language cps cse)
  #:use-module (ice-9 match)
  #:use-module (srfi srfi-1)
  #:use-module (srfi srfi-9)
  #:use-module (srfi srfi-11)
  #:use-module (language cps)
  #:use-module (language cps utils)
  #:use-module (language cps effects-analysis)
  #:use-module (language cps intmap)
  #:use-module (language cps intset)
  #:use-module (language cps renumber)
  #:export (eliminate-common-subexpressions))

(define (compute-available-expressions succs kfun clobbers)
  "Compute and return a map of LABEL->ANCESTOR..., where ANCESTOR... is
an intset containing ancestor labels whose value is available at LABEL."
  (let ((init (intmap-map (lambda (label succs) #f) succs))
        (kill clobbers)
        (gen (intmap-map (lambda (label succs) (intset label)) succs))
        (subtract (lambda (in-1 kill-1)
                    (if in-1
                        (intset-subtract in-1 kill-1)
                        empty-intset)))
        (add intset-union)
        (meet (lambda (in-1 in-1*)
                (if in-1
                    (intset-intersect in-1 in-1*)
                    in-1*))))
    (let ((in (intmap-replace init kfun empty-intset))
          (out init)
          (worklist (intset kfun)))
      (solve-flow-equations succs in out kill gen subtract add meet worklist))))

(define (intset-pop set)
  (match (intset-next set)
    (#f (values set #f))
    (i (values (intset-remove set i) i))))

(define-syntax-rule (make-worklist-folder* seed ...)
  (lambda (f worklist seed ...)
    (let lp ((worklist worklist) (seed seed) ...)
      (call-with-values (lambda () (intset-pop worklist))
        (lambda (worklist i)
          (if i
              (call-with-values (lambda () (f i seed ...))
                (lambda (i* seed ...)
                  (let add ((i* i*) (worklist worklist))
                    (match i*
                      (() (lp worklist seed ...))
                      ((i . i*) (add i* (intset-add worklist i)))))))
              (values seed ...)))))))

(define worklist-fold*
  (case-lambda
    ((f worklist seed)
     ((make-worklist-folder* seed) f worklist seed))))

(define (compute-truthy-expressions conts kfun)
  "Compute a \"truth map\", indicating which expressions can be shown to
be true and/or false at each label in the function starting at KFUN.
Returns an intmap of intsets.  The even elements of the intset indicate
labels that may be true, and the odd ones indicate those that may be
false.  It could be that both true and false proofs are available."
  (define (true-idx label) (ash label 1))
  (define (false-idx label) (1+ (ash label 1)))

  (define (propagate boolv succ out)
    (let* ((in (intmap-ref boolv succ (lambda (_) #f)))
           (in* (if in (intset-union in out) out)))
      (if (eq? in in*)
          (values '() boolv)
          (values (list succ)
                  (intmap-add boolv succ in* (lambda (old new) new))))))

  (define (visit-cont label boolv)
    (let ((in (intmap-ref boolv label)))
      (define (propagate0)
        (values '() boolv))
      (define (propagate1 succ)
        (propagate boolv succ in))
      (define (propagate2 succ0 succ1)
        (let*-values (((changed0 boolv) (propagate boolv succ0 in))
                      ((changed1 boolv) (propagate boolv succ1 in)))
          (values (append changed0 changed1) boolv)))
      (define (propagate-branch succ0 succ1)
        (let*-values (((changed0 boolv)
                       (propagate boolv succ0
                                  (intset-add in (false-idx label))))
                      ((changed1 boolv)
                       (propagate boolv succ1
                                  (intset-add in (true-idx label)))))
          (values (append changed0 changed1) boolv)))

      (match (intmap-ref conts label)
        (($ $kargs names vars term)
         (match term
           (($ $continue k)   (propagate1 k))
           (($ $branch kf kt) (propagate-branch kf kt))
           (($ $prompt k kh)  (propagate2 k kh))
           (($ $throw)        (propagate0))))
        (($ $kreceive arity k)
         (propagate1 k))
        (($ $kfun src meta self tail clause)
         (if clause
             (propagate1 clause)
             (propagate0)))
        (($ $kclause arity kbody kalt)
         (if kalt
             (propagate2 kbody kalt)
             (propagate1 kbody)))
        (($ $ktail) (propagate0)))))

  (worklist-fold* visit-cont
                  (intset kfun)
                  (intmap-add empty-intmap kfun empty-intset)))

(define-record-type <analysis>
  (make-analysis effects clobbers preds avail truthy-labels)
  analysis?
  (effects analysis-effects)
  (clobbers analysis-clobbers)
  (preds analysis-preds)
  (avail analysis-avail)
  (truthy-labels analysis-truthy-labels))

;; When we determine that we can replace an expression with
;; already-bound variables, we change the expression to a $values.  At
;; its continuation, if it turns out that the $values expression is the
;; only predecessor, we elide the predecessor, to make redundant branch
;; folding easier.  Ideally, elision results in redundant branches
;; having multiple predecessors which already have values for the
;; branch.
;;
;; We could avoid elision, and instead search backwards when we get to a
;; branch that we'd like to elide.  However it's gnarly: branch elisions
;; reconfigure the control-flow graph, and thus affect the avail /
;; truthy maps.  If we forwarded such a distant predecessor, if there
;; were no intermediate definitions, we'd have to replay the flow
;; analysis from far away.  Maybe it's possible but it's not obvious.
;;
;; The elision mechanism is to rewrite predecessors to continue to the
;; successor.  We could have instead replaced the predecessor with the
;; body of the successor, but that would invalidate the values of the
;; avail / truthy maps, as well as the clobber sets.
;;
;; We can't always elide the predecessor though.  If any of the
;; predecessor's predecessors is a back-edge, it hasn't been
;; residualized yet and so we can't rewrite it.  This is an
;; implementation limitation.
;;
(define (elide-predecessor label pred out analysis)
  (match analysis
    (($ <analysis> effects clobbers preds avail truthy-labels)
     (let ((pred-preds (intmap-ref preds pred)))
       (and
        ;; Don't elide predecessors that are the targets of back-edges.
        (< (intset-prev pred-preds) pred)
        (cons
         (intset-fold
          (lambda (pred-pred out)
            (define (rename k) (if (eqv? k pred) label k))
            (intmap-replace!
             out pred-pred
             (rewrite-cont (intmap-ref out pred-pred)
               (($ $kargs names vals ($ $continue k src exp))
                ($kargs names vals ($continue (rename k) src ,exp)))
               (($ $kargs names vals ($ $branch kf kt src op param args))
                ($kargs names vals ($branch (rename kf) (rename kt) src op param args)))
               (($ $kargs names vals ($ $prompt k kh src escape? tag))
                ($kargs names vals ($prompt (rename k) (rename kh) src escape? tag)))
               (($ $kreceive ($ $arity req () rest () #f) kbody)
                ($kreceive req rest (rename kbody)))
               (($ $kclause arity kbody kalternate)
                ;; Can only be a body continuation.
                ($kclause ,arity (rename kbody) kalternate)))))
          pred-preds
          (intmap-remove out pred))
         (make-analysis effects
                        clobbers
                        (intmap-add (intmap-add preds label pred intset-remove)
                                    label pred-preds intset-union)
                        avail
                        truthy-labels)))))))

(define (prune-branch analysis pred succ)
  (match analysis
    (($ <analysis> effects clobbers preds avail truthy-labels)
     (make-analysis effects
                    clobbers
                    (intmap-add preds succ pred intset-remove)
                    avail
                    truthy-labels))))

(define (prune-successors analysis pred succs)
  (intset-fold (lambda (succ analysis)
                 (prune-branch analysis pred succ))
               succs analysis))

(define (term-successors term)
  (match term
    (($ $continue k) (intset k))
    (($ $branch kf kt) (intset kf kt))
    (($ $prompt k kh) (intset k kh))
    (($ $throw) empty-intset)))

(define (eliminate-common-subexpressions-in-fun kfun conts out substs)
  (define equiv-set (make-hash-table))
  (define (true-idx idx) (ash idx 1))
  (define (false-idx idx) (1+ (ash idx 1)))
  (define (subst-var substs var)
    (intmap-ref substs var (lambda (var) var)))
  (define (subst-vars substs vars)
    (let lp ((vars vars))
      (match vars
        (() '())
        ((var . vars) (cons (subst-var substs var) (lp vars))))))

  (define (compute-term-key term)
    (match term
      (($ $continue k src exp)
       (match exp
         (($ $const val)                       (cons 'const val))
         (($ $prim name)                       (cons 'prim name))
         (($ $fun body)                        #f)
         (($ $rec names syms funs)             #f)
         (($ $const-fun label)                 #f)
         (($ $code label)                      (cons 'code label))
         (($ $call proc args)                  #f)
         (($ $callk k proc args)               #f)
         (($ $primcall name param args)        (cons* name param args))
         (($ $values args)                     #f)))
      (($ $branch kf kt src op param args)     (cons* op param args))
      (($ $prompt)                             #f)
      (($ $throw)                              #f)))

  (define (add-auxiliary-definitions! label defs substs term-key)
    (define (add-def! aux-key var)
      (let ((equiv (hash-ref equiv-set aux-key '())))
        (hash-set! equiv-set aux-key
                   (acons label (list var) equiv))))
    (define-syntax add-definitions
      (syntax-rules (<-)
        ((add-definitions)
         #f)
        ((add-definitions
          ((def <- op arg ...) (aux <- op* arg* ...) ...)
          . clauses)
         (match term-key
           (('op arg ...)
            (match defs
              (#f
               ;; If the successor is a control-flow join, don't
               ;; pretend to know the values of its defs.
               #f)
              ((def) (add-def! (list 'op* arg* ...) aux) ...)))
           (_ (add-definitions . clauses))))
        ((add-definitions
          ((op arg ...) (aux <- op* arg* ...) ...)
          . clauses)
         (match term-key
           (('op arg ...)
            (add-def! (list 'op* arg* ...) aux) ...)
           (_ (add-definitions . clauses))))))
    (add-definitions
     ((scm-set! p s i x)               (x <- scm-ref p s i))
     ((scm-set!/tag p s x)             (x <- scm-ref/tag p s))
     ((scm-set!/immediate p s x)       (x <- scm-ref/immediate p s))
     ((word-set! p s i x)              (x <- word-ref p s i))
     ((word-set!/immediate p s x)      (x <- word-ref/immediate p s))
     ((pointer-set!/immediate p s x)   (x <- pointer-ref/immediate p s))

     ((u <- scm->f64 #f s)             (s <- f64->scm #f u))
     ((s <- f64->scm #f u)             (u <- scm->f64 #f s))
     ((u <- scm->u64 #f s)             (s <- u64->scm #f u))
     ((s <- u64->scm #f u)             (u <- scm->u64 #f s)
      (u <- scm->u64/truncate #f s))
     ((s <- u64->scm/unlikely #f u)    (u <- scm->u64 #f s)
      (u <- scm->u64/truncate #f s))
     ((u <- scm->s64 #f s)             (s <- s64->scm #f u))
     ((s <- s64->scm #f u)             (u <- scm->s64 #f s))
     ((s <- s64->scm/unlikely #f u)    (u <- scm->s64 #f s))
     ((u <- untag-fixnum #f s)         (s <- s64->scm #f u)
      (s <- tag-fixnum #f u))
     ;; NB: These definitions rely on U having top 2 bits equal to
     ;; 3rd (sign) bit.
     ((s <- tag-fixnum #f u)           (u <- scm->s64 #f s)
      (u <- untag-fixnum #f s))
     ((s <- u64->s64 #f u)             (u <- s64->u64 #f s))
     ((u <- s64->u64 #f s)             (s <- u64->s64 #f u))

     ((u <- untag-char #f s)           (s <- tag-char #f u))
     ((s <- tag-char #f u)             (u <- untag-char #f s))))

  (define (rename-uses term substs)
    (define (subst-var var)
      (intmap-ref substs var (lambda (var) var)))
    (define (rename-exp exp)
      (rewrite-exp exp
        ((or ($ $const) ($ $prim) ($ $fun) ($ $rec) ($ $const-fun) ($ $code))
         ,exp)
        (($ $call proc args)
         ($call (subst-var proc) ,(map subst-var args)))
        (($ $callk k proc args)
         ($callk k (and proc (subst-var proc)) ,(map subst-var args)))
        (($ $primcall name param args)
         ($primcall name param ,(map subst-var args)))
        (($ $values args)
         ($values ,(map subst-var args)))))
    (rewrite-term term
      (($ $branch kf kt src op param args)
       ($branch kf kt src op param ,(map subst-var args)))
      (($ $continue k src exp)
       ($continue k src ,(rename-exp exp)))
      (($ $prompt k kh src escape? tag)
       ($prompt k kh src escape? (subst-var tag)))
      (($ $throw src op param args)
       ($throw src op param ,(map subst-var args)))))

  (define (visit-term label term substs analysis)
    (let* ((term (rename-uses term substs)))
      (define (residualize)
        (values term analysis))
      (define (eliminate k src vals)
        (values (build-term ($continue k src ($values vals))) analysis))
      (define (fold-branch true? kf kt src)
        (values (build-term ($continue (if true? kt kf) src ($values ())))
                (prune-branch analysis label (if true? kf kt))))

      (match (compute-term-key term)
        (#f (residualize))
        (term-key
         (match analysis
           (($ <analysis> effects clobbers preds avail truthy-labels)
            (let ((avail (intmap-ref avail label)))
              (let lp ((candidates (hash-ref equiv-set term-key '())))
                (match candidates
                  (()
                   ;; No available expression; residualize.
                   (residualize))
                  (((candidate . vars) . candidates)
                   (cond
                    ((not (intset-ref avail candidate))
                     ;; This expression isn't available here; try
                     ;; the next one.
                     (lp candidates))
                    (else
                     (match term
                       (($ $continue k src)
                        ;; Yay, a match; eliminate the expression.
                        (eliminate k src vars))
                       (($ $branch kf kt src)
                        (let* ((bool (intmap-ref truthy-labels label))
                               (t (intset-ref bool (true-idx candidate)))
                               (f (intset-ref bool (false-idx candidate))))
                          (if (eqv? t f)
                              ;; Can't fold the branch; keep on
                              ;; looking for another candidate.
                              (lp candidates)
                              ;; Nice, the branch folded.
                              (fold-branch t kf kt src)))))))))))))))))

  (define (visit-label label cont out substs analysis)
    (match cont
      (($ $kargs names vars term)
       (define (visit-term* names vars out substs analysis)
         (call-with-values (lambda ()
                             (visit-term label term substs analysis))
           (lambda (term analysis)
             (values (intmap-add! out label
                                  (build-cont ($kargs names vars ,term)))
                     substs
                     analysis))))
       (define (visit-term-normally)
         (visit-term* names vars out substs analysis))
       (match analysis
         (($ <analysis> effects clobbers preds avail truthy-labels)
          (let ((preds (intmap-ref preds label)))
            (cond
             ((eq? preds empty-intset)
              ;; Branch folding made this term unreachable.  Prune from
              ;; preds set.
              (values out substs
                      (prune-successors analysis label (term-successors term))))
             ((trivial-intset preds)
              => (lambda (pred)
                   (match (intmap-ref out pred)
                     (($ $kargs names' vars' ($ $continue _ _ ($ $values vals)))
                      ;; Substitute dominating definitions, and try to elide the
                      ;; predecessor entirely.
                      (let ((substs (fold (lambda (var val substs)
                                            (intmap-add substs var val))
                                          substs vars vals)))
                        (match (elide-predecessor label pred out analysis)
                          (#f
                           ;; Can't elide; predecessor must be target of
                           ;; backwards branch.
                           (visit-term* names vars out substs analysis))
                          ((out . analysis)
                           (visit-term* names' vars' out substs analysis)))))
                     (($ $kargs _ _ term)
                      (match (compute-term-key term)
                        (#f #f)
                        (term-key
                         (let ((fx (intmap-ref effects pred)))
                           ;; Add residualized definition to the equivalence set.
                           ;; Note that expressions that allocate a fresh object
                           ;; or change the current fluid environment can't be
                           ;; eliminated by CSE (though DCE might do it if the
                           ;; value proves to be unused, in the allocation case).
                           (when (and (not (causes-effect? fx &allocation))
                                      (not (effect-clobbers? fx (&read-object &fluid))))
                             (let ((equiv (hash-ref equiv-set term-key '())))
                               (hash-set! equiv-set term-key (acons pred vars equiv)))))
                         ;; If the predecessor defines auxiliary definitions, as
                         ;; `cons' does for the results of `car' and `cdr', define
                         ;; those as well.
                         (add-auxiliary-definitions! pred vars substs term-key)))
                      (visit-term-normally))
                     (_
                      (visit-term-normally)))))
             (else
              (visit-term-normally)))))))
      (_ (values (intmap-add! out label cont) substs analysis))))

  ;; Because of the renumber pass, the labels are numbered in reverse
  ;; post-order, so the intmap-fold will visit definitions before
  ;; uses.
  (let* ((effects (synthesize-definition-effects (compute-effects conts)))
         (clobbers (compute-clobber-map effects))
         (succs (compute-successors conts kfun))
         (preds (invert-graph succs))
         (avail (compute-available-expressions succs kfun clobbers))
         (truthy-labels (compute-truthy-expressions conts kfun)))
    (call-with-values
        (lambda ()
          (intmap-fold visit-label conts out substs
                       (make-analysis effects clobbers preds avail truthy-labels)))
      (lambda (out substs analysis)
        (values out substs)))))

(define (fold-renumbered-functions f conts . seeds)
  ;; Precondition: CONTS has been renumbered, and therefore functions
  ;; contained within it are topologically sorted, and the conts of each
  ;; function's body are numbered sequentially after the function's
  ;; $kfun.
  (define (next-function-body kfun)
    (match (intmap-ref conts kfun (lambda (_) #f))
      (#f #f)
      ((and cont ($ $kfun))
       (let lp ((k (1+ kfun)) (body (intmap-add! empty-intmap kfun cont)))
         (match (intmap-ref conts k (lambda (_) #f))
           ((or #f ($ $kfun))
            (persistent-intmap body))
           (cont
            (lp (1+ k) (intmap-add! body k cont))))))))

  (let fold ((kfun 0) (seeds seeds))
    (match (next-function-body kfun)
      (#f (apply values seeds))
      (conts
       (call-with-values (lambda () (apply f kfun conts seeds))
         (lambda seeds
           (fold (1+ (intmap-prev conts)) seeds)))))))

(define (eliminate-common-subexpressions conts)
  (let ((conts (renumber conts 0)))
    (persistent-intmap
     (fold-renumbered-functions eliminate-common-subexpressions-in-fun
                                conts empty-intmap empty-intmap))))
