#ifndef EVALUATORS_HI_EVALUATOR_H
#define EVALUATORS_HI_EVALUATOR_H

#include "../evaluator.h"
#include "../task_proxy.h"
#include "../per_state_information.h"

namespace hi_evaluator {
class HIEvaluator : public Evaluator
{
    using TypeID = int;
    static constexpr TypeID NO_TYPE_ID = -1;

    using StateRecord = std::pair<TypeID /*associated_type_id*/, int /*heuristic_value*/>;

    std::shared_ptr<Evaluator> evaluator;

    StateID current_parent_state_id = StateID::no_state;
    int current_parent_heuristic_value = EvaluationResult::INFTY;

    TypeID current_nonimproving_type_id = NO_TYPE_ID;
    TypeID current_improving_type_id = NO_TYPE_ID;

    PerStateInformation<StateRecord> per_state_information;

    void record_state(const State& state, const StateRecord& record);
    StateRecord forget_state(const State& state);

public:
    HIEvaluator(const std::shared_ptr<Evaluator>& eval, const std::string& description, utils::Verbosity verbosity);
    void notify_initial_state(const State &initial_state) override;
    void notify_state_transition(const State& parent_state, OperatorID op_id, const State& state) override;
    EvaluationResult compute_result(EvaluationContext& eval_context) override;
    void get_path_dependent_evaluators(std::set<Evaluator*>& evals) override;
    bool dead_ends_are_reliable() const override;
};
}

#endif
