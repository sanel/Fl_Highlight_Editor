;;; setup essential editor stuff; called after boot.ss

(require 'utils)
(require 'mode)

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
			  (apply func args))
			hook))

(define (editor-print-hook hook)
  (for-each (lambda (func)
			  (println (get-closure-code func)))
			hook))

;; (forward-char n)
(define (forward-char n)
  (goto-char (+ n (point))))

(define (backward-char n)
  (goto-char (- (point) n)))

;; (buffer-file-name)

(define *editor-buffer-file-name* #f)

(define (buffer-file-name)
  *editor-buffer-file-name*)

(add-hook! *editor-after-loadfile-hook*
  (lambda (f)
	(set! *editor-buffer-file-name* f)
	(set-tab-width 3)))
