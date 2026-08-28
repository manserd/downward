; force GBFS to first explore a dead-end path from which HI/LW can escape early

(define (domain test)
  (:predicates
    (ticket)
    (start)
    (a1) (a2)
    (b1) (b2) (b3)
    (end)
  )

  ; FIXME: VAL requires :parameters for every action. Is there a way around that?

  (:action start-a1 :parameters () :precondition (start) :effect (and (not (start)) (a1) (not (ticket))))
  (:action a1-a2 :parameters () :precondition (a1) :effect (and (not (a1)) (a2) (ticket)))
  (:action a2-end :parameters () :precondition (a2) :effect (and (not (a2)) (not (ticket)) (end)))

  (:action start-b1 :parameters () :precondition (start) :effect (and (not (start)) (b1)))
  (:action b1-b2 :parameters () :precondition (b1) :effect (and (not (b1)) (b2)))
  (:action b2-b3 :parameters () :precondition (b2) :effect (and (not (b2)) (b3)))
  (:action b3-end :parameters () :precondition (b3) :effect (and (not (b3)) (end)))
)
