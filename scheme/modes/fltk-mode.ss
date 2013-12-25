(define-mode fltk-mode
  "Mode for editing FLTK FLUID files."
  (syn 'default #f 'default-face)
  (syn 'eol    "#" 'comment-face)
  (syn 'regex  "version|header_name|code_name|label|xywh" 'keyword-face)
  (syn 'regex  "void\\s+|int\\s+|double\\s+|class\\s+|struct\\s+|char\\s+" 'type-face)
  (syn 'regex  "[0-9]+" 'string-face))
