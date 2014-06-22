(define-mode markdown-mode
  "Mode for editing files with markdown syntax."
  (syn 'default #f 'default-face)
  (syn 'eol "#" 'important-face)
  (syn 'block '("```" . "```")  'comment-face)
  (syn 'regex "\\[.*?\\]\\(.*?\\)" 'keyword-face))
