(set! *editor-context-table*
  '(#(0 #f              default-face)
	#(3 "\\s*def\\s+|for\\s+|\\s+in\\s+|\\s*class\\s+|\\s+pass\\s+|not\\s+|\\s+if\\s+|\\s+elif\\s+|\\s+else[ :]|lambda[ :]|return[ $]|\\s*from\\s+|\\s*import\\s+|\\s*except\\s+" keyword-face)
	#(3 "[A-Z1-9_]{2,}+" type-face)
	#(3 "^\\s*#\\s*[a-z]+" macro-face)
	#(3 "^\\s*#\\s*[a-z]+\\s+<.*[^>]>" macro-face)
	#(3 "(FIXME|TODO):" important-face)
	#(3 "(\".*?\"|'.*?')" string-face)
	#(1 "#"            comment-face)))
	;#(2 '("\\\"\\\"\\\"" . "\\\"\\\"\\\"") string-face))) 
	;#(3 "(\"\"\".*?\"\"\"|\'\'\'.*?\'\'\'" string-face)))