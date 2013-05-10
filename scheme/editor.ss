;;
;; Fl_Highlight_Editor - extensible text editing widget
;; Copyright (c) 2013 Sanel Zukan.
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

;; (forward-char n)
(define (forward-char n)
  (goto-char (+ n (point))))

;; (backward-char n)
(define (backward-char n)
  (goto-char (- (point) n)))

;; (buffer-file-name)

(define (buffer-file-name)
  *editor-buffer-file-name*)

;; load given mode by matching against filename
(define-with-return (editor-try-load-mode-by-filename lst filename)
  (for-each
    (lambda (item)
	  (let* ([rx     (regex-compile (car item) '(RX_EXTENDED))]
			 [match? (and rx (regex-match rx filename))])
		(when match?
		  (let ([mode (cond
						[(string? (cdr item)) (cdr item)]
						[(symbol? (cdr item)) (symbol->string (cdr item))]
						[else
						  (error "Mode name not string or symbol")])])

			(if (and *editor-current-mode*
					 (string=? *editor-current-mode* mode))
			  (return #t)
			  (let* ([path    (string-append "scheme/modes/" mode ".ss")]
					 [exists? (file-exists? path)])
				(when exists?
				  (println "Loading mode " mode)
				  (load path)
				  (editor-repaint-context-changed)
				  (editor-repaint-face-changed)
				  (set! *editor-current-mode* mode)

				  ;; now load <mode-name>-hook variable if defined
				  (let1 mode-sym (string->symbol (string-append mode "-hook"))
					(when (defined? mode-sym)
					  (editor-run-hook (symbol->string mode-sym) (eval mode-sym))))

				  (return #t) ) ) ) ) ) ) )
	lst))

(set! *editor-face-table*
  '(#(default-face   56  12 0)
	#(comment-face   216 12 0)
	#(keyword-face   216 12 0)
	#(important-face 216 12 1)
	#(macro-face     72  12 0)
	#(type-face      60  12 0)
	#(string-face    72  12 0)))

;;; file types

(define *editor-auto-mode-table*
  '(("(\\.[CchH]|\\.[hc]pp|\\.[hc]xx|\\.[hc]\\+\\+|\\.CC)$" . c-mode)
	("(\\.py|\\.pyc)$" . python-mode)
	("(\\.md)$" . markdown-mode)
	("(\\.ss|\\.scm|\\.scheme)$" . scheme-mode)))

(add-hook! *editor-before-loadfile-hook*
  (lambda (f)
	(set! *editor-buffer-file-name* f)
	(editor-try-load-mode-by-filename *editor-auto-mode-table* f)))
