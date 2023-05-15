(def! not (fn* (x) (if x false true)))

(def! >= (fn* (x y) (if (= x y) true (> x y))))
(def! <  (fn* (x y) (not (>= x y))))
(def! <= (fn* (x y) (if (= x y) true (< x y))))
(def! negative? (fn* (n) (< n 0)))
(def! positive? (fn* (n) (> n 0)))
(def! zero? (fn* (n) (= n 0)))

(def! first (fn* (seq) (nth seq 0)))

(defmacro! cond (fn* (head & tail)
    `(if ~(first head)
         ~(list-ref head 1)
         ~(if (empty? tail)
              `nil
              (apply cond tail)))))

(defmacro! thunk (fn* (& body)
                      `(fn* () ~@body)))

;; --- lazy values
;; behold the power of LISP that allows you to avoid performing unnecessary checks to
;; know whether the value has been already computed
(def! thunk->lazy (fn* (f)
                       (let* ((h (fn* () 
                                      (let* ((val (f)))
                                        (do
                                          (atom-set! g (fn* () val))
                                          val))))
                              (g (atom h)))
                         (fn* ()
                              ((deref g))))))
(defmacro! lazy (fn* (& body)
                     `(make-lazy-thunk (fn* () ~@body))))
