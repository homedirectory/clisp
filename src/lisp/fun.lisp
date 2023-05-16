(def! sum-acc 
      (lambda (x acc) 
           (if (= x 0) 
             acc 
             (sum-acc (- x 1) (+ x acc)))))

(def! collatz
      (let* ((cltz (lambda (m nums)
                        (cond ((<= m 1)  (cons m nums))
                              ((even? m) (cltz (/ m 2) (cons m nums)))
                              (true      (cltz (+ 1 (* 3 m)) (cons m nums)))))))
        (lambda (n) (cltz n '()))))
