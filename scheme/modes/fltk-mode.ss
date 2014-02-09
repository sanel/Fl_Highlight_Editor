(define-mode fltk-mode
  "Mode for editing FLTK FLUID files."
  (syn 'default #f 'default-face)
  (syn 'regex  "(version|header_name|code_name|label|xywh|image|resizable|type|visible|open|tooltip)\\s+" 'keyword-face)
  (syn 'regex  "(void|int|double|class|struct|char)\\s+" 'type-face)
  (syn 'regex  "(fl|Fl|FL)_[a-zA-Z_0-9]+" 'type-face)
  (syn 'regex  "[A-Z]+_(FRAME|BOX)" 'type-face)
  (syn 'regex  "([0-9]+\.?[0-9]*|[0-9]+)\\s+" 'string-face)
  (syn 'eol    "#" 'comment-face)
  (syn 'eol    "//" 'comment-face)
  (syn 'block '("/*" . "*/") 'comment-face))
