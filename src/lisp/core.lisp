(def! not (lambda (x) (if x false true)))

(def! >= (lambda (x y) (if (= x y) true (> x y))))
(def! <  (lambda (x y) (not (>= x y))))
(def! <= (lambda (x y) (if (= x y) true (< x y))))
(def! negative? (lambda (n) (< n 0)))
(def! positive? (lambda (n) (> n 0)))
(def! zero? (lambda (n) (= n 0)))

(def! first (lambda (seq) (nth seq 0)))

(defmacro! cond (lambda (head & tail)
    `(if ~(first head)
         ~(list-ref head 1)
         ~(if (empty? tail)
              `nil
              (apply cond tail)))))

(defmacro! thunk (lambda (& body)
                      `(lambda () ~@body)))

;; --- lazy values
;; behold the power of LISP that allows you to avoid performing unnecessary checks to
;; know whether the value has been already computed
(def! thunk->lazy (lambda (f)
                    (let* ((h (lambda () 
                                (let* ((val (f)))
                                  (atom-set! g (lambda () val))
                                  val)))
                           (g (atom h)))
                      (lambda ()
                        ((deref g))))))
(defmacro! lazy (lambda (& body)
                     `(make-lazy-thunk (lambda () ~@body))))
