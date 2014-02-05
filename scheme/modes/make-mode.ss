(define-mode make-mode
  "Mode for editing Make files."
  (syn 'default #f 'default-face)
  (syn 'eol   "#" 'comment-face)
  (syn 'regex "^[^:]*:" keyword-face) ;; rule
  (syn 'regex "ifeq\\s+|endif\\s+" keyword-face) ;; keywords
  (syn 'regex "\\$[@<#\\^]" keyword-face) ;; magic variables
  (syn 'regex "\\$\\(\\w+\\)" type-face) ;; variable
  (syn 'regex "\\$\\([a-z]+\\s+[^\\)]*\\)" keyword-face)) ;; command
