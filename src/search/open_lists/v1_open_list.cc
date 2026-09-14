#include "v1_open_list.h"

#include "../plugins/plugin.h"
#include "../open_list.h"
#include "../evaluator.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

#define OUT_STYLE "\x1b[91m"
#define OUT_PREFIX "v1 "
#include <ranges>

#include "../../out.h"

using namespace std;
#include <optional>

namespace v1_open_list {

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

template<typename Entry>
class Group
{
    std::vector<Entry> entries;

public:
    void add_entry(Entry entry)
    {
        entries.push_back(entry);
    }

    Entry remove_random_entry(const shared_ptr<utils::RandomNumberGenerator>& rng)
    {
        /* Select entry. */
        const size_t entries_index = rng->random(entries.size());
        const auto entry = std::move(entries[entries_index]);

        /* Remove entry from group. */
        if (entries_index != entries.size() - 1) {
            entries[entries_index] = std::move(entries.back());
        }
        entries.pop_back();

        return entry;
    }

    bool empty()
    {
        return entries.empty();
    }
};

template<typename Entry> class Bucket;
template<typename Entry> class Buckets;

template<typename Entry>
class LowHBias
{
public:
    void on_create_group(Bucket<Entry>& bucket, int h) const
    {
        bucket.set_bias(std::min(bucket.get_bias_or_infty_if_empty(), h));
    }
};

template<typename Entry>
class Bucket
{
    struct GroupInfo
    {
        int h;
    };

    BucketID id;
    utils::HashMap<int, Group<Entry>> group_by_h;
    std::vector<GroupInfo> group_infos;
    utils::HashMap<int, size_t> group_infos_index_by_h;
    double total_weight = 0;
    int bias = EvaluationResult::INFTY;

public:
    explicit Bucket(const BucketID id) : id(id) {}

    BucketID get_id()
    {
        return id;
    }

    void add_entry(int h, Entry entry)
    {
        /* Find or create group and group info. */
        auto [i, is_new] = group_by_h.try_emplace(h);
        Group<Entry>& group = i->second;
        size_t group_info_index;
        if (is_new) {
            group_info_index = group_infos.size();
            group_infos_index_by_h[h] = group_info_index;
            group_infos.push_back(GroupInfo{.h = h});
            Buckets<Entry>::bias_strategy.on_create_group(*this, h);
        } else {
            group_info_index = group_infos_index_by_h[h];
        }
        GroupInfo &group_info = group_infos[group_info_index];

        /* Add entry to group. */
        group.add_entry(entry);
    }

    Entry remove_random_entry(const shared_ptr<utils::RandomNumberGenerator>& rng)
    {
        /* Select group. */
        std::vector<double> weights;
        for (auto &group_info : group_infos) {
            weights.push_back(group_info.weight);
        }
        size_t group_info_index = softmin_sample(rng, weights);

        auto& group_info = group_infos[group_info_index];
        auto group_h = group_info.h;
        auto& group = group_by_h.at(group_h);

        /* Select entry. */
        auto entry = group.remove_random_entry(rng);
        if constexpr (!USE_SOFTMIN) {
            group_info.weight -= 1;
            total_weight -= 1;
        }

        /* If the group is now empty, delete it. */
        if (group.empty()) {
            if constexpr (USE_SOFTMIN) {
                total_weight -= group_info.weight;
            }
            if (group_info_index != group_infos.size() - 1) {
                group_infos_index_by_h[group_infos.back().h] = group_info_index;
                group_infos[group_info_index] = std::move(group_infos.back());
            }
            group_infos.pop_back();
            group_by_h.erase(group_h);
            group_infos_index_by_h.erase(group_h);

            /* Recompute min h if necessary. */
            if (group_h == bias) {
                bias = EvaluationResult::INFTY;
                for (const auto& group_info_ : group_infos) {
                    bias = std::min(bias, group_info_.h);
                }
            }
        }

        return entry;
    }

    int get_bias_or_infty_if_empty()
    {
        assert(bias != EvaluationResult::INFTY || (group_by_h.empty() && group_infos.empty()));
        return bias;
    }

    void set_bias(const int new_bias)
    {
        bias = new_bias;
    }
};

template<typename Entry>
class Buckets
{
    class Family
    {
        std::vector<BucketID> members;
        utils::HashMap<BucketID, size_t> members_index_by_bucket_id;

    public:
        void add_bucket(BucketID bucket_id)
        {
            members_index_by_bucket_id[bucket_id] = members.size();
            members.push_back(bucket_id);
        }

        void remove_bucket(BucketID bucket_id)
        {
            if (const size_t current_members_index = members_index_by_bucket_id.at(bucket_id);
                current_members_index != members.size() - 1) {
                members.at(current_members_index) = std::move(members.back());
                members_index_by_bucket_id.at(members.back()) = current_members_index;
            }
            members.pop_back();
            members_index_by_bucket_id.erase(bucket_id);
        }

        BucketID select_random_bucket(const shared_ptr<utils::RandomNumberGenerator>& rng)
        {
            const size_t members_index = rng->random(members.size());
            return members[members_index];
        }

        [[nodiscard]] bool empty() const
        {
            assert(!members.empty() || members_index_by_bucket_id.empty());
            return members.empty();
        }
    };

    std::vector<Bucket<Entry>> buckets;
    utils::HashMap<BucketID, size_t> buckets_index_by_bucket_id;
    utils::HashMap<int, Family> family_by_bias;

    void remove_family(const int family_h)
    {
        assert(family_by_bias.at(family_h).empty());
        family_by_bias.erase(family_h);
    }

public:
    static constexpr LowHBias<Entry> bias_strategy = LowHBias<Entry>();

    void add_entry(BucketID bucket_id, int h, Entry entry)
    {
        /* Find or create requested bucket. */
        size_t bucket_index;
        if (const auto i = buckets_index_by_bucket_id.find(bucket_id); i != buckets_index_by_bucket_id.end()) {
            bucket_index = i->second;
        } else {
            bucket_index = buckets.size();
            buckets.push_back(Bucket<Entry>(bucket_id));
            buckets_index_by_bucket_id[bucket_id] = bucket_index;
        }
        auto &bucket = buckets[bucket_index];

        /* Add entry to bucket. */
        const int bias_before = bucket.get_bias_or_infty_if_empty();
        bucket.add_entry(h, entry);
        const int bias_after = bucket.get_bias_or_infty_if_empty();

        /* Change family if necessary. */
        if (bias_after != bias_before) {
            if (bias_before != EvaluationResult::INFTY) {
                family_by_bias.at(bias_before).remove_bucket(bucket_id);
                if (family_by_bias.at(bias_before).empty()) {
                    remove_family(bias_before);
                }
            }
            family_by_bias[bias_after].add_bucket(bucket_id);
        }
    }

    Entry remove_random_entry(const shared_ptr<utils::RandomNumberGenerator>& rng)
    {
        /* Select family. */
        std::vector<int> biases;
        for (const auto& bias : family_by_bias | views::keys) {
            biases.push_back(bias);
        }
        const int family_bias = biases[rng->random(biases.size())];
        auto &family = family_by_bias.at(family_bias);

        /* Select bucket. */
        const BucketID bucket_id = family.select_random_bucket(rng);
        const size_t bucket_index = buckets_index_by_bucket_id.at(bucket_id);
        auto& bucket = buckets[bucket_index];
        assert(bucket.get_bias_or_infty_if_empty() == family_bias);

        /* Remove entry. */
        auto entry = bucket.remove_random_entry(rng);
        const int bias_after = bucket.get_bias_or_infty_if_empty();

        /* If min h changed: */
        if (bias_after != family_bias) {

            /* 1) Remove the bucket from its family. */
            family.remove_bucket(bucket_id);
            if (family.empty()) {
                remove_family(family_bias);
            }

            if (bias_after != EvaluationResult::INFTY) {

                /* 2 a) If the bucket is not empty, add it to its new family. */
                family_by_bias[bias_after].add_bucket(bucket_id);
            } else {

                /* 2 b) If the bucket is empty, delete it. */
                if (bucket_index != buckets.size() - 1) {
                    buckets_index_by_bucket_id.at(buckets.back().get_id()) = bucket_index;
                    buckets[bucket_index] = std::move(buckets.back());
                }
                buckets.pop_back();
                buckets_index_by_bucket_id.erase(bucket_id);
            }
        }

        return entry;
    }

    [[nodiscard]] bool empty() const
    {
        assert(!buckets.empty() || (buckets_index_by_bucket_id.empty() && family_by_bias.empty()));
        return buckets.empty();
    }

    void clear()
    {
        buckets.clear();
        buckets_index_by_bucket_id.clear();
        family_by_bias.clear();
    }
};

template<typename Entry>
class V1OpenList : public OpenList<Entry>
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

    explicit V1OpenList(const shared_ptr<Evaluator> &heuristic, const vector<shared_ptr<Evaluator>> &evaluators, int random_seed);

    void notify_new_expansion(const Entry& parent_entry) override;
    Entry remove_min() override;
    [[nodiscard]] bool empty() const override;
    void clear() override;
    bool is_dead_end(EvaluationContext &eval_context) const override;
    bool is_reliable_dead_end(EvaluationContext &eval_context) const override;
    void get_path_dependent_evaluators(set<Evaluator *> &evals) override;
};

template<typename Entry>
V1OpenList<Entry>::V1OpenList(
    const shared_ptr<Evaluator> &heuristic,
    const vector<shared_ptr<Evaluator>> &evaluators,
    const int random_seed
) : heuristic(heuristic), evaluators(evaluators)
{
    rng = utils::get_rng(random_seed);
    outl("seed " << random_seed);
}

template<typename Entry>
void V1OpenList<Entry>::do_insertion(EvaluationContext &eval_context, const Entry &entry)
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

    
    const int group_id = h;
    buckets.add_entry(family_id, bucket_id, group_id, entry);
}

template <typename Entry>
void V1OpenList<Entry>::notify_new_expansion(const Entry& parent_entry)
{
    parent.emplace(parent_entry);
}

template<typename Entry>
Entry V1OpenList<Entry>::remove_min()
{
    return buckets.remove_random_entry(rng);
}

template<typename Entry>
bool V1OpenList<Entry>::empty() const
{
    return buckets.empty();
}

template<typename Entry>
void V1OpenList<Entry>::clear()
{
    buckets.clear();
}

template<typename Entry>
bool V1OpenList<Entry>::is_dead_end(EvaluationContext &eval_context) const
{
    return eval_context.is_evaluator_value_infinite(heuristic.get());
    // FIXME:
}

template<typename Entry>
bool V1OpenList<Entry>::is_reliable_dead_end(EvaluationContext &eval_context) const
{
    return is_dead_end(eval_context) && heuristic->dead_ends_are_reliable();
    // FIXME:
}

template<typename Entry>
void V1OpenList<Entry>::get_path_dependent_evaluators(set<Evaluator *> &evals)
{
    heuristic->get_path_dependent_evaluators(evals);
    // FIXME:
}

V1OpenListFactory::V1OpenListFactory(
    const shared_ptr<Evaluator> &heuristic,
    const vector<shared_ptr<Evaluator>> &evaluators,
    const int random_seed
) : heuristic(heuristic), evaluators(evaluators), random_seed(random_seed)
{
}

unique_ptr<StateOpenList>
V1OpenListFactory::create_state_open_list()
{
    return utils::make_unique_ptr<V1OpenList<StateOpenListEntry>>(heuristic, evaluators, random_seed);
}

unique_ptr<EdgeOpenList>
V1OpenListFactory::create_edge_open_list()
{
    return utils::make_unique_ptr<V1OpenList<EdgeOpenListEntry>>(heuristic, evaluators, random_seed);
}

class V1OpenListFeature : public plugins::TypedFeature<OpenListFactory, V1OpenListFactory>
{

public:

    V1OpenListFeature() : TypedFeature("v1")
    {
        add_option<shared_ptr<Evaluator>>("heuristic", "");
        add_list_option<shared_ptr<Evaluator>>("evaluators","Evaluators used to determine the bucket for each entry.");
        utils::add_rng_options_to_feature(*this);
    }

    [[nodiscard]] shared_ptr<V1OpenListFactory> create_component(const plugins::Options &opts, const utils::Context &context) const override
    {
        plugins::verify_list_non_empty<shared_ptr<Evaluator>>(context, opts, "evaluators");
        return plugins::make_shared_from_arg_tuples<V1OpenListFactory>(
            opts.get<shared_ptr<Evaluator>>("heuristic"),
            opts.get_list<shared_ptr<Evaluator>>("evaluators"),
            utils::get_rng_arguments_from_options(opts)
        );
    }
};

static plugins::FeaturePlugin<V1OpenListFeature> _plugin;

}
