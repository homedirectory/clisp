(def! sum-acc 
      (fn* (x acc) 
           (if (= x 0) 
             acc 
             (sum-acc (- x 1) (+ x acc)))))

(def! collatz
      (let* ((cltz (fn* (m nums)
                        (cond ((<= m 1)  (cons m nums))
                              ((even? m) (cltz (/ m 2) (cons m nums)))
                              (true      (cltz (+ 1 (* 3 m)) (cons m nums)))))))
        (fn* (n) (cltz n '()))))
