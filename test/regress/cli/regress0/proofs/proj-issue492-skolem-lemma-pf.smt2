(set-logic ALL)
(set-info :status sat)
(assert (exists ((x (Set (_ BitVec 16)))) (set.member (set.choose (set.inter x x)) (set.inter x x))))
(check-sat)
