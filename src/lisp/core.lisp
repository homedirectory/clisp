(defmacro! defun! 
           (lambda (dfn & exprs)
             (let* ((name (list-ref dfn 0))
                    (params (list-rest dfn)))
               `(def! ~name (lambda ~params ~@exprs)))))
; Examples:
; (defun! (f x y) (+ x y))
; (defun! (do-it) (println "hello world") 42)

(defmacro! and
           (lambda (head & tail)
             ; TODO generate unique symbol name
             `(let* ((_head ~head))
                (if (not _head)
                  false
                  ~(if (empty? tail)
                     '_head
                     (apply and tail))))))

(defmacro! or
           (lambda (head & tail)
             ; TODO generate unique symbol name
             `(let* ((_head ~head))
                (if _head
                  _head
                  ~(if (empty? tail)
                     'false
                     (apply or tail))))))

(defun! (not x) (if x false true))

(defun! (>= x y) (or (= x y) (> x y)))
(defun! (<  x y) (not (>= x y)))
(defun! (<= x y) (not (> x y)))
(defun! (negative? n) (< n 0))
(defun! (positive? n) (> n 0))
(defun! (zero? n) (= n 0))

(defun! (first seq) (nth seq 0))

(defmacro! cond 
           (lambda (head & tail)
             `(if ~(list-ref head 0)
                ~(list-ref head 1)
                ~(if (empty? tail)
                   'nil
                   (apply cond tail)))))

(defmacro! thunk (lambda (& body)
                   `(lambda () ~@body)))

;; --- lazy values
;; behold the power of LISP that allows you to avoid performing unnecessary checks to
;; know whether the value has been already computed
(defun! (thunk->lazy f)
        (let* ((h (lambda () 
                    (let* ((val (f)))
                      (atom-set! g (lambda () val))
                      val)))
               (g (atom h)))
          (lambda ()
            ((deref g)))))

(defmacro! lazy (lambda (& body)
                  `(make-lazy-thunk (lambda () ~@body))))
