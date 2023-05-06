(def! not (fn* (x) (if x false true)))
(def! >= (fn* (x y) (if (= x y) true (> x y))))
(def! <  (fn* (x y) (not (>= x y))))
(def! <= (fn* (x y) (if (= x y) true (< x y))))

(def! first (fn* (seq) (nth seq 0)))

(defmacro! cond (fn* (head & tail)
    `(if ~(first head)
         ~(list-ref head 1)
         ~(if (empty? tail)
              `nil
              (apply cond tail)))))

(def! throw exn)
