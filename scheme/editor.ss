;;
;; Fl_Highlight_Editor - extensible text editing widget
;; Copyright (c) 2013-2014 Sanel Zukan.
;;
;; This library is free software; you can redistribute it and/or
;; modify it under the terms of the GNU Lesser General Public
;; License as published by the Free Software Foundation; either
;; version 2 of the License, or (at your option) any later version.
;;
;; This library is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
;; Lesser General Public License for more details.
;;
;; You should have received a copy of the GNU Lesser General Public License
;; along with this library. If not, see <http://www.gnu.org/licenses/>.

;;; setup essential editor stuff; called after boot.ss

(require 'utils)
(require 'constants)

;;; globals

(define *editor-buffer-file-name* #f)
(define *editor-current-mode* #f)

;;; hook facility

(define-macro (add-hook! hook func)
  `(set! ,hook
        (reverse ;; reverse it, so oldest functions are called first
         (cons ,func ,hook))))

(define-macro (add-to-list! list elem)
  `(set! ,list
         (cons ,elem ,list)))

(define-macro (add-to-list-once! list elem)
  `(if (null? (filter (lambda (x)
                        (eqv? x ,elem))
                      ,list))
     (set! ,list (cons ,elem ,list))))

(define (editor-run-hook hook-name hook . args)
  (for-each (lambda (func)
              (if (null? args)
                (func)
                (apply func args)))
            hook))

(define (editor-print-hook hook)
  (for-each (lambda (func)
              (println (get-closure-code func)))
            hook))

;; search for a file in *load-path*; returns full path or #f if not found
(define-with-return (editor-find-file file)
  (for-each
    (lambda (path)
      (let ([path (string-append path "/" file)])
        (if (file-exists? path)
          (return path))))
    *load-path*)
  #f)

;; (forward-char n)
(define (forward-char n)
  (goto-char (+ n (point))))

;; (backward-char n)
(define (backward-char n)
  (goto-char (- (point) n)))

;; (buffer-file-name)

(define (buffer-file-name)
  *editor-buffer-file-name*)

;;; (define-mode)

(define (quoted? val)
  (and (pair? val)
       (eq? 'quote (car val))))

;(define-macro (define-mode mode doc . args)
;  `(let ([syn (lambda (type what face)
;              (vector
;                (case type
;                  [(default) 0]
;                  [(eol) 1]
;                  [(block) 2]
;                  [(regex) 3]
;                  [(exact) 4]
;                  [else (error 'syn "Unrecognized context type:" type "in expression" ',args)])
;               what face))])
;    (set! *editor-context-table* (list ,@args))))

(define (define-mode-lowlevel mode doc . args)
  (let1 ret '()
    (for-each
      (lambda (x)
        (case (car x)
          ;; syn part, in form (syn context-type context face)
          [(syn)
           (unless (= 4 (length x))
             (error 'define-mode "syn token expect 4 elements, but got: " x))

           ;; parse 'syn' list; here quoting elements is allowed (it can be confusing for user what
           ;; is quoted and not) and if done so, we just unquote them
           (let* ([item (cadr x)]
                  [item (if (quoted? item) (eval item) item)]
                  [num (case item
                        [(default) 0]
                        [(eol)     1]
                        [(block)   2]
                        [(regex)   3]
                        [(exact)   4]
                        [else
                          (error 'define-mode "Unrecognized context type:" (cadr x) "in expression:" x)])]
                  [what (let1 tmp (list-ref x 2)
                          (cond
                            [(quoted? tmp) (eval tmp)]
                            [else tmp]))]
                  [face (list-ref x 3)]
                  [face (if (quoted? face) (eval face) face)])
             (add-to-list! ret (vector num what face)))]

          ;; callbacks, executed before hook is started and after was started
          ;; TODO: not completed yet so we just ignore it for now
          [(before after) #t]
          [else
            (error 'define-mode "Got unrecognized token:" (car x) "in expression:" x)]))
      (car args) )
    ;; set global table in reverse order, so the table represent the order as set here
    (set! *editor-context-table* (reverse ret)) ))

(define-macro (define-mode mode doc . args)
  `(define-mode-lowlevel ',mode ,doc ',args))

(define (editor-try-load-mode mode)
  (if (and *editor-current-mode*
           (string=? *editor-current-mode* mode))
      #t ;; return true so we knows mode is already loaded
      (let1 path (editor-find-file (string-append mode ".ss"))
        (if path
          (begin
            (println "Loading mode " mode)
            (load path)
            ;; returns string (mode-name) so we knows mode is newly loaded
            (set! *editor-current-mode* mode))
          ;; return false if mode wasn't found
          #f))))

;; load given mode by matching against filename
(define-with-return (editor-try-load-mode-by-filename lst filename)
  (for-each
    (lambda (item)
      (let* ([rx     (regex-compile (car item) '(RX_EXTENDED))]
             [match? (and rx (regex-match rx filename))])
        (when match?
          (let* ([mode (cond
                         [(string? (cdr item)) (cdr item)]
                         [(symbol? (cdr item)) (symbol->string (cdr item))]
                         [else
                           (error "Mode name not string or symbol")])]
                 [state (editor-try-load-mode mode)])
            (when state
              (if (eq? state #t)
                (return #t)
                (begin
                  (editor-repaint-context-changed)
                  (editor-repaint-face-changed)

                  ;; now load <mode-name>-hook variable if defined
                  (let* ([mode-hook-str (string-append mode "-hook")]
                         [mode-hook-sym (string->symbol mode-hook-str)])
                    (when (defined? mode-hook-sym)
                      (editor-run-hook mode-hook-str (eval mode-hook-sym))))
                  (return #t))))))))
    lst))

;;; file types

(define *editor-auto-mode-table*
  '(("(\\.[CchH]|\\.[hc]pp|\\.[hc]xx|\\.[hc]\\+\\+|\\.CC)$" . c-mode)
    ("(\\.py|\\.pyc)$" . python-mode)
    ("(\\.md)$" . markdown-mode)
    ("(\\.fl)$" . fltk-mode)
    ("(\\.ss|\\.scm|\\.scheme)$" . scheme-mode)
    ("(\\.ht|\\.htm|\\.html|\\.xhtml|\\.sgml)$" . html-mode)
    ("([mM]akefile|GNUMakefile|\\.make)$" . make-mode)))

;; FIXME: register 'modes' subfolder; do this better
(add-to-list! *load-path* (string-append (car *load-path*) "/modes"))

(add-hook! *editor-before-loadfile-hook*
  (lambda (f)
    (set! *editor-buffer-file-name* f)
    (editor-try-load-mode-by-filename *editor-auto-mode-table* f)))

(add-hook! *editor-after-loadfile-hook*
  (lambda (f)
    (set-tab-width 4)))

;;; default themes

(define (default-lite-theme)
  (editor-set-background-color FL_WHITE)
  (set! *editor-face-table*
    (list
      (vector 'default-face FL_BLACK 12 FL_COURIER)
      (vector 'comment-face FL_BLUE  12 FL_COURIER)
      (vector 'keyword-face FL_BLUE  12 FL_COURIER)
      (vector 'important-face FL_BLUE 12 FL_COURIER_BOLD)
      (vector 'type-face FL_DARK_GREEN 12 FL_COURIER)
      (vector 'attribute-face FL_DARK_CYAN 12 FL_COURIER)
      (vector 'string-face FL_DARK_RED 12 FL_COURIER)
      (vector 'macro-face  FL_DARK_CYAN 12 FL_COURIER))))

(define (default-dark-theme)
  (editor-set-background-color FL_BLACK)
  (set! *editor-face-table*
    (list
      (vector 'default-face FL_GREEN 12 FL_COURIER)
      (vector 'keyword-face FL_BLUE   12 FL_COURIER))))

(default-lite-theme)

;;; default faces

;(editor-set-background-color FL_LIGHT1)
;(set! *editor-face-table*
;  '(#(default-face   56  12 0)
;    #(comment-face   216 12 0)
;    #(keyword-face   216 12 0)
;    #(important-face 216 12 1)
;    #(macro-face     72  12 0)
;    #(type-face      60  12 0)
;    #(string-face    72  12 0)))
