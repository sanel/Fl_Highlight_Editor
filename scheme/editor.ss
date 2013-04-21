;;; setup essential editor stuff; called after boot.ss

(require 'utils)
(require 'mode)


;;; hook facility

(define-macro (add-hook! hook func)
  `(set! ,hook (cons ,func ,hook)))

(define (editor-run-hook hook-name hook . args)
  (for-each (lambda (func)
			  (apply func args))
			hook))

(define (editor-print-hook hook)
  (for-each (lambda (func)
			  (println (get-closure-code func)))
			hook))

(add-hook! *editor-before-loadfile-hook*
   (lambda (file)
	 (println "===> loading " file)))

(add-hook! *editor-after-loadfile-hook*
   (lambda (file)
	 (println "===> done " file)
	 (system "uname -a")))
