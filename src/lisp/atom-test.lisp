(def! lst (let* (a (atom 1)) (list (lambda () (swap! a + 1)) (lambda () (deref a)))))
(def! atom-inc (list-ref lst 0))
(def! atom-deref (list-ref lst 1))

(atom-inc)
;>=2
(atom-deref)
;>=2
(atom-inc)
(atom-inc)
(atom-deref)
;>=4
