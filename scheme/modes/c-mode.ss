(define-mode c-mode
  "Mode for editing C-like languages."
  (syn 'default #f           'default-face)
  (syn 'eol   "//"           'comment-face)
  (syn 'block '("/*" . "*/") 'comment-face)
  (syn 'regex "auto|break|case|continue|default|do|else|enum|extern|for|goto|if|return|sizeof|static|switch|typedef|union|struct|while" 'keyword-face)
  (syn 'regex "char\\s+|const\\s+|double\\s+|float\\s+|int\\s+|long\\s+|register\\s+|short\\s+|signed\\s+|unsigned\\s+|void\\s+|volatile\\s+" 'type-face)
  (syn 'regex "^\\s*#\\s*[a-z]+" 'macro-face)
  (syn 'regex "(FIXME|TODO):"    'important-face)
  (syn 'exact "XXX"              'keyword-face)
  ;; match strings and single characters, including escape sequences
  (syn 'regex "\"([^\"]|\\\\\"|\\\\)*\"" 'string-face)
  (syn 'regex "'(.|\\\\\')'" 'string-face))
