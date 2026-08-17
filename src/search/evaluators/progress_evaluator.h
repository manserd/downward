#ifndef EVALUATORS_PROGRESS_EVALUATOR_H
#define EVALUATORS_PROGRESS_EVALUATOR_H

#include "../evaluator.h"

#include <memory>
#include <optional>

namespace plugins {
class Options;
}

#include "../task_proxy.h" // TODO: is this an appropriate way to declare State?

namespace progress_evaluator {
class ProgressEvaluator : public Evaluator
{
    std::shared_ptr<Evaluator> evaluator;

    std::optional<State> parent_state;

public:
    ProgressEvaluator(
        const std::shared_ptr<Evaluator>& eval,
        const std::string& description, utils::Verbosity verbosity);

    virtual bool dead_ends_are_reliable() const override;
    virtual EvaluationResult compute_result(
        EvaluationContext& eval_context) override;
    virtual void get_path_dependent_evaluators(std::set<Evaluator*>& evals) override;
    void notify_state_transition(const State& parent_state_, OperatorID op_id,
                                 const State& state) override;
};
}

#endif
