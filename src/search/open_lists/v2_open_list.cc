#include "v2_open_list.h"

#include "../plugins/plugin.h"
#include "../open_list.h"
#include "../evaluator.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

#define OUT_STYLE "\x1b[91m"
#define OUT_PREFIX "v2 "
#include <ranges>

#include "../../out.h"

using namespace std;
#include <optional>

namespace v2_open_list {

static constexpr double TAU = 1.0;

template<typename T> size_t uniform_sample(const shared_ptr<utils::RandomNumberGenerator>& rng, const std::vector<T>& values)
{
    return rng->random(static_cast<int>(values.size()));
}

template<typename T> size_t softmin_sample(const shared_ptr<utils::RandomNumberGenerator>& rng, const std::vector<T>& values)
{
    std::vector<double> weights;
    double total_weight = 0;
    for (const auto& value : values) {
        double weight = std::exp(-value / TAU);
        weights.push_back(weight);
        total_weight += weight;
    }

    double x = rng->random() * total_weight;
    size_t values_index = 0;
    while (values_index + 1 < values.size() && x > weights[values_index]) {
        x -= weights[values_index];
        ++values_index;
    }

    return values_index;
}

using BucketKey = std::vector<int>;
using BucketID = BucketKey;
template<typename Entry> class Buckets;
template<typename Entry> class BucketGroup;
template<typename Entry> class Bucket;
template<typename Entry> class Family;

template<typename Entry>
class Family
{
    std::vector<Entry> entries;

public:
    void adopt_entry(Entry entry)
    {
        entries.push_back(entry);
    }
};

template<typename Entry>
class Bucket
{
    utils::HashMap<int, Family<Entry>> family_by_family_id;

public:
    void family_adopt_entry(const int family_id, Entry entry)
    {
        auto& family = family_by_family_id[family_id];
        family.adopt_entry(entry);
    }
};

template<typename Entry>
class BucketGroup
{
    std::vector<BucketID> bucket_ids;
    utils::HashMap<BucketID, size_t> bucket_ids_index_by_bucket_id;

public:
    void adopt_stray_bucket(const BucketID bucket_id)
    {
        bucket_ids_index_by_bucket_id[bucket_id] = bucket_ids.size();
        bucket_ids.push_back(bucket_id);
    }

    void abandon_bucket(const BucketID bucket_id)
    {
        if (
            const size_t current_bucket_ids_index = bucket_ids_index_by_bucket_id.at(bucket_id);
            current_bucket_ids_index != bucket_ids.size() - 1
        ) {
            bucket_ids.at(current_bucket_ids_index) = std::move(bucket_ids.back());
            bucket_ids_index_by_bucket_id.at(bucket_ids.back()) = current_bucket_ids_index;
        }
        bucket_ids.pop_back();
        bucket_ids_index_by_bucket_id.erase(bucket_id);
    }
};

template<typename Entry>
class Buckets
{
    std::vector<Bucket<Entry>> buckets;
    utils::HashMap<BucketID, size_t> buckets_index_by_bucket_id;
    utils::HashMap<int, BucketGroup<Entry>> group_by_group_id;
    utils::HashMap<BucketID, int> group_id_by_bucket_id;

public:
    bool has_bucket(const BucketID bucket_id)
    {
        const auto i = buckets_index_by_bucket_id.find(bucket_id);
        return i != buckets_index_by_bucket_id.end();
    }

    Bucket<Entry>& get_bucket(const BucketID bucket_id)
    {
        const size_t bucket_index = buckets_index_by_bucket_id[bucket_id];
        return buckets[bucket_index];
    }

    void create_stray_bucket(const BucketID bucket_id)
    {
        const size_t bucket_index = buckets.size();
        buckets.push_back(Bucket<Entry>(bucket_id));
        buckets_index_by_bucket_id[bucket_id] = bucket_index;
        return buckets.back();
    }

    void group_adopt_stray_bucket(const int group_id, const BucketID bucket_id)
    {
        auto& group = group_by_group_id[group_id];
        group.adopt_stray_bucket(bucket_id);
        group_id_by_bucket_id[bucket_id] = group_id;
    }

    void add_to_bucket(const BucketID bucket_id, Entry entry)
    {
        auto& bucket = get_bucket(bucket_id);
    }
};

template<typename Entry>
class V2OpenList : public OpenList<Entry>
{
    shared_ptr<Evaluator> heuristic;
    vector<shared_ptr<Evaluator>> evaluators;
    shared_ptr<utils::RandomNumberGenerator> rng;

    std::optional<Entry> parent;

    Buckets<Entry> buckets;

    int depth_placeholder = 0;

protected:

    void do_insertion(EvaluationContext &eval_context, const Entry &entry) override;

public:

    explicit V2OpenList(const shared_ptr<Evaluator> &heuristic, const vector<shared_ptr<Evaluator>> &evaluators, int random_seed);

    void notify_new_expansion(const Entry& parent_entry) override;
    Entry remove_min() override;
    [[nodiscard]] bool empty() const override;
    void clear() override;
    bool is_dead_end(EvaluationContext &eval_context) const override;
    bool is_reliable_dead_end(EvaluationContext &eval_context) const override;
    void get_path_dependent_evaluators(set<Evaluator *> &evals) override;
};

template<typename Entry>
V2OpenList<Entry>::V2OpenList(
    const shared_ptr<Evaluator> &heuristic,
    const vector<shared_ptr<Evaluator>> &evaluators,
    const int random_seed
) : heuristic(heuristic), evaluators(evaluators)
{
    rng = utils::get_rng(random_seed);
    outl("seed " << random_seed);
}

template<typename Entry>
void V2OpenList<Entry>::do_insertion(EvaluationContext &eval_context, const Entry &entry)
{
    /* Determine bucket key. */
    BucketKey bucket_key;
    bucket_key.reserve(evaluators.size());
    for (const shared_ptr<Evaluator> &evaluator : evaluators) {
        bucket_key.push_back(eval_context.get_evaluator_value_or_infinity(evaluator.get()));
    }
    const BucketID bucket_id = bucket_key;

    /* Determine h. */
    const int h = eval_context.get_evaluator_value_or_infinity(heuristic.get());

    if (!buckets.has_bucket(bucket_id)) {
        buckets.create_stray_bucket(bucket_id);
        buckets.group_adopt_stray_bucket(h, bucket_id);
    }
    buckets.add_to_bucket(bucket_id, entry);
}

template <typename Entry>
void V2OpenList<Entry>::notify_new_expansion(const Entry& parent_entry)
{
    parent.emplace(parent_entry);
}

template<typename Entry>
Entry V2OpenList<Entry>::remove_min()
{
    return buckets.remove_random_entry(rng);
}

template<typename Entry>
bool V2OpenList<Entry>::empty() const
{
    return buckets.empty();
}

template<typename Entry>
void V2OpenList<Entry>::clear()
{
    buckets.clear();
}

template<typename Entry>
bool V2OpenList<Entry>::is_dead_end(EvaluationContext &eval_context) const
{
    return eval_context.is_evaluator_value_infinite(heuristic.get());
    // FIXME:
}

template<typename Entry>
bool V2OpenList<Entry>::is_reliable_dead_end(EvaluationContext &eval_context) const
{
    return is_dead_end(eval_context) && heuristic->dead_ends_are_reliable();
    // FIXME:
}

template<typename Entry>
void V2OpenList<Entry>::get_path_dependent_evaluators(set<Evaluator *> &evals)
{
    heuristic->get_path_dependent_evaluators(evals);
    // FIXME:
}

V2OpenListFactory::V2OpenListFactory(
    const shared_ptr<Evaluator> &heuristic,
    const vector<shared_ptr<Evaluator>> &evaluators,
    const int random_seed
) : heuristic(heuristic), evaluators(evaluators), random_seed(random_seed)
{
}

unique_ptr<StateOpenList>
V2OpenListFactory::create_state_open_list()
{
    return utils::make_unique_ptr<V2OpenList<StateOpenListEntry>>(heuristic, evaluators, random_seed);
}

unique_ptr<EdgeOpenList>
V2OpenListFactory::create_edge_open_list()
{
    return utils::make_unique_ptr<V2OpenList<EdgeOpenListEntry>>(heuristic, evaluators, random_seed);
}

class V2OpenListFeature : public plugins::TypedFeature<OpenListFactory, V2OpenListFactory>
{

public:

    V2OpenListFeature() : TypedFeature("v2")
    {
        add_option<shared_ptr<Evaluator>>("heuristic", "");
        add_list_option<shared_ptr<Evaluator>>("evaluators","Evaluators used to determine the bucket for each entry.");
        utils::add_rng_options_to_feature(*this);
    }

    [[nodiscard]] shared_ptr<V2OpenListFactory> create_component(const plugins::Options &opts, const utils::Context &context) const override
    {
        plugins::verify_list_non_empty<shared_ptr<Evaluator>>(context, opts, "evaluators");
        return plugins::make_shared_from_arg_tuples<V2OpenListFactory>(
            opts.get<shared_ptr<Evaluator>>("heuristic"),
            opts.get_list<shared_ptr<Evaluator>>("evaluators"),
            utils::get_rng_arguments_from_options(opts)
        );
    }
};

static plugins::FeaturePlugin<V2OpenListFeature> _plugin;

}
