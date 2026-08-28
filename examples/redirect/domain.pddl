; trap GBFS on a plateau from which HI/LW can escape early

(define (domain redirect)
	(:predicates
		(key)
		(start)
		(fake1) (fake2) (fake3) (fake4)
		(real1) (real2) (real3)
	)

	(:action start-fake1 :parameters () :precondition (start) :effect (and (not (start)) (fake1) (not (key))))
	(:action fake1-fake2 :parameters () :precondition (fake1) :effect (and (not (fake1)) (fake2) (key)))
	(:action fake2-fake3 :parameters () :precondition (fake2) :effect (and (not (fake2)) (fake3) (not (key))))
	(:action fake3-fake4 :parameters () :precondition (fake3) :effect (and (not (fake3)) (fake4) (key)))

	(:action start-real1 :parameters () :precondition (start) :effect (and (not (start)) (real1)))
	(:action real1-real2 :parameters () :precondition (real1) :effect (and (not (real1)) (real2)))
	(:action real2-real3 :parameters () :precondition (real2) :effect (and (not (real2)) (real3)))
)
