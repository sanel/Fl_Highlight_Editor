
;; TODO: many of these functions can be easily implemented as C routines

(define-macro (let1 a b . body)
  `(let ([,a ,b])
     ,@body))

(define-macro (if-let1 a b . body)
  `(let ([,a ,b])
     (if ,a
       ,@body)))

(define (take n lst)
  (if (or (equal? lst '())
          (<= n 0))
    (list)
    (cons (car lst)
          (take (- n 1) (cdr lst)))))

(define (drop n lst)
  (if (or (equal? lst '())
          (<= n 0))
    lst
    (drop (- n 1) (cdr lst)))) 

(define (split-at n lst)
  (list (take n lst)
        (drop n lst)))

(define (partition n lst)
  (let* ([s (take n lst)]
         [l (length s)])
    (if (= n l)
      (cons s (partition n (drop n lst)))
      (list))))

(define (filter pred seq)
  (cond
    [(null? seq) '()]
    [(pred (car seq))
     (cons (car seq)
           (filter pred (cdr seq)))]
    [else
      (filter pred (cdr seq))]))

(define (print . args)
  (for-each display args))

(define (println . args)
  (for-each display args)
  (newline))

(define (string-join delim lst)
  (define (interpose delim lst)
    (if (null? lst)
        lst
        (cons delim
              (cons (car lst) (interpose delim (cdr lst))))))
  (if (null? lst)
    ""
    (apply string-append (cdr (interpose delim lst)))))
