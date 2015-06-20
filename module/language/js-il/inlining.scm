(define-module (language js-il inlining)
  #:use-module ((srfi srfi-1) #:select (partition))
  #:use-module (ice-9 match)
  #:use-module (language js-il)
  #:export (count-calls
            inline-single-calls
            ))

(define (count-calls exp)
  (define counts (make-hash-table))
  (define (count-inc! key)
    (hashv-set! counts key (+ 1 (hashv-ref counts key 0))))
  (define (count-inf! key)
    (hashv-set! counts key +inf.0))
  (define (analyse-args arg-list)
    (for-each (match-lambda
               (($ kid name)
                (count-inf! name))
               (($ id name) #f))
              arg-list))
  (define (analyse exp)
    (match exp
      (($ program ((ids . funs) ...))
       (for-each analyse funs))

      (($ function self tail body)
       (analyse body))

      (($ jump-table spec)
       (for-each (lambda (p) (analyse (cdr p)))
                 spec))

      (($ continuation params body)
       (analyse body))

      (($ local bindings body)
       (for-each analyse bindings)
       (analyse body))

      (($ var id exp)
       (analyse exp))

      (($ continue ($ kid cont) args)
       (count-inc! cont)
       (for-each analyse args))

      (($ primcall name args)
       (analyse-args args))

      (($ call name ($ kid k) args)
       (count-inf! k)
       (analyse-args args))

      (($ closure ($ kid label) num-free)
       (count-inf! label))

      (($ branch test consequence alternate)
       (analyse test)
       (analyse consequence)
       (analyse alternate))

      (($ kid name)
       (count-inf! name))

      (($ seq body)
       (for-each analyse body))

      (($ prompt escape? tag ($ kid handler))
       (count-inf! handler))

      (else #f)))
  (analyse exp)
  counts)

(define no-values-primitives
  '(define!
    cache-current-module!
    set-cdr!
    set-car!
    vector-set!
    free-set!
    vector-set!/immediate
    box-set!
    struct-set!
    struct-set!/immediate
    wind
    unwind
    push-fluid
    pop-fluid
    ))

(define no-values-primitive?
  (let ((h (make-hash-table)))
    (for-each (lambda (prim)
                (hashv-set! h prim #t))
              no-values-primitives)
    (lambda (prim)
      (hashv-ref h prim))))

(define (inline-single-calls exp)

  (define calls (count-calls exp))

  (define (inlinable? k)
    (eqv? 1 (hashv-ref calls k)))

  (define (split-inlinable bindings)
    (partition (match-lambda
                (($ var ($ kid id) _) (inlinable? id)))
               bindings))

  (define (lookup kont substs)
    (match substs
      ((($ var ($ kid id) exp) . rest)
       (if (= id kont)
           exp
           (lookup kont rest)))
      (() kont)
      (else
       (throw 'lookup-failed kont))))

  (define (inline exp substs)
    (match exp

      ;; FIXME: This hacks around the fact that define doesn't return
      ;; arguments to the continuation. This should be handled when
      ;; converting to js-il, not here.
      (($ continue
          ($ kid (? inlinable? cont))
          (($ primcall (? no-values-primitive? prim) args)))
       (match (lookup cont substs)
         (($ continuation () body)
          (make-seq
           (list
            (make-primcall prim args)
            (inline body substs))))
         (else
          ;; inlinable but not locally bound
          exp)))

      (($ continue ($ kid (? inlinable? cont)) args)
       (match (lookup cont substs)
         (($ continuation kargs body)
          (if (not (= (length args) (length kargs)))
              (throw 'args-dont-match cont args kargs)
              (make-local (map make-var kargs args)
                          ;; gah, this doesn't work
                          ;; identifiers need to be separated earlier
                          ;; not just as part of compilation
                          (inline body substs))))
         (else
          ;; inlinable but not locally bound
          ;; FIXME: This handles tail continuations, but only by accident
          exp)))

      (($ continue cont args)
       exp)

      (($ continuation params body)
       (make-continuation params (inline body substs)))

      (($ local bindings body)
       (call-with-values
           (lambda ()
             (split-inlinable bindings))
         (lambda (new-substs uninlinable-bindings)
           (define substs* (append new-substs substs))
           (make-local (map (lambda (x) (inline x substs*))
                            uninlinable-bindings)
                       (inline body substs*)))))

      (($ var id exp)
       (make-var id (inline exp substs)))

      (($ seq body)
       (make-seq (map (lambda (x) (inline x substs))
                      body)))

      (($ branch test consequence alternate)
       (make-branch test
                    (inline consequence substs)
                    (inline alternate substs)))

      (exp exp)))

  (define (handle-function fun)
    (define (handle-bindings bindings)
      (map (lambda (binding)
             (match binding
               (($ var id ($ continuation params body))
                (make-var id (make-continuation params (inline body '()))))))
           bindings))
    (match fun
      (($ function self tail ($ local bindings ($ jump-table spec)))
       (make-function self
                      tail
                      (make-local (handle-bindings bindings)
                                  (make-jump-table spec))))))

  (match exp
    (($ program ((ids . funs) ...))
     (make-program (map (lambda (id fun)
                          (cons id (handle-function fun)))
                        ids
                        funs)))))
