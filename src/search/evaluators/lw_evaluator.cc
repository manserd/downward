#include "lw_evaluator.h"

#include "../evaluation_context.h"
#include "../evaluation_result.h"
#include "../plugins/plugin.h"

#define OUT_STYLE "\x1b[92m"
#define OUT_PREFIX "[lw] "
#include "../../out.h"

namespace lw_evaluator {

LWEvaluator::LWEvaluator(const std::shared_ptr<Evaluator>& eval, const std::string &description, utils::Verbosity verbosity) : Evaluator(false, false, false, description, verbosity), evaluator(eval)
{
    outl("created");
}

void LWEvaluator::record_state(const State& state, const StateRecord& record)
{
    per_state_information[state] = record;
}

LWEvaluator::StateRecord LWEvaluator::forget_state(const State& state)
{
    /*
    As far as I can tell, PerStateInformation never erases entries, meaning we're leaking memory here.
    If needed, we can easily move to an own map.
    */
    return per_state_information[state];
}

LWEvaluator::TypeID LWEvaluator::get_or_create_current_improving_type_id(const StateID& state_id, const int low_water_mark_value)
{
    /* If an improving type for the given low-water mark value already exists, use that. */
    const auto map_iterator = current_improving_type_ids.find(low_water_mark_value);
    if (map_iterator != current_improving_type_ids.end()) {
        return map_iterator->second;
    }

    /* Otherwise, create a new improving type for this low-water mark value. */
    const int new_type_id = state_id.hash();
    current_improving_type_ids[low_water_mark_value] = new_type_id;
    return new_type_id;
}

void LWEvaluator::notify_initial_state(const State& /*initial_state*/)
{
    outl("notify_initial_state " << initial_state.get_id());
}

void LWEvaluator::notify_state_transition(const State &parent_state, OperatorID /*op_id*/, const State &/*state*/)
{
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

        /* Cache the new parent's low_water_mark value. We'll need this to determine if successors made progress. */
        current_parent_low_water_mark_value = record.second;

        /* Reset the improving type IDs. */
        current_improving_type_ids.clear();

        outl("* current parent " << parent_state.get_id() << " lw=" << current_parent_low_water_mark_value << " type [" << current_nonimproving_type_id.value() << "]");

    }

    outl("expanding " << parent_state.get_id() << " -> " << state.get_id());
};

EvaluationResult LWEvaluator::compute_result(EvaluationContext &eval_context)
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

    int low_water_mark_value = std::min(current_parent_low_water_mark_value, heuristic_value);

    if (low_water_mark_value < current_parent_low_water_mark_value) {
        outl("-> improves (h'=" << heuristic_value << " < lw=" << current_parent_low_water_mark_value << ")");

        /* This state made progress. Associate it with the improving type. */
        type_id = get_or_create_current_improving_type_id(eval_context.get_state().get_id(), heuristic_value);

    } else {
        outl("-> does not improve (h'=" << heuristic_value << " >= lw=" << current_parent_low_water_mark_value << ")");

        /* This state didn't make progress. Associate it with the non-improving type. */
        type_id = current_nonimproving_type_id;

    }

    assert(type_id != NO_TYPE_ID);

    result.set_evaluator_value(type_id);
    record_state(eval_context.get_state(), StateRecord{type_id, low_water_mark_value});

    outl("=> type [" << result.get_evaluator_value() << "]");

    return result;
}

void LWEvaluator::get_path_dependent_evaluators(std::set<Evaluator*>& evals)
{
    evals.insert(this);
    evaluator->get_path_dependent_evaluators(evals);
}

bool LWEvaluator::dead_ends_are_reliable() const
{
    return evaluator->dead_ends_are_reliable();
}

class LWEvaluatorFeature : public plugins::TypedFeature<Evaluator, LWEvaluator>
{
public:
    LWEvaluatorFeature() : TypedFeature("lw")
    {
        document_subcategory("evaluators_basic");
        document_title("LW evaluator");
        document_synopsis("");

        add_option<std::shared_ptr<Evaluator>>("eval", "evaluator");
        add_evaluator_options_to_feature(*this, "lw");
    }

    std::shared_ptr<LWEvaluator> create_component(const plugins::Options& opts, const utils::Context&) const override
    {
        return plugins::make_shared_from_arg_tuples<LWEvaluator>(opts.get<std::shared_ptr<Evaluator>>("eval"), get_evaluator_arguments_from_options(opts));
    }
};

static plugins::FeaturePlugin<LWEvaluatorFeature> _plugin;
}
