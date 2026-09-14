#include "v0_open_list.h"

#include "../plugins/plugin.h"
#include "../open_list.h"
#include "../evaluator.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

#define OUT_STYLE "\x1b[91m"
#define OUT_PREFIX "v0 "
#include "../../out.h"

using namespace std;
#include <optional>

namespace v0_open_list {

using BucketKey = vector<int>;

template<class Entry>
struct Bundle
{
    BucketKey key;
    std::vector<Entry> entries;
    size_t indices_index{};
    int minimal_heuristic_value = EvaluationResult::INFTY;
    size_t group_index{};

    Bundle() = default;
};

template<class Entry>
class V0OpenList : public OpenList<Entry>
{
    shared_ptr<Evaluator> heuristic;
    vector<shared_ptr<Evaluator>> evaluators;
    shared_ptr<utils::RandomNumberGenerator> rng;

    std::optional<Entry> parent;

    std::vector<Bundle<Entry>> buckets;
    std::vector<size_t> free_bucket_indices;
    std::vector<size_t> full_bucket_indices;

    utils::HashMap<BucketKey, size_t> bucket_key_to_bucket_index;

    std::vector<int> minimal_heuristic_values;
    utils::HashMap<int, size_t> minimal_heuristic_value_to_index;
    utils::HashMap<int, std::vector<size_t>> minimal_heuristic_value_to_bucket_indices;

    template<class T> bool in(std::vector<T> vector, T value)
    {
        return std::find(vector.begin(), vector.end(), value) != vector.end();
    }

protected:

    void do_insertion(EvaluationContext &eval_context, const Entry &entry) override;

public:

    explicit V0OpenList(const shared_ptr<Evaluator> &heuristic, const vector<shared_ptr<Evaluator>> &evaluators, int random_seed);

    void notify_new_expansion(const Entry& parent_entry) override;
    Entry remove_min() override;
    [[nodiscard]] bool empty() const override;
    void clear() override;
    bool is_dead_end(EvaluationContext &eval_context) const override;
    bool is_reliable_dead_end(EvaluationContext &eval_context) const override;
    void get_path_dependent_evaluators(set<Evaluator *> &evals) override;
};

template<class Entry>
V0OpenList<Entry>::V0OpenList(
    const shared_ptr<Evaluator> &heuristic,
    const vector<shared_ptr<Evaluator>> &evaluators,
    const int random_seed
) : heuristic(heuristic), evaluators(evaluators)
{
    rng = utils::get_rng(random_seed);
    outl("seed " << random_seed);
}

template<class Entry>
void V0OpenList<Entry>::do_insertion(EvaluationContext &eval_context, const Entry &entry)
{
    vector<int> bucket_key;
    bucket_key.reserve(evaluators.size());
    for (const shared_ptr<Evaluator> &evaluator : evaluators) {
        bucket_key.push_back(eval_context.get_evaluator_value_or_infinity(evaluator.get()));
    }
    outl("key is " << bucket_key);

    const int heuristic_value = eval_context.get_evaluator_value_or_infinity(heuristic.get());
    outl("h is " << heuristic_value);

    size_t bucket_index;
    if (const auto i = bucket_key_to_bucket_index.find(bucket_key); i != bucket_key_to_bucket_index.end()) {
        assert(utils::in_bounds(i->second, buckets));
        assert(in(full_bucket_indices, i->second));
        assert(!in(free_bucket_indices, i->second));
        bucket_index = i->second;
        outl("using bucket #" << bucket_index);
    } else {
        if (free_bucket_indices.empty()) {
            free_bucket_indices.push_back(buckets.size());
            buckets.push_back(Bundle<Entry>());
            outl("allocating new bucket");
        }
        bucket_index = free_bucket_indices.back();
        free_bucket_indices.pop_back();
        buckets[bucket_index].indices_index = full_bucket_indices.size();
        buckets[bucket_index].key = bucket_key;
        bucket_key_to_bucket_index[bucket_key] = bucket_index;
        full_bucket_indices.push_back(bucket_index);
        outl("(re)activating bucket #" << bucket_index);
    }

    auto &bucket = buckets[bucket_index];
    bucket.entries.push_back(entry);

    if (heuristic_value < bucket.minimal_heuristic_value) {
        /* Remove bucket from old group (if present). */
        if (bucket.minimal_heuristic_value != EvaluationResult::INFTY) {
            auto &bucket_group = minimal_heuristic_value_to_bucket_indices[bucket.minimal_heuristic_value];

            outl("removing bucket from group h=" << bucket.minimal_heuristic_value);
            assert(utils::in_bounds(bucket.group_index, bucket_group));
            utils::swap_and_pop_from_vector(bucket_group, bucket.group_index);

            /*
            swap_and_pop invalidates the swapped bucket's .group_index. Therefore:

            If the new vector size is greater than the index we swapped (= a swap took place), fix the swapped bucket's .group_index.
            */
            if (bucket.group_index < bucket_group.size()) {
                outl("fixing swapped bucket's .group_index");
                buckets[bucket_group[bucket.group_index]].group_index = bucket.group_index;
            }

            /* If the old bucket group is now empty, remove it from the pool of minimal heuristic values. */
            if (bucket_group.empty()) {
                const size_t swap_index = minimal_heuristic_value_to_index[bucket.minimal_heuristic_value];

                utils::swap_and_pop_from_vector(minimal_heuristic_values, swap_index);

                /* Update the swapped minimal heuristic value's index. */
                if (swap_index < minimal_heuristic_values.size()) {
                    minimal_heuristic_value_to_index[minimal_heuristic_values[swap_index]] = swap_index;
                }

                // TODO: clean up
            }
        }

        /* Add bucket to new group. */
        outl("adding bucket to group h=" << heuristic_value);
        bucket.minimal_heuristic_value = heuristic_value;
        bucket.group_index = minimal_heuristic_value_to_bucket_indices[heuristic_value].size();
        minimal_heuristic_value_to_bucket_indices[heuristic_value].push_back(bucket_index);
    }

    outl("inserted entry");
}

template <class Entry>
void V0OpenList<Entry>::notify_new_expansion(const Entry& parent_entry)
{
    parent.emplace(parent_entry);
}

template<class Entry>
Entry V0OpenList<Entry>::remove_min()
{
    const size_t indices_index = rng->random(full_bucket_indices.size());
    const size_t bucket_index = full_bucket_indices[indices_index];
    auto &bucket = buckets[bucket_index];
    assert(bucket.indices_index == indices_index);
    outl("selected bucket #" << bucket_index);

    const size_t entry_index = rng->random(bucket.entries.size());
    Entry entry = utils::swap_and_pop_from_vector(bucket.entries, entry_index);
    outl("selected entry #" << entry_index);

    if (bucket.entries.empty()) {
        /* 1. Remove bucket from full buckets. */
        outl("deactivating bucket");
        bucket_key_to_bucket_index.erase(bucket.key); // Not sure if this is necessary... probably is though
        assert(utils::in_bounds(bucket.indices_index, full_bucket_indices));
        utils::swap_and_pop_from_vector(full_bucket_indices, bucket.indices_index);

        /*
        swap_and_pop invalidates the swapped bucket's .indices_index. Therefore:

        2. If the new full_bucket_indices size is greater than the index we swapped (= a swap took place), fix the swapped bucket's .indices_index.
        */
        if (bucket.indices_index < full_bucket_indices.size()) {
            outl("fixing swapped bucket's .indices_index");
            buckets[full_bucket_indices[bucket.indices_index]].indices_index = bucket.indices_index;
        }

        /* 3. Add bucket to free buckets. */
        outl("freeing bucket");
        bucket.indices_index = free_bucket_indices.size();
        free_bucket_indices.push_back(bucket_index);
    }

    return entry;
}

template<class Entry>
bool V0OpenList<Entry>::empty() const
{
    return full_bucket_indices.empty();
}

template<class Entry>
void V0OpenList<Entry>::clear()
{
    buckets.clear();
    free_bucket_indices.clear();
    full_bucket_indices.clear();
    bucket_key_to_bucket_index.clear();
}

template<class Entry>
bool V0OpenList<Entry>::is_dead_end(EvaluationContext &eval_context) const
{
    return eval_context.is_evaluator_value_infinite(heuristic.get());
    // FIXME:
}

template<class Entry>
bool V0OpenList<Entry>::is_reliable_dead_end(EvaluationContext &eval_context) const
{
    return is_dead_end(eval_context) && heuristic->dead_ends_are_reliable();
    // FIXME:
}

template<class Entry>
void V0OpenList<Entry>::get_path_dependent_evaluators(set<Evaluator *> &evals)
{
    heuristic->get_path_dependent_evaluators(evals);
    // FIXME:
}

V0OpenListFactory::V0OpenListFactory(
    const shared_ptr<Evaluator> &heuristic,
    const vector<shared_ptr<Evaluator>> &evaluators,
    const int random_seed
) : heuristic(heuristic), evaluators(evaluators), random_seed(random_seed)
{
}

unique_ptr<StateOpenList>
V0OpenListFactory::create_state_open_list()
{
    return utils::make_unique_ptr<V0OpenList<StateOpenListEntry>>(heuristic, evaluators, random_seed);
}

unique_ptr<EdgeOpenList>
V0OpenListFactory::create_edge_open_list()
{
    return utils::make_unique_ptr<V0OpenList<EdgeOpenListEntry>>(heuristic, evaluators, random_seed);
}

class V0OpenListFeature : public plugins::TypedFeature<OpenListFactory, V0OpenListFactory>
{

public:

    V0OpenListFeature() : TypedFeature("v0")
    {
        add_option<shared_ptr<Evaluator>>("heuristic", "");
        add_list_option<shared_ptr<Evaluator>>("evaluators","Evaluators used to determine the bucket for each entry.");
        utils::add_rng_options_to_feature(*this);
    }

    [[nodiscard]] shared_ptr<V0OpenListFactory> create_component(const plugins::Options &opts, const utils::Context &context) const override
    {
        plugins::verify_list_non_empty<shared_ptr<Evaluator>>(context, opts, "evaluators");
        return plugins::make_shared_from_arg_tuples<V0OpenListFactory>(
            opts.get<shared_ptr<Evaluator>>("heuristic"),
            opts.get_list<shared_ptr<Evaluator>>("evaluators"),
            utils::get_rng_arguments_from_options(opts)
        );
    }
};

static plugins::FeaturePlugin<V0OpenListFeature> _plugin;

}
