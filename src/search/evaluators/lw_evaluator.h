#ifndef EVALUATORS_LW_EVALUATOR_H
#define EVALUATORS_LW_EVALUATOR_H

#include "../evaluator.h"
#include "../task_proxy.h"
#include "../per_state_information.h"

namespace lw_evaluator {
class LWEvaluator : public Evaluator
{
    using TypeID = int;
    static constexpr TypeID NO_TYPE_ID = -1;

    using StateRecord = std::pair<TypeID /*associated_type_id*/, int /*low_water_mark_value*/>;

    std::shared_ptr<Evaluator> evaluator;

    StateID current_parent_state_id = StateID::no_state;
    int current_parent_low_water_mark_value = EvaluationResult::INFTY;

    TypeID current_nonimproving_type_id = NO_TYPE_ID;
    utils::HashMap<int, TypeID> current_improving_type_ids;

    PerStateInformation<StateRecord> per_state_information;

    TypeID get_or_create_current_improving_type_id(const StateID& state_id, int low_water_mark_value);

    void record_state(const State& state, const StateRecord& record);
    StateRecord forget_state(const State& state);

public:
    LWEvaluator(const std::shared_ptr<Evaluator>& eval, const std::string& description, utils::Verbosity verbosity);
    void notify_initial_state(const State &initial_state) override;
    void notify_state_transition(const State& parent_state, OperatorID op_id, const State& state) override;
    EvaluationResult compute_result(EvaluationContext& eval_context) override;
    void get_path_dependent_evaluators(std::set<Evaluator*>& evals) override;
    bool dead_ends_are_reliable() const override;
};
}

#endif
