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
