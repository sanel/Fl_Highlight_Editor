
;; expected format is '(face-name #(foreground font size)) and it matches FLTK Style_Table_Entry layout
;(define *editor-face-table* '())
;
;(define *editor-default-foreground* 56) ;; FL_BLACK
;(define *editor-default-size*       12) ;; FL_NORMAL_SIZE
;(define *editor-default-font*        0) ;; FL_HELVETICA
;
;;;; code for handling faces
;
;(define (define-face-lowlevel name lst)
;  (define (validate item)
;    (unless (memq (car item) '(:foreground
;                               :font
;                               :size))
;      (error 'define-face "Received unknown type" (car item) "for face: " name)))
;
;  (define (get-item-or-default item lst default)
;    (if-let1 tmp (assoc item lst)
;      (cadr tmp)
;      default))
;
;  ;; validate we have correct keys
;  (for-each validate lst)
;
;  (let* ([foreground (get-item-or-default ':foreground lst *editor-default-foreground*)]
;         [font       (get-item-or-default ':font lst *editor-default-font*)]
;         [size       (get-item-or-default ':size lst *editor-default-size*)]
;         [found      (assoc name *editor-face-table*)])
;    (if found
;      (set-cdr! found (list (vector foreground font size)))
;      (set! *editor-face-table*
;            (cons (list name (vector foreground font size))
;                  *editor-face-table*)))))
;
;(define (set-face-lowlevel name i value)
;  (if-let1 item (assoc name *editor-face-table*)
;    (vector-set! (cadr item) i value)
;    (error 'set-face-lowlevel "Unable to find face:" name)))
;
;(define-macro (define-face name . args)
;  `(define-face-lowlevel ',name (partition 2 ',args)))
;
;(define (set-face-foreground name value)
;  (set-face-lowlevel name 0 value))
;
;(define (set-face-font name value)
;  (set-face-lowlevel name 1 value))
;
;(define (set-face-size name value)
;  (set-face-lowlevel name 2 value))
;
;;;; code for handling generic-mode
;
;;; *editor-current-mode* is in this form:
;;; '(mode-name mode-string
;;;    #(token-type token-value face-name)
;;;    #(...))
;;;
;;; token-type is deduced like this:
;;;   0 - default color/font
;;;   1 - single comment; token value can be, e.g. '//' and will match to the end of the line
;;;   2 - block; token value is pair in form '(start . end)' and can be used for block comments, like '(/* . */)'
;;;   3 - regex; token value is string for regex compilation
;
;(define *editor-current-mode* #f)
;
;;;; demos
;
;(define-face keyword-face
;  :foreground 10
;  :size 8
;  :font 10)
;
;(define-face comment-face
;  :foreground 72)
;
;(define-face operator-face
;  :foreground 70
;  :size 14
;  :font 10)

(set! *editor-style-table*
  '(#(0 #f              default-face)
	#(1 "//"            comment-face)
	#(2 ("/*" . "*/")   comment-face)
	#(3 "static|return|delete|new|switch|case" keyword-face)
	#(3 "void\\s+|int\\s+|double\\s+|class\\s+|struct\\s+|char\\s+" type-face)
	#(3 "^\\s*#\\s*[a-z]+" macro-face)
	#(3 "(FIXME|TODO):" important-face)
	#(4 "XXX"           keyword-face)
	#(3 "(\".*?\"|'.*?')" string-face)))

(set! *editor-face-table*
  '(#(default-face   56  12 0)
	#(comment-face   216 12 0)
	#(keyword-face   216 12 0)
	#(important-face 216 12 1)
	#(macro-face     72  12 0)
	#(type-face      60  12 0)
	#(string-face    72  12 1)))
