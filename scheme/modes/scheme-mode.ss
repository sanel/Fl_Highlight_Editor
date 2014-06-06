(define-mode scheme-mode
  "Mode for editing Scheme language."
  (syn 'default #f 'default-face)
  (syn 'regex "\\s*(define|define-macro|cond|case|if|else|lambda|return|range|define-with-return|for-each|map|apply|set\\!|let\\*|vector|list)\\s+" 'keyword-face)
  (syn 'regex "\\*[a-zA-Z\\-]+\\*" 'important-face)
  (syn 'regex "\"([^\"]|\\\\\"|\\\\)*\"" 'string-face)
  (syn 'regex "[\\(\\)\\[]+" 'parentheses-face)
  (syn 'regex "\]+" 'parentheses-face) ;; strangely, GNU regex will not match this if this was part of above regex
  (syn 'block '("#|" . "|#") 'comment-face)
  (syn 'regex "\"([^\"]|\\\\\"|\\\\)*\"" 'string-face)
  (syn 'eol   ";"  'comment-face))
