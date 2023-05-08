;; factorial
(def! ! (fn* (n) (if (< n 2) 1 (* n (! (- n 1))))))

;; proper isolation of procedure envs
;(def! f1 (fn* (x) (f2)))
;(def! f2 (fn* () x))
;(f1 10)
;/.*\'?x\'? not found.*

;; capturing of procedure env
(def! make-adder (fn* (x) (fn* (n) (+ x n))))
(def! inc (make-adder 1))
(inc 1)
;=>2
(inc 1)
;=>2

(def! make-adder1 (fn* (x) (let* (y x) (fn* (n) (+ y n)))))
(def! inc1 (make-adder1 1))
(inc1 1)
;=>2
(inc1 1)
;=>2

(println "hello from test.mal") ; and this is a comment
