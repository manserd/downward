#include "progress_evaluator.h"

#include "../evaluation_context.h"
#include "../evaluation_result.h"
#include "../plugins/plugin.h"

#include <cstdlib>
#include <sstream>

#define out(...) cout << "\x1b[95m[pe] " << __VA_ARGS__ << "\x1b[0m"
#define outl(...) out(__VA_ARGS__) << endl

using namespace std;

namespace progress_evaluator {
ProgressEvaluator::ProgressEvaluator(
    const shared_ptr<Evaluator>& eval,
    const string& description, utils::Verbosity verbosity)
    : Evaluator(false, false, false, description, verbosity),
      evaluator(eval)
{
    outl("created");
}


bool ProgressEvaluator::dead_ends_are_reliable() const
{
    return evaluator->dead_ends_are_reliable();
}

EvaluationResult ProgressEvaluator::compute_result(
    EvaluationContext& eval_context)
{
    EvaluationResult result;
    int parent_value = EvaluationResult::INFTY;
    if (parent_state.has_value()) {
        EvaluationContext parent_eval_context(parent_state.value());
        parent_value = parent_eval_context.get_evaluator_value_or_infinity(evaluator.get());
    }

    const int value = eval_context.get_evaluator_value_or_infinity(evaluator.get());

    const int progress = parent_value - value;

    result.set_evaluator_value(progress);

    return result;
}

void ProgressEvaluator::get_path_dependent_evaluators(set<Evaluator*>& evals)
{
    evals.insert(this);
    evaluator->get_path_dependent_evaluators(evals);
}

void ProgressEvaluator::notify_state_transition(const State& parent_state_, OperatorID /*op_id*/,
                                                const State& /*state*/)
{
    this->parent_state.emplace(parent_state_);
};

class ProgressEvaluatorFeature
    : public plugins::TypedFeature<Evaluator, ProgressEvaluator>
{
public:
    ProgressEvaluatorFeature() : TypedFeature("progress")
    {
        document_subcategory("evaluators_basic");
        document_title("Progress evaluator");
        document_synopsis("");

        add_option<shared_ptr<Evaluator>>("eval", "evaluator");
        add_evaluator_options_to_feature(*this, "progress");
    }

    virtual shared_ptr<ProgressEvaluator> create_component(
        const plugins::Options& opts,
        const utils::Context&) const override
    {
        return plugins::make_shared_from_arg_tuples<ProgressEvaluator>(
            opts.get<shared_ptr<Evaluator>>("eval"),
            get_evaluator_arguments_from_options(opts)
        );
    }
};

static plugins::FeaturePlugin<ProgressEvaluatorFeature> _plugin;
}
