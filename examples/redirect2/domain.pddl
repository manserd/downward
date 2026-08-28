; like redirect1 but real and fake have different HI at init
; good with HI and LW but doesn't show HI's weakness: if neither successors of init were new lw, they would still end up in the same type

(define (domain redirect2)
	(:predicates
		(key)
		(init)
		(start)
		(fake1) (fake2) (fake3) (fake4)
		(real1) (real2) (real3)
	)

	(:action init-start :parameters () :precondition (init) :effect (and (not (init)) (start)))
	(:action init-real1 :parameters () :precondition (init) :effect (and (not (init)) (real1)))

	(:action start-fake1 :parameters () :precondition (start) :effect (and (not (start)) (fake1) (not (key))))
	(:action fake1-fake2 :parameters () :precondition (fake1) :effect (and (not (fake1)) (fake2) (key)))
	(:action fake2-fake3 :parameters () :precondition (fake2) :effect (and (not (fake2)) (fake3) (not (key))))
	(:action fake3-fake4 :parameters () :precondition (fake3) :effect (and (not (fake3)) (fake4) (key)))

	(:action real1-real2 :parameters () :precondition (real1) :effect (and (not (real1)) (real2)))
	(:action real2-real3 :parameters () :precondition (real2) :effect (and (not (real2)) (real3)))
)
