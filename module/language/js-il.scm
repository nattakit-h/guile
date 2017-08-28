;;; JavaScript Intermediate Language (JS-IL)

;; Copyright (C) 2015, 2017 Free Software Foundation, Inc.

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

;;; Code:

(define-module (language js-il)
  #:use-module (srfi srfi-9)
  #:use-module (srfi srfi-9 gnu)
  #:use-module (ice-9 match)
  #:export (make-program program
            make-function function
            make-params params
            make-continuation continuation
            make-local local
            make-continue continue
            make-const const
            make-primcall primcall
            make-call call
            make-closure closure
            make-branch branch
            make-id id
            make-kid kid
            make-seq seq
            make-prompt prompt
            ))

;; Copied from (language cps)
;; Should put in a srfi 99 module
(define-syntax define-record-type*
  (lambda (x)
    (define (id-append ctx . syms)
      (datum->syntax ctx (apply symbol-append (map syntax->datum syms))))
    (syntax-case x ()
      ((_ name field ...)
       (and (identifier? #'name) (and-map identifier? #'(field ...)))
       (with-syntax ((cons (id-append #'name #'make- #'name))
                     (pred (id-append #'name #'name #'?))
                     ((getter ...) (map (lambda (f)
                                          (id-append f #'name #'- f))
                                        #'(field ...))))
         #'(define-record-type name
             (cons field ...)
             pred
             (field getter)
             ...))))))

;; TODO: add type predicates to fields so I can only construct valid
;; objects
(define-syntax-rule (define-js-type name field ...)
  (begin
    (define-record-type* name field ...)
    (set-record-type-printer! name print-js)))

(define (print-js exp port)
  (format port "#<js-il ~S>" (unparse-js exp)))

(define-js-type program body)
(define-js-type function self tail clauses)
(define-js-type params self req opt rest kw allow-other-keys?)
(define-js-type continuation params body)
(define-js-type local bindings body) ; local scope
(define-js-type continue cont args)
(define-js-type const value)
(define-js-type primcall name args)
(define-js-type call name k args)
(define-js-type closure label num-free)
(define-js-type branch test consequence alternate)
(define-js-type id name)
(define-js-type kid name)
(define-js-type seq body)
(define-js-type prompt escape? tag handler)

(define (unparse-js exp)
  (match exp
    (($ program body)
     `(program . ,(map (match-lambda
                        ((($ kid k) . fun)
                         (cons k (unparse-js fun))))
                       body)))
    (($ continuation params body)
     `(continuation ,(map unparse-js params) ,(unparse-js body)))
    (($ function ($ id self) ($ kid tail) clauses)
     `(function ,self
                ,tail
                ,@(map (match-lambda
                        ((($ kid id) params kont)
                         (list id
                               (unparse-js params)
                               (unparse-js kont))))
                       clauses)))
    (($ params ($ id self) req opt rest kws allow-other-keys?)
     `(params ,self
              ,(map unparse-js req)
              ,(map unparse-js opt)
              ,(and rest (unparse-js rest))
              ,(map (match-lambda
                     ((kw ($ id name) ($ id sym))
                      (list kw name sym)))
                    kws)
              ,allow-other-keys?))
    (($ local bindings body)
     `(local ,(map (match-lambda
                    ((a . d)
                     (cons (unparse-js a)
                           (unparse-js d))))
                   bindings)
             ,(unparse-js body)))
    (($ continue ($ kid k) args)
     `(continue ,k ,(map unparse-js args)))
    (($ branch test then else)
     `(if ,(unparse-js test) ,(unparse-js then) ,(unparse-js else)))
    (($ const c)
     `(const ,c))
    (($ primcall name args)
     `(primcall ,name ,(map unparse-js args)))
    (($ call ($ id name) ($ kid k) args)
     `(call ,name ,k ,(map unparse-js args)))
    (($ closure ($ kid label) nfree)
     `(closure ,label ,nfree))
    (($ id name)
     `(id . ,name))
    (($ kid name)
     `(kid . ,name))))
