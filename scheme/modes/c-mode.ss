;(set! *editor-context-table*
;  '(#(0 #f              default-face)
;	#(1 "//"            comment-face)
;	#(2 ("/*" . "*/")   comment-face)
;	#(3 "static|return|delete|new|switch|case" keyword-face)
;	#(3 "void\\s+|int\\s+|double\\s+|class\\s+|struct\\s+|char\\s+" type-face)
;	#(3 "^\\s*#\\s*[a-z]+" macro-face)
;	#(3 "^\\s*#\\s*[a-z]+\\s+<.*[^>]>" macro-face)
;	#(3 "(FIXME|TODO):" important-face)
;	#(4 "XXX"           keyword-face)
;	#(3 "(\".*?\"|'.*?')" string-face)))

(define-mode c-mode
  "Mode for editing C-like languages."
  (syn 'default #f            'default-face)
  (syn 'eol    "//"           'comment-face)
  (syn 'block  '("/*" . "*/") 'comment-face)
  (syn 'regex  "static|return|delete|new" 'keyword-face)
  (syn 'regex  "void\\s+|int\\s+|double\\s+|class\\s+|struct\\s+|char\\s+" 'type-face)
  (syn 'regex  "^\\s*#\\s*[a-z]+" 'macro-face)
  (syn 'regex  "(FIXME|TODO):" 'important-face)
  (syn 'exact  "XXX"           'keyword-face)
  (syn 'regex "(\".*?\"|'.*?')" 'string-face))
