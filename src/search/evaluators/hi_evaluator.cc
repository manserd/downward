#include "hi_evaluator.h"

#include "../evaluation_context.h"
#include "../evaluation_result.h"
#include "../plugins/plugin.h"

#define OUT_STYLE "\x1b[95m"
#define OUT_PREFIX "[hi] "
#include "../../out.h"

namespace hi_evaluator {

HIEvaluator::HIEvaluator(const std::shared_ptr<Evaluator>& eval, const std::string &description, utils::Verbosity verbosity) : Evaluator(false, false, false, description, verbosity), evaluator(eval)
{
    outl("created");
}

void HIEvaluator::record_state(const State& state, const StateRecord& record)
{
    per_state_information[state] = record;
}

HIEvaluator::StateRecord HIEvaluator::forget_state(const State& state)
{
    /*
    As far as I can tell, PerStateInformation never erases entries, meaning we're leaking memory here.
    If needed, we can easily move to an own map.
    */
    return per_state_information[state];
}

void HIEvaluator::notify_initial_state(const State &initial_state)
{
    outl("notify_initial_state " << initial_state.get_id());

    /*
    compute_result() for the initial state will need a value for current_improving_type_id, since
    that's the type it will be associated with (the initial state is always an improving state). For
    this reason, we set current_improving_type_id to the initial state's ID here.
    */
    current_improving_type_id = initial_state.get_id().hash();
}

void HIEvaluator::notify_state_transition(const State &parent_state, OperatorID /*op_id*/, const State &state)
{
    outl("expanding " << parent_state.get_id() << " -> " << state.get_id());

    /*
    We use this mechanism to detect when a new expansion starts: when this is the case,
    we cache some values which we'll need later.
    */

    if (parent_state.get_id() != current_parent_state_id) {

        /* Update our cached parent state ID. */
        current_parent_state_id = parent_state.get_id();

        const StateRecord record = forget_state(parent_state);

        /* Cache the new parent's type ID. We will associate its non-improving successors with this type. */
        current_nonimproving_type_id = record.first;

        /* Cache the new parent's heuristic value. We'll need this to determine if successors made progress. */
        current_parent_heuristic_value = record.second;

        /* Use this first successor's ID as the ID of the type with which we'll associate improving successors. */
        current_improving_type_id = state.get_id().hash();

    }
};

EvaluationResult HIEvaluator::compute_result(EvaluationContext &eval_context)
{
    const int heuristic_value = eval_context.get_evaluator_value_or_infinity(evaluator.get());

    EvaluationResult result;

    /*
    We want to avoid keeping state records for dead ends, because since those states will never
    be expanded, we won't have the chance to erase them, creating a memory leak.
    */
    if (heuristic_value == EvaluationResult::INFTY) {
        outl("-> infty");

        result.set_evaluator_value(EvaluationResult::INFTY);
        return result;
    }

    TypeID type_id;

    if (heuristic_value < current_parent_heuristic_value) {
        outl("-> improves");

        /* This state made progress. Associate it with the improving type. */
        type_id = current_improving_type_id;

    } else {
        outl("-> does not improve");

        /* This state didn't make progress. Associate it with the non-improving type. */
        type_id = current_nonimproving_type_id;

    }

    assert(type_id != NO_TYPE_ID);

    result.set_evaluator_value(type_id);
    record_state(eval_context.get_state(), StateRecord{type_id, heuristic_value});

    outl("=> type [" << result.get_evaluator_value() << "]");

    return result;
}

void HIEvaluator::get_path_dependent_evaluators(std::set<Evaluator*>& evals)
{
    evals.insert(this);
    evaluator->get_path_dependent_evaluators(evals);
}

bool HIEvaluator::dead_ends_are_reliable() const
{
    return evaluator->dead_ends_are_reliable();
}

class HIEvaluatorFeature : public plugins::TypedFeature<Evaluator, HIEvaluator>
{
public:
    HIEvaluatorFeature() : TypedFeature("hi")
    {
        document_subcategory("evaluators_basic");
        document_title("HI evaluator");
        document_synopsis("");

        add_option<std::shared_ptr<Evaluator>>("eval", "evaluator");
        add_evaluator_options_to_feature(*this, "hi");
    }

    std::shared_ptr<HIEvaluator> create_component(const plugins::Options& opts, const utils::Context&) const override
    {
        return plugins::make_shared_from_arg_tuples<HIEvaluator>(opts.get<std::shared_ptr<Evaluator>>("eval"), get_evaluator_arguments_from_options(opts));
    }
};

static plugins::FeaturePlugin<HIEvaluatorFeature> _plugin;
}
