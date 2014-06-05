(define-mode make-mode
  "Mode for editing Make files."
  (syn 'default #f 'default-face)
  (syn 'eol   "#" 'comment-face)
  (syn 'regex "^[^:]*:" 'keyword-face) ;; rule
  (syn 'regex "^\\s*(ifeq|endif|include|echo)\\s+" 'keyword-face) ;; keywords
  (syn 'regex "\\$[@<#\\^]" 'keyword-face) ;; magic variables
  (syn 'regex "\\$\\(\\w+\\)" 'type-face) ;; variable
  (syn 'regex "@\\w+" 'keyword-face) ;; @echo and etc
  (syn 'regex "\\$\\([a-z]+\\s+[^\\)]*\\)" 'keyword-face)) ;; command
