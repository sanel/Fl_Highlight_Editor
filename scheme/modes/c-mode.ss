(define-mode c-mode
  "Mode for editing C-like languages."
  (syn 'default #f           'default-face)
  (syn 'regex "auto|break|case|continue|default|do|else|enum|extern|for|goto|if|return|sizeof|static|switch|typedef|union|struct|while" 'keyword-face)
  (syn 'regex "(char|const|double|float|int|long|register|short|signed|unsigned|void|volatile|va_list)\\s+" 'type-face)
  (syn 'regex "^\\s*#\\s*\\w+" 'macro-face)
  (syn 'regex "(FIXME|TODO|XXX):"    'important-face)
  (syn 'exact "NULL" 'keyword-face)
  ;; match strings and single characters, including escape sequences
  (syn 'regex "\"([^\"]|\\\\\"|\\\\)*\"" 'string-face)
  (syn 'regex "'(.|\\\\\')'" 'string-face)
  (syn 'eol   "//"           'comment-face)
  (syn 'block '("/*" . "*/") 'comment-face))
