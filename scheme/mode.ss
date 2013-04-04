
;;; code for handling faces

(define *editor-face-table* '())

;; expected format is: '(face-name '(comments) '(keywords) '((..) (...) other))
(define *editor-current-mode* #f)

;; declared as a table of pairs, so we can 'cdr' second item, instead doing next hop with 'cadr'
(define *editor-fl-color-table*
  '((FL_BLACK    . 56)
	(FL_RED      . 88)
	(FL_GREEN    . 63)
	(FL_YELLOW   . 95)
	(FL_BLUE     . 216)
	(FL_MAGENTA  . 248)
	(FL_CYAN     . 223)
	(FL_DARK_RED . 72)))

(define *editor-fl-font-table*
  '((FL_HELVETICA             . 0)
	(FL_HELVETICA_BOLD        . 1)
	(FL_HELVETICA_ITALIC      . 2)
	(FL_HELVETICA_BOLD_ITALIC . 3)
	(FL_COURIER               . 4)
	(FL_COURIER_BOLD          . 5)
	(FL_COURIER_ITALIC        . 6)
	(FL_COURIER_BOLD_ITALIC   . 7)
	(FL_TIMES                 . 8)
	(FL_TIMES_BOLD            . 9)
	(FL_TIMES_ITALIC          . 10)
	(FL_TIMES_BOLD_ITALIC     . 11)
	(FL_SYMBOL                . 12)
	(FL_SCREEN                . 13)
	(FL_SCREEN_BOLD           . 14)
	(FL_ZAPF_DINGBATS         . 15)))

(define (define-face-lowlevel name lst)
  ;; validate given item and convert colors as they are received as symbols
  (define (validate-and-correct item)
	(unless (memq (car item) '(:background :foreground :font))
	  (error 'define-face "Received unknown type" (car item) "for face: " name)))

  (for-each validate-and-correct lst)

  ;; check if we have registered face with the same name, and just update it if so
  (let1 item (assoc name *editor-face-table*)
    (if item
	  (set-cdr! item (list lst))
	  (set! *editor-face-table*
			(cons (list name lst) *editor-face-table*) ))))

(define-macro (define-face name . args)
  `(define-face-lowlevel ',name (partition 2 ',args)))

;;; code for handling generic-mode

(define-macro (define-generic-mode name comments keywords additional functions documentation)
  `(set! *editor-current-mode*
     (list ',name
		   ,comments
		   (regex-compile (string-join "|" ,keywords) '(RX_EXTENDED))
		   ,additional
		   ,functions
		   ,documentation)))

(define-generic-mode c-mode
  ;; comments
  '("//" ("/*" . "*/"))
  ;; keywords
  '("static" "void" "int" "char" "new" "delete")
  ;; additional
  #f
  #f
  "Mode for C language.")

;;; demos

(define-face keyword-face
  :foreground 88)

(define-face comment-face
  :foreground 72)

(println "!!!!" (length *editor-current-mode*))
