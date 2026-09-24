#include "one_open_list.h"

#include "../evaluator.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

#include <ranges>
#include <optional>

#define OUT_STYLE "\x1b[92m"
#define OUT_PREFIX "one "
#include "../../out.h"

namespace one_open_list {

using RNG = std::shared_ptr<utils::RandomNumberGenerator>;

/*
Type Systems.
*/

template<typename Entry> class HGTS
{
    std::shared_ptr<Evaluator> h;
    std::vector<std::shared_ptr<Evaluator>> evaluators;

public:
    using TypeKey = std::vector<int>;

    struct TypeInfo
    {
        TypeKey key;
        int depth{};
        int h{};
    };

    explicit HGTS(
        const std::shared_ptr<Evaluator>& h,
        std::vector<std::shared_ptr<Evaluator>> evaluators
    ) : h(h),
        evaluators(std::move(evaluators))
    {
    }

    static void notify_new_expansion(const Entry& /*parent_entry*/)
    {
    }

    TypeInfo assign_type(EvaluationContext& eval_context, const Entry&)
    {
        TypeKey type_key;
        type_key.reserve(evaluators.size());
        for (const auto& evaluator : evaluators) {
            type_key.push_back(eval_context.get_evaluator_value_or_infinity(evaluator.get()));
        }
        return {
            .key = std::move(type_key),
            .depth = 0,
            .h = eval_context.get_evaluator_value_or_infinity(h.get())
        };
    }

    bool is_dead_end(EvaluationContext& eval_context) const
    {
        if (is_reliable_dead_end(eval_context)) {
            return true;
        }
        for (const std::shared_ptr<Evaluator> &evaluator : evaluators) {
            if (!eval_context.is_evaluator_value_infinite(evaluator.get()))
                return false;
        }
        return true;
    }

    bool is_reliable_dead_end(EvaluationContext& eval_context) const
    {
        for (const auto& evaluator : evaluators) {
            if (evaluator->dead_ends_are_reliable() && eval_context.is_evaluator_value_infinite(evaluator.get())) {
                return true;
            }
        }
        return false;
    }

    void get_path_dependent_evaluators(std::set<Evaluator*>& evals) const
    {
        for (const auto& evaluator : evaluators) {
            evaluator->get_path_dependent_evaluators(evals);
        }
    }

    static void clear()
    {
    }
};

template<typename Entry> class HITS
{
public:
    using TypeKey = Entry;

    struct TypeInfo
    {
        TypeKey key;
        int depth{};
        int h{};
    };

private:
    std::shared_ptr<Evaluator> h;
    utils::HashMap<Entry, TypeInfo> info_by_entry;
    std::optional<TypeInfo> parent;
    std::optional<Entry> new_type_key;

public:
    explicit HITS(const std::shared_ptr<Evaluator>& h) : h(h)
    {
    }

    void notify_new_expansion(const Entry& parent_entry)
    {
        parent = info_by_entry.at(parent_entry);
        info_by_entry.erase(parent_entry);
        new_type_key.reset();
    }

    TypeInfo assign_type(EvaluationContext& eval_context, const Entry& child)
    {
        assert(!info_by_entry.contains(child));
        const int child_h = eval_context.get_evaluator_value_or_infinity(h.get());

        std::optional<TypeKey> key;
        int depth;
        if (!parent.has_value()) {
            depth = 0;
            key.emplace(child);
        } else if (child_h < parent.value().h) {
            depth = parent.value().depth + 1;
            if (!new_type_key) {
                new_type_key.emplace(child);
            }
            key.emplace(new_type_key.value());
        } else {
            depth = parent.value().depth;
            key.emplace(parent.value().key);
        }

        TypeInfo info = {
            .key = key.value(),
            .depth = depth,
            .h = child_h,
        };
        info_by_entry.emplace(child, info);
        return info;
    }

    bool is_dead_end(EvaluationContext& eval_context) const
    {
        return eval_context.is_evaluator_value_infinite(h.get());
    }

    bool is_reliable_dead_end(EvaluationContext& eval_context) const
    {
        return is_dead_end(eval_context) && h->dead_ends_are_reliable();
    }

    void get_path_dependent_evaluators(std::set<Evaluator*>& evals) const
    {
        h->get_path_dependent_evaluators(evals);
    }

    void clear()
    {
        info_by_entry.clear();
        parent.reset();
        new_type_key.reset();
    }
};

template<typename Entry> class LOTS
{
public:
    using TypeKey = Entry;

    struct TypeInfo
    {
        TypeKey key;
        int depth{};
        int lw{};
        int h{};
    };

private:
    std::shared_ptr<Evaluator> h;
    utils::HashMap<Entry, TypeInfo> info_by_entry;
    std::optional<TypeInfo> parent;
    utils::HashMap<int, Entry> new_type_key_by_lw;

public:
    explicit LOTS(const std::shared_ptr<Evaluator>& h) : h(h)
    {
    }

    void notify_new_expansion(const Entry& parent_entry)
    {
        parent = info_by_entry.at(parent_entry);
        info_by_entry.erase(parent_entry);
        new_type_key_by_lw.clear();
    }

    TypeInfo assign_type(EvaluationContext& eval_context, const Entry& child)
    {
        assert(!info_by_entry.contains(child));
        // TODO: can this happen?
        if (const auto it = info_by_entry.find(child); it != info_by_entry.end()) {
            return it->second;
        }

        const int child_h = eval_context.get_evaluator_value_or_infinity(h.get());

        std::optional<TypeKey> key;
        int depth;
        int lw;
        if (!parent.has_value()) {
            depth = 0;
            lw = child_h;
            key.emplace(child);
        } else if (child_h < parent.value().lw) {
            depth = parent.value().depth + 1;
            lw = child_h;
            if (!new_type_key_by_lw.contains(child_h)) {
                new_type_key_by_lw.emplace(child_h, child);
            }
            key.emplace(new_type_key_by_lw.at(child_h));
        } else {
            depth = parent.value().depth;
            lw = parent.value().lw;
            key.emplace(parent.value().key);
        }

        TypeInfo info = {
            .key = key.value(),
            .depth = depth,
            .lw = lw,
            .h = child_h,
        };
        info_by_entry.emplace(child, info);
        return info;
    }

    bool is_dead_end(EvaluationContext& eval_context) const
    {
        return eval_context.is_evaluator_value_infinite(h.get());
    }

    bool is_reliable_dead_end(EvaluationContext& eval_context) const
    {
        return is_dead_end(eval_context) && h->dead_ends_are_reliable();
    }

    void get_path_dependent_evaluators(std::set<Evaluator*>& evals) const
    {
        h->get_path_dependent_evaluators(evals);
    }

    void clear()
    {
        info_by_entry.clear();
        parent.reset();
        new_type_key_by_lw.clear();
    }
};

/*
Storage.
*/

template<typename Entry> class UBucket
{
    struct Item
    {
        Entry entry;
        int h;
    };

    std::vector<Item> items;
    utils::HashMap<int, std::size_t> h_counts;
    int min_h = EvaluationResult::INFTY;

    void recompute_min_h()
    {
        min_h = EvaluationResult::INFTY;
        for (auto h : h_counts | std::views::keys) {
            min_h = std::min(min_h, h);
        }
    }

public:
    void add(Entry entry, int h)
    {
        items.push_back(Item{.entry = entry, .h = h});
        ++h_counts[h];
        min_h = std::min(min_h, h);
    }

    Entry remove_random(const RNG& rng)
    {
        assert(!empty());

        std::size_t index = rng->random(items.size());
        auto [entry, h] = items[index];

        items[index] = std::move(items.back());
        items.pop_back();

        auto it = h_counts.find(h);
        --it->second;
        if (it->second == 0) {
            h_counts.erase(it);
            if (min_h == h) {
                recompute_min_h();
            }
        }
        return entry;
    }

    [[nodiscard]] int get_min_h() const
    {
        return min_h;
    }

    bool empty()
    {
        return items.empty();
    }
};

template<typename Entry> class HBucket
{
    struct Group
    {
        std::vector<Entry> entries;
        double weight{};
    };

    utils::HashMap<int, Group> group_by_h;
    double total_weight = 0;
    int min_h = EvaluationResult::INFTY;
    double state_temperature;

    void recompute_min_h()
    {
        min_h = EvaluationResult::INFTY;
        for (const auto& h : group_by_h | std::views::keys) {
            min_h = std::min(min_h, h);
        }
    }

public:
    explicit HBucket(const double state_temperature) : state_temperature(state_temperature) {}

    void add(const Entry& entry, int h)
    {
        const bool is_new_group = !group_by_h.contains(h);
        auto& group = group_by_h[h];
        if (is_new_group) {
            group.weight = std::exp(-h / state_temperature);
            total_weight += group.weight;
        }
        group.entries.push_back(entry);
        min_h = std::min(min_h, h);
    }

    Entry remove_random(const RNG& rng)
    {
        assert(!empty());

        /* Select item. */
        double x = rng->random() * total_weight;
        Group* group = nullptr;
        int h = EvaluationResult::INFTY;
        for (auto& [candidate_h, candidate_group] : group_by_h) {
            group = &candidate_group;
            h = candidate_h;
            if (x < candidate_group.weight) {
                break;
            }
            x -= candidate_group.weight;
        }
        assert(!group->entries.empty());
        std::size_t position_in_group = rng->random(group->entries.size());

        /* Delete item. */
        Entry entry = group->entries[position_in_group];
        if (position_in_group != group->entries.size() - 1) {
            group->entries[position_in_group] = group->entries.back();
        }
        group->entries.pop_back();
        if (group->entries.empty()) {
            total_weight -= group->weight;
            group_by_h.erase(h);
        }
        if (h == min_h && !group_by_h.contains(h)) {
            recompute_min_h();
        }

        return entry;
    }

    [[nodiscard]] int get_min_h() const
    {
        return min_h;
    }

    bool empty()
    {
        return group_by_h.empty();
    }
};

#include "one_open_list/uu.h"
#include "one_open_list/uh.h"
#include "one_open_list/hu.h"
#include "one_open_list/hh.h"
#include "one_open_list/dh.h"
#include "one_open_list/du.h"

/*
Open List.
*/

template<typename Entry, typename TypeSystem, typename Buckets> class OneOpenList : public OpenList<Entry>
{
    using TypeInfo = TypeSystem::TypeInfo;
    using BucketKey = TypeSystem::TypeKey;

    TypeSystem type_system;
    Buckets buckets;
    RNG rng;

protected:
    void do_insertion(EvaluationContext& eval_context, const Entry& entry) override
    {
        auto info = type_system.assign_type(eval_context, entry);
        buckets.bucket_add(info, entry);
    }

public:
    OneOpenList(
        TypeSystem type_system,
        Buckets buckets,
        const int random_seed
    ) : type_system(std::move(type_system)),
        buckets(std::move(buckets))
    {
        rng = utils::get_rng(random_seed);
    }

    void notify_new_expansion(const Entry& parent_entry) override
    {
        type_system.notify_new_expansion(parent_entry);
    }

    Entry remove_min() override
    {
        return buckets.remove_random(rng);
    }

    [[nodiscard]] bool empty() const override
    {
        return buckets.empty();
    }

    void clear() override
    {
        buckets.clear();
        type_system.clear();
    }

    bool is_dead_end(EvaluationContext& eval_context) const override
    {
        return type_system.is_dead_end(eval_context);
    }

    bool is_reliable_dead_end(EvaluationContext& eval_context) const override
    {
        return type_system.is_reliable_dead_end(eval_context);
    }

    void get_path_dependent_evaluators(std::set<Evaluator*>& evals) override
    {
        type_system.get_path_dependent_evaluators(evals);
    }
};

class OneOpenListFactory : public OpenListFactory
{
    const std::shared_ptr<Evaluator> heuristic;
    std::string type_system;
    const std::vector<std::shared_ptr<Evaluator>> evaluators;
    std::string bucket_selection;
    double bucket_temperature;
    std::string state_selection;
    double state_temperature;
    int random_seed;

    template<typename Entry> [[nodiscard]] std::unique_ptr<OpenList<Entry>> create() const
    {
        if (type_system == "hg") {
            using TypeSystem = HGTS<Entry>;
            using TypeKey = std::vector<int>;
            if (bucket_selection == "U") {
                if (state_selection == "U") {
                    using Storage = UUBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic, evaluators),
                        Storage(),
                        random_seed
                    );
                }
            }
            if (bucket_selection == "H") {
                if (state_selection == "U") {
                    using Storage = HUBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic, evaluators),
                        Storage(bucket_temperature),
                        random_seed
                    );
                }
            }
        }
        if (type_system == "hi") {
            using TypeSystem = HITS<Entry>;
            using TypeKey = Entry;
            if (bucket_selection == "U") {
                if (state_selection == "U") {
                    using Storage = UUBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(),
                        random_seed
                    );
                }
                if (state_selection == "H") {
                    using Storage = UHBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(state_temperature),
                        random_seed
                    );
                }
            }
            if (bucket_selection == "H") {
                if (state_selection == "U") {
                    using Storage = HUBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(bucket_temperature),
                        random_seed
                    );
                }
                if (state_selection == "H") {
                    using Storage = HHBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(bucket_temperature, state_temperature),
                        random_seed
                    );
                }
            }
            if (bucket_selection == "D") {
                if (state_selection == "U") {
                    using Storage = DUBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(bucket_temperature),
                        random_seed
                    );
                }
                if (state_selection == "H") {
                    using Storage = DHBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(bucket_temperature, state_temperature),
                        random_seed
                    );
                }
            }
        }
        if (type_system == "lw") {
            using TypeSystem = LOTS<Entry>;
            using TypeKey = Entry;
            if (bucket_selection == "U") {
                if (state_selection == "U") {
                    using Storage = UUBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(),
                        random_seed
                    );
                }
                if (state_selection == "H") {
                    using Storage = UHBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(state_temperature),
                        random_seed
                    );
                }
            }
            if (bucket_selection == "H") {
                if (state_selection == "U") {
                    using Storage = HUBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(bucket_temperature),
                        random_seed
                    );
                }
                if (state_selection == "H") {
                    using Storage = HHBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(bucket_temperature, state_temperature),
                        random_seed
                    );
                }
            }
            if (bucket_selection == "D") {
                if (state_selection == "U") {
                    using Storage = DUBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(bucket_temperature),
                        random_seed
                    );
                }
                if (state_selection == "H") {
                    using Storage = DHBuckets<TypeKey, Entry>;
                    return std::make_unique<OneOpenList<Entry, TypeSystem, Storage>>(
                        TypeSystem(heuristic),
                        Storage(bucket_temperature, state_temperature),
                        random_seed
                    );
                }
            }
        }
        assert(false);
    }

public:
    OneOpenListFactory(
        const std::shared_ptr<Evaluator>& heuristic,
        std::string type_system,
        const std::vector<std::shared_ptr<Evaluator>>& evaluators,
        std::string bucket_selection,
        const double bucket_temperature,
        std::string state_selection,
        const double state_temperature,
        const int random_seed
    ) : heuristic(heuristic),
        type_system(std::move(type_system)),
        evaluators(evaluators),
        bucket_selection(std::move(bucket_selection)),
        bucket_temperature(bucket_temperature),
        state_selection(std::move(state_selection)),
        state_temperature(state_temperature),
        random_seed(random_seed)
    {}

    std::unique_ptr<StateOpenList> create_state_open_list() override
    {
        return create<StateOpenListEntry>();
    }

    std::unique_ptr<EdgeOpenList> create_edge_open_list() override
    {
        return create<EdgeOpenListEntry>();
    }
};

class OneOpenListFeature : public plugins::TypedFeature<OpenListFactory, OneOpenListFactory>
{
protected:
    [[nodiscard]] std::shared_ptr<OneOpenListFactory> create_component(const plugins::Options& opts, const utils::Context&) const override
    {
        return plugins::make_shared_from_arg_tuples<OneOpenListFactory>(
            opts.get<std::shared_ptr<Evaluator>>("heuristic"),
            opts.get<std::string>("type_system"),
            opts.get_list<std::shared_ptr<Evaluator>>("evaluators"),
            opts.get<std::string>("bucket_selection"),
            opts.get<double>("bucket_temperature"),
            opts.get<std::string>("state_selection"),
            opts.get<double>("state_temperature"),
            utils::get_rng_arguments_from_options(opts)
        );
    }

public:
    OneOpenListFeature() : TypedFeature("one")
    {
        add_option<std::shared_ptr<Evaluator>>("heuristic");
        add_option<std::string>("type_system");
        add_list_option<std::shared_ptr<Evaluator>>("evaluators");
        add_option<std::string>("bucket_selection");
        add_option<double>("bucket_temperature");
        add_option<std::string>("state_selection");
        add_option<double>("state_temperature");
        utils::add_rng_options_to_feature(*this);
    }
};

[[maybe_unused]] static plugins::FeaturePlugin<OneOpenListFeature> _plugin;

}
