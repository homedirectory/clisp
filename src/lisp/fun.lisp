(def! sum-acc 
      (fn* (x acc) 
           (if (= x 0) 
             acc 
             (sum-acc (- x 1) (+ x acc)))))

(def! collatz
      (fn* (n) 
           (cond ((<= n 1)  1)
                 ((even? n) (collatz (/ n 2)))
                 (true      (collatz (+ 1 (* 3 n)))))))
