#include "heuristic_improvement_open_list.h"

#include "../evaluator.h"
#include "../open_list.h"

#include "../plugins/plugin.h"
#include "../utils/collections.h"
#include "../utils/hash.h"
#include "../utils/logging.h"
#include "../utils/markup.h"
#include "../utils/memory.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#define out(...) cout << "\x1b[96m[HI] " << __VA_ARGS__ << "\x1b[0m"
#define outl(...) out(__VA_ARGS__) << endl

using namespace std;

namespace heuristic_improvement_open_list {
template <class Entry>
class HeuristicImprovementOpenList : public OpenList<Entry>
{
    shared_ptr<Evaluator> evaluator;
    shared_ptr<utils::RandomNumberGenerator> rng;

    using Bucket = vector<Entry>;
    vector<Bucket> buckets;
    std::optional<size_t> index_of_parent_bucket;
    std::optional<size_t> index_of_new_bucket;
    utils::HashMap<Entry, size_t> entry_to_bucket_index;
    std::optional<size_t> index_of_bucket_of_most_recently_removed_entry;

    void delete_empty_bucket_and_reset_indices(size_t bucket_index)
    {
        assert(buckets[bucket_index].empty());

        utils::swap_and_pop_from_vector(buckets, bucket_index);

        // FIXME: We should probably explicitly reset affected buckets at the call site and only do this in debug builds.
        index_of_parent_bucket.reset();
        index_of_new_bucket.reset();
        index_of_bucket_of_most_recently_removed_entry.reset();
    }

    size_t add_bucket()
    {
        buckets.push_back(Bucket{});
        return buckets.size() - 1;
    }

protected:
    void do_insertion(EvaluationContext& eval_context, const Entry& entry) override;

public:
    explicit HeuristicImprovementOpenList(const shared_ptr<Evaluator>& evaluator,
                                          int random_seed);

    Entry remove_min() override;
    [[nodiscard]] bool empty() const override;
    void clear() override;
    bool is_dead_end(EvaluationContext& eval_context) const override;
    bool is_reliable_dead_end(EvaluationContext& eval_context) const override;
    void get_path_dependent_evaluators(set<Evaluator*>& evals) override;
    void notify_new_expansion(const Entry& parent_entry) override;
};

template <class Entry>
HeuristicImprovementOpenList<Entry>::HeuristicImprovementOpenList(
    const shared_ptr<Evaluator>& evaluator, const int random_seed)
    : evaluator(evaluator),
      rng(utils::get_rng(random_seed)) {}

template <class Entry>
void HeuristicImprovementOpenList<Entry>::notify_new_expansion(const Entry& parent_entry)
{
    outl("@notify_new_expansion");

    auto map_iterator = entry_to_bucket_index.find(parent_entry);

    if (map_iterator == entry_to_bucket_index.end()) {
        /*
        Entries are always inserted into _all_ open lists before they're expanded.
        - If parent_entry is _not_ in our map, it must be because _we_ removed it in remove_min.
        - Since the algorithm stops calling remove_min once it receives an open entry, this entry must
          come from the most recent call to _our_ remove_min.
        - Therefore, the bucket index we last cached in remove_min must be _this_ entry's bucket index.
        */

        outl("parent entry unknown");
        if (!index_of_bucket_of_most_recently_removed_entry.has_value()) {
            /*
            If parent_entry was the last member of its bucket when we selected it, that bucket was
            subsequently deleted. A new bucket will be created in do_insertion once needed.
            */

            outl("no cached bucket index -> bucket was deleted");
            index_of_parent_bucket.reset();
        } else {
            outl("have cached bucket index -> use that");
            index_of_parent_bucket.emplace(index_of_bucket_of_most_recently_removed_entry.value());
        }
    } else {
        outl("parent entry known -> get bucket index from map");
        size_t bucket_index = map_iterator->second;
        assert(utils::in_bounds(bucket_index, buckets));
        bucket_index = map_iterator->second;
    }

    index_of_bucket_of_most_recently_removed_entry.reset(); // FIXME: Might be fine to ignore this.

    /*
    A new "new" bucket (i.e., a new type) will be created in do_insertion once needed.
    */

    outl("resetting new bucket index");
    index_of_new_bucket.reset();
}

template <class Entry>
void HeuristicImprovementOpenList<Entry>::do_insertion(EvaluationContext& eval_context,
                                                       const Entry& entry)
{
    outl("@insert");

    auto progress = eval_context.get_evaluator_value_or_infinity(evaluator.get());

    std::optional<size_t> bucket_index;
    if (progress > 0) {
        // This entry made progress. Add it to the new bucket.
        outl("progress state (" << progress << ")");
        if (!index_of_new_bucket.has_value()) {
            // The new bucket has not been created yet. Create it; we will reuse this bucket for all
            // improving sibling entries.
            outl("have no new bucket -> create it");
            index_of_new_bucket.emplace(add_bucket());
        } else {
            outl("reusing existing new bucket");
        }
        bucket_index.emplace(index_of_new_bucket.value());
    } else {
        // The previously selected entry was _not_ a progress state. Add this entry to the
        // previously selected bucket.
        outl("non-progress state (" << progress << ")");
        if (!index_of_parent_bucket.has_value()) {
            // The previously selected entry was the last in its bucket. Create a new one.
            outl("have no parent bucket -> create it");
            index_of_parent_bucket.emplace(add_bucket());
        } else {
            outl("reusing existing parent bucket");
        }
        bucket_index.emplace(index_of_parent_bucket.value());
    }
    assert(bucket_index.has_value());
    buckets[bucket_index.value()].push_back(entry);
    entry_to_bucket_index[entry] = bucket_index.value();
    outl("inserted");
}

template <class Entry>
Entry HeuristicImprovementOpenList<Entry>::remove_min()
{
    outl("@remove_min");

    /* Select a bucket. */

    /*
    We need access to this information in subsequent insertions to be able to add non-improving
    successors of the selected entry to this same bucket.
    */
    index_of_bucket_of_most_recently_removed_entry.emplace(rng->random(buckets.size()));

    Bucket& bucket = buckets[index_of_bucket_of_most_recently_removed_entry.value()];
    assert(!bucket.empty());

    /* Select an entry. */
    size_t entry_index = rng->random(bucket.size());
    Entry selected_entry = utils::swap_and_pop_from_vector(bucket, entry_index);

    /*
    remove_min may be called multiple times for a single search step, for example if we return
    entries that have been closed already via an alternate open list. Therefore we must immediately
    erase the selected entry from our map here.
    */
    outl("erasing selected entry");
    entry_to_bucket_index.erase(selected_entry);

    /*
    The same applies to empty buckets: if we don't delete these immediately, a subsequent call to
    this function might select an empty bucket.
    */
    if (bucket.empty()) {
        outl("deleting empty bucket");
        delete_empty_bucket_and_reset_indices(
            index_of_bucket_of_most_recently_removed_entry.value());
    }

    return selected_entry;
}

template <class Entry>
bool HeuristicImprovementOpenList<Entry>::empty() const
{
    return buckets.empty();
}

template <class Entry>
void HeuristicImprovementOpenList<Entry>::clear()
{
    index_of_parent_bucket.reset();
    index_of_new_bucket.reset();
    buckets.clear();
}

// TODO: Verify that this is correct behavior. This was taken from best_first_open_list.
template <class Entry>
bool HeuristicImprovementOpenList<Entry>::is_dead_end(
    EvaluationContext& eval_context) const
{
    return eval_context.is_evaluator_value_infinite(evaluator.get());
}

// TODO: Verify that this is correct behavior. This was taken from best_first_open_list.
template <class Entry>
bool HeuristicImprovementOpenList<Entry>::is_reliable_dead_end(
    EvaluationContext& eval_context) const
{
    return is_dead_end(eval_context) && evaluator->dead_ends_are_reliable();
}

template <class Entry>
void HeuristicImprovementOpenList<Entry>::get_path_dependent_evaluators(set<Evaluator*>& evals)
{
    evaluator->get_path_dependent_evaluators(evals);
}

HeuristicImprovementOpenListFactory::HeuristicImprovementOpenListFactory(
    const shared_ptr<Evaluator>& evaluator, const int random_seed)
    : evaluator(evaluator), random_seed(random_seed) {}

unique_ptr<StateOpenList> HeuristicImprovementOpenListFactory::create_state_open_list()
{
    return utils::make_unique_ptr<HeuristicImprovementOpenList<StateOpenListEntry>>(evaluator,
        random_seed);
}

unique_ptr<EdgeOpenList> HeuristicImprovementOpenListFactory::create_edge_open_list()
{
    return utils::make_unique_ptr<HeuristicImprovementOpenList<EdgeOpenListEntry>>(evaluator,
        random_seed);
}

class HeuristicImprovementOpenListFeature
    : public plugins::TypedFeature<OpenListFactory, HeuristicImprovementOpenListFactory>
{
protected:
    [[nodiscard]] shared_ptr<HeuristicImprovementOpenListFactory>
    create_component(const plugins::Options& opts, const utils::Context&) const override
    {
        return plugins::make_shared_from_arg_tuples<HeuristicImprovementOpenListFactory>(
            opts.get<shared_ptr<Evaluator>>("eval"),
            utils::get_rng_arguments_from_options(opts));
        // TODO: best_first_open_list includes this. Is this needed?
        // get_open_list_arguments_from_options(opts));
    }

public:
    HeuristicImprovementOpenListFeature() : TypedFeature("hi")
    {
        document_title("Heuristic improvement open list");
        document_synopsis("");

        add_option<shared_ptr<Evaluator>>(
            "eval", "Evaluator used to determine the bucket for each entry.");
        utils::add_rng_options_to_feature(*this);
        // TODO: See todo above.
        // add_open_list_options_to_feature(*this);
    }
};

static plugins::FeaturePlugin<HeuristicImprovementOpenListFeature> _plugin;
}
