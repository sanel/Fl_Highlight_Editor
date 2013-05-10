(set! *editor-context-table*
  '(#(0 #f          default-face)
	#(3 "\\*.*?\\*" macro-face)
	#(3 "[\\(\\)]+" type-face)
	#(3 "\\s*(define|define-macro|cond|case|if|else|lambda|return|range|define-with-return|for-each|map|apply|set\\!|let\\*)\\s+" keyword-face)
	#(3 "\".*?\"" string-face)
	#(1 ";" comment-face)))
