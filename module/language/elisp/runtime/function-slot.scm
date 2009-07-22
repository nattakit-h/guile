;;; Guile Emac Lisp

;; Copyright (C) 2001 Free Software Foundation, Inc.

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.
;; 
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;; 
;; You should have received a copy of the GNU General Public License
;; along with this program; see the file COPYING.  If not, write to
;; the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;;; Code:

(define-module (language elisp runtime function-slot)
  #:use-module (language elisp runtime))

; This module contains the function-slots of elisp symbols.  Elisp built-in
; functions are implemented as predefined function bindings here.


; Equivalence and equalness predicates.

(built-in-func eq (lambda (a b)
                    (elisp-bool (eq? a b))))

(built-in-func equal (lambda (a b)
                       (elisp-bool (equal? a b))))


; Number predicates.

(built-in-func floatp (lambda (num)
                        (elisp-bool (and (real? num)
                                         (or (inexact? num)
                                             (prim not (integer? num)))))))

(built-in-func integerp (lambda (num)
                          (elisp-bool (and (exact? num)
                                           (integer? num)))))

(built-in-func numberp (lambda (num)
                         (elisp-bool (real? num))))

(built-in-func wholenump (lambda (num)
                           (elisp-bool (and (exact? num)
                                            (integer? num)
                                            (prim >= num 0)))))

(built-in-func zerop (lambda (num)
                       (elisp-bool (prim = num 0))))


; Number comparisons.

(built-in-func = (lambda (num1 num2)
                   (elisp-bool (prim = num1 num2))))
(built-in-func /= (lambda (num1 num2)
                    (elisp-bool (prim not (prim = num1 num2)))))

(built-in-func < (lambda (num1 num2)
                   (elisp-bool (prim < num1 num2))))
(built-in-func <= (lambda (num1 num2)
                    (elisp-bool (prim <= num1 num2))))
(built-in-func > (lambda (num1 num2)
                   (elisp-bool (prim > num1 num2))))
(built-in-func >= (lambda (num1 num2)
                    (elisp-bool (prim >= num1 num2))))

(built-in-func max (lambda (. nums)
                     (prim apply (@ (guile) max) nums)))
(built-in-func min (lambda (. nums)
                     (prim apply (@ (guile) min) nums)))

(built-in-func abs (@ (guile) abs))


; Number conversion.

(built-in-func float (lambda (num)
                       (if (exact? num)
                         (exact->inexact num)
                         num)))

; TODO: truncate, floor, ceiling, round.


; Arithmetic functions.

(built-in-func 1+ (@ (guile) 1+))
(built-in-func 1- (@ (guile) 1-))
(built-in-func + (@ (guile) +))
(built-in-func - (@ (guile) -))
(built-in-func * (@ (guile) *))
(built-in-func % (@ (guile) modulo))

; TODO: / with correct integer/real behaviour, mod (for floating-piont values).


; Floating-point rounding operations.

(built-in-func ffloor (@ (guile) floor))
(built-in-func fceiling (@ (guile) ceiling))
(built-in-func ftruncate (@ (guile) truncate))
(built-in-func fround (@ (guile) round))


; List predicates.

(built-in-func consp
  (lambda (el)
    (elisp-bool (pair? el))))
(built-in-func atomp
  (lambda (el)
    (elisp-bool (prim not (pair? el)))))

(built-in-func listp
  (lambda (el)
    (elisp-bool (or (pair? el) (null? el)))))
(built-in-func nlistp
  (lambda (el)
    (elisp-bool (and (prim not (pair? el))
                     (prim not (null? el))))))

(built-in-func null
  (lambda (el)
    (elisp-bool (null? el))))


; Accessing list elements.

(built-in-func car
  (lambda (el)
    (if (null? el)
      nil-value
      (prim car el))))
(built-in-func cdr
  (lambda (el)
    (if (null? el)
      nil-value
      (prim cdr el))))

(built-in-func car-safe
  (lambda (el)
    (if (pair? el)
      (prim car el)
      nil-value)))
(built-in-func cdr-safe
  (lambda (el)
    (if (pair? el)
      (prim cdr el)
      nil-value)))

(built-in-func nth
  (lambda (n lst)
    (if (negative? n)
      (prim car lst)
      (let iterate ((i n)
                    (tail lst))
        (cond
          ((null? tail) nil-value)
          ((zero? i) (prim car tail))
          (else (iterate (prim 1- i) (prim cdr tail))))))))
(built-in-func nthcdr
  (lambda (n lst)
    (if (negative? n)
      lst
      (let iterate ((i n)
                    (tail lst))
        (cond
          ((null? tail) nil-value)
          ((zero? i) tail)
          (else (iterate (prim 1- i) (prim cdr tail))))))))


; Building lists.

(built-in-func cons (@ (guile) cons))
(built-in-func list (@ (guile) list))
(built-in-func make-list
  (lambda (len obj)
    (prim make-list len obj)))

(built-in-func append (@ (guile) append))
(built-in-func reverse (@ (guile) reverse))
(built-in-func copy-tree (@ (guile) copy-tree))

(built-in-func number-sequence
  (lambda (from . rest)
    (if (prim > (prim length rest) 2)
      (runtime-error "too many arguments for number-sequence"
                     (prim cdddr rest))
      (if (null? rest)
        `(,from)
        (let ((to (prim car rest))
              (sep (if (or (null? (prim cdr rest))
                           (eq? nil-value (prim cadr rest)))
                     1
                     (prim cadr rest))))
          (cond
            ((or (eq? nil-value to) (prim = to from)) `(,from))
            ((and (zero? sep) (prim not (prim = from to)))
             (runtime-error "infinite list in number-sequence"))
            ((prim < (prim * to sep) (prim * from sep)) '())
            (else
              (let iterate ((i (prim +
                                  from
                                  (prim * sep
                                          (prim quotient
                                            (prim abs (prim - to from))
                                            (prim abs sep)))))
                            (result '()))
                (if (prim = i from)
                  (prim cons i result)
                  (iterate (prim - i sep) (prim cons i result)))))))))))


; Changing lists.

(built-in-func setcar
  (lambda (cell val)
    (prim set-car! cell val)
    val))

(built-in-func setcdr
  (lambda (cell val)
    (prim set-cdr! cell val)
    val))


; Throw can be implemented as built-in function.

(built-in-func throw
  (lambda (tag value)
    (prim throw 'elisp-exception tag value)))


; Miscellaneous.

(built-in-func not (lambda (x)
                     (if x nil-value t-value)))
