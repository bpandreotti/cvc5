(set-info :smt-lib-version 2.6)
(set-logic ALL)
(set-info :status sat)

(set-option :re-elim on)
(declare-const x String)
(assert (str.in_re x (re.++ (str.to_re "example-bucket/") (re.* re.allchar) (str.to_re "/") re.allchar re.allchar re.allchar re.allchar re.allchar re.allchar re.allchar re.allchar (str.to_re "-") re.allchar re.allchar re.allchar re.allchar (str.to_re "-") re.allchar re.allchar re.allchar re.allchar (str.to_re "-") re.allchar re.allchar re.allchar re.allchar (str.to_re "-") re.allchar re.allchar re.allchar re.allchar re.allchar re.allchar re.allchar re.allchar re.allchar re.allchar re.allchar re.allchar (str.to_re "/foo"))))
(check-sat)
