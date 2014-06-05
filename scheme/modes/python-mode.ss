(define-mode python-mode
  "Mode for editing Python files."
  (syn 'default #f 'default-face)
  (syn 'regex 
	   (string-append 
		"\\s*def\\s+|for\\s+|\\s+in\\s+|\\s*class\\s+|\\s+pass\\s+|"
		"not\\s+|\\s+if\\s+|\\s+elif\\s+|\\s+else[ :]|lambda[ :]|return[ $]|"
		"\\s*from\\s+|\\s*import\\s+|\\s*except\\s+|\\s*try:")
	   'keyword-face)
  (syn 'regex "[A-Z1-9_]{2,}+" 'type-face)
  (syn 'regex "^\\s*#\\s*[a-z]+" 'macro-face)
  (syn 'regex "^\\s*#\\s*[a-z]+\\s+<.*[^>]>" 'macro-face)
  (syn 'regex "(FIXME|TODO):" 'important-face)
  (syn 'regex "(\".*?\"|'.*?')" 'string-face)
  (syn 'eol "#" 'comment-face))
