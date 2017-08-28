(define-module (scripts jslink)
  #:use-module (system base compile)
  #:use-module (system base language)
  #:use-module (language javascript)
  #:use-module (srfi srfi-1)
  #:use-module (srfi srfi-37)
  #:use-module (ice-9 format)
  #:use-module (rnrs bytevectors)
  #:use-module (ice-9 binary-ports)
  #:export (jslink))

(define %summary "Link a JS module.")

(define* (copy-port from #:optional (to (current-output-port)) #:key (buffer-size 1024))
  (define bv (make-bytevector buffer-size))
  (let loop ()
    (let ((num-read (get-bytevector-n! from bv 0 buffer-size)))
      (unless (eof-object? num-read)
        (put-bytevector to bv 0 num-read)
        (loop)))))

(define boot-dependencies
  '(("ice-9/posix" . #f)
    ("ice-9/ports" . (ice-9 ports))
    ("ice-9/threads" . (ice-9 threads))
    ("srfi/srfi-4" . (srfi srfi-4))

    ("ice-9/deprecated" . #t)
    ("ice-9/boot-9" . #t)
    ;; FIXME: needs to be at end, or I get strange errors
    ("ice-9/psyntax-pp" . #t)
    ))

(define (fail . messages)
  (format (current-error-port) "error: ~{~a~}~%" messages)
  (exit 1))

(define %options
  (list (option '(#\h "help") #f #f
                (lambda (opt name arg result)
                  (alist-cons 'help? #t result)))

        (option '("version") #f #f
                (lambda (opt name arg result)
                  (show-version)
                  (exit 0)))

        (option '(#\o "output") #t #f
                (lambda (opt name arg result)
                  (if (assoc-ref result 'output-file)
                      (fail "`-o' option cannot be specified more than once")
                      (alist-cons 'output-file arg result))))

        (option '(#\d "depends") #t #f
                (lambda (opt name arg result)
                  (define (read-from-string s)
                    (call-with-input-string s read))
                  (let ((depends (assoc-ref result 'depends)))
                    (alist-cons 'depends (cons (read-from-string arg) depends)
                                result))))

        (option '("no-boot") #f #f
                (lambda (opt name arg result)
                  (alist-cons 'no-boot? #t result)))
        ))

(define (parse-args args)
  "Parse argument list @var{args} and return an alist with all the relevant
options."
  (args-fold args %options
             (lambda (opt name arg result)
               (format (current-error-port) "~A: unrecognized option" name)
               (exit 1))
             (lambda (file result)
               (let ((input-files (assoc-ref result 'input-files)))
                 (alist-cons 'input-files (cons file input-files)
                             result)))

             ;; default option values
             '((input-files)
               (depends)
               (no-boot? . #f)
               )))

(define (show-version)
  (format #t "compile (GNU Guile) ~A~%" (version))
  (format #t "Copyright (C) 2017 Free Software Foundation, Inc.
License LGPLv3+: GNU LGPL version 3 or later <http://gnu.org/licenses/lgpl.html>.
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.~%"))

(define (show-help)
  (format #t "Usage: jslink [OPTION] FILE
Link Javascript FILE with all its dependencies

  -h, --help           print this help message
  -o, --output=OFILE   write output to OFILE
  -o, --depends=DEP    add dependency on DEP

Report bugs to <~A>.~%"
          %guile-bug-report-address))

(define* (link-file file #:key (extra-dependencies '()) output-file no-boot?)
  (let ((dependencies (if no-boot?
                          extra-dependencies
                          (append boot-dependencies extra-dependencies)))
        (output-file (or output-file "main.js")) ;; FIXME: changeable
        )
    (with-output-to-file output-file
      (lambda ()
        (format #t "(function () {\n")
        (link-runtime)
        (format #t "/* ---------- end of runtime ---------- */\n")
        (for-each (lambda (x)
                    (let ((path (car x))
                          (file (cdr x)))
                      (link-dependency path file))
                    (format #t "/* ---------- */\n"))
                  dependencies)
        (format #t "/* ---------- end of dependencies ---------- */\n")
        (link-main file no-boot?)
        (format #t "})();")
        output-file))))

(define *runtime-file* (%search-load-path "language/js-il/runtime.js"))

(define (link-runtime)
  (call-with-input-file *runtime-file* copy-port))

(define (link-dependency path file)
  (define (compile-dependency file)
    (call-with-input-file file
      (lambda (in)
        ((language-printer (lookup-language 'javascript))
         (read-and-compile in
                           #:from 'scheme
                           #:to 'javascript
                           #:env (default-environment (lookup-language 'scheme)))
         (current-output-port)))))
  (format #t "boot_modules[~s] =\n" path)
  (cond ((string? file)
         (compile-dependency file))
        ((list? file)
         (print-statement (compile `(define-module ,file)
                                   #:from 'scheme #:to 'javascript)
                          (current-output-port))
         (newline))
        (file (compile-dependency (%search-load-path path)))
        (else
         (format #t "function (cont) { return cont(scheme.UNDEFINED); };")))
  (newline))

(define (link-main file no-boot?)
  ;; FIXME: continuation should be changeable with a switch
  (call-with-input-file file
    (lambda (in)
      (format #t "var main =\n")
      (copy-port in)
      (newline)
      (if no-boot?
          (format #t "main(scheme.initial_cont);\n")
          (format #t "boot_modules[\"ice-9/boot-9\"](function() {return main((function (x) {console.log(x); return x; }));});"))))) ; scheme.initial_cont

(define (jslink . args)
  (let* ((options      (parse-args args))
         (help?        (assoc-ref options 'help?))
         (dependencies (assoc-ref options 'depends))
         (input-files  (assoc-ref options 'input-files))
         (output-file  (assoc-ref options 'output-file))
         (no-boot?     (assoc-ref options 'no-boot?)))

    (if (or help? (null? input-files))
        (begin (show-help) (exit 0)))

    (unless (null? (cdr input-files))
      (fail "can only link one file at a time"))
    (format #t "wrote `~A'\n"
            (link-file (car input-files)
                       #:extra-dependencies dependencies
                       #:output-file output-file
                       #:no-boot? no-boot?))))

(define main jslink)
