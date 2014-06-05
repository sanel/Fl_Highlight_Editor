(define-mode html-mode
  "Mode for editing HTML."
  (syn 'default #f 'default-face)
  (syn 'regex "(xml:lang|xmlns)" 'important-face)
  (syn 'regex "<(\\w+)" 'keyword-face)
  (syn 'regex "</\\w+>" 'keyword-face)
  (syn 'regex "&\\w+;" 'type-face) ;; entity names
  (syn 'regex "&#[0-9]+;" 'type-face) ;; codes
  (syn 'regex "/\?>" 'keyword-face)
  (syn 'regex "\\s+(class|style)\\s*=" 'attribute-face)
  ;; minimal matching with string escape support
  (syn 'regex "\"([^\"]|\\\\\"|\\\\)*\"" 'string-face)
  ;; comment comes last
  (syn 'block '("<!--" . "-->") 'comment-face))
