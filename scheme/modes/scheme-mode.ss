(define-mode scheme-mode
  "Mode for editing Scheme language."
  (syn 'default #f 'default-face)
  (syn 'eol   ";"  'comment-face)
  (syn 'block '("#|" . "|#") 'comment-face)
  (syn 'regex "\\s*(define|define-macro|cond|case|if|else|lambda|return|range|define-with-return|for-each|map|apply|set\\!|let\\*)\\s+" 'keyword-face)
  (syn 'regex "\"([^\"]|\\\\\"|\\\\)*\"" 'string-face))
