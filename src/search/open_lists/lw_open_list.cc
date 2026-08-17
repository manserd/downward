#include "lw_open_list.h"

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

#define out(...) std::cout << "\x1b[94m[LW] " << __VA_ARGS__ << "\x1b[0m"
#define outl(...) out(__VA_ARGS__) << std::endl

namespace lw_open_list {
template <class Entry>
void LWOpenList<Entry>::notify_new_expansion(const Entry& parent_entry)
{
    auto map_iterator = entry_to_bucket_index.find(parent_entry);
    assert(map_iterator != entry_to_bucket_index.end());
    index_of_parent_bucket.emplace(map_iterator->second);
    h_to_bucket_index.clear();
    outl("recognized bucket #" << index_of_parent_bucket.value());
}

template <class Entry>
void LWOpenList<Entry>::do_insertion(EvaluationContext& eval_context, const Entry& entry)
{
    auto h = eval_context.get_evaluator_value_or_infinity(evaluator.get());

    assert(!index_of_parent_bucket.has_value() || utils::in_bounds(index_of_parent_bucket.value(), buckets));
    const int parent_bucket_lw = index_of_parent_bucket.has_value()
                                     ? buckets[index_of_parent_bucket.value()].first
                                     : EvaluationResult::INFTY; // entry must be the initial entry

    size_t bucket_index;
    if (h < parent_bucket_lw) {
        // entry has made LW progress
        auto map_iterator = h_to_bucket_index.find(h);
        if (map_iterator == h_to_bucket_index.end()) {
            // this is the first successor with this h value. create a new bucket
            bucket_index = buckets.size();
            buckets.push_back(Bucket(h, {}));
            outl("creating and adding to new bucket #" << bucket_index << " (h=" << h << ")");
        } else {
            // we previously saw a sibling with the same h value. add this entry to that bucket
            bucket_index = map_iterator->second;
            outl("adding to new bucket #" << bucket_index << " (h=" << h << ")");
        }
    } else {
        // entry has not made LW progress: add it to the parent's bucket
        assert(index_of_parent_bucket.has_value());
        bucket_index = index_of_parent_bucket.value();
        outl("adding to existing bucket #" << index_of_parent_bucket.value() << ", position #" << buckets[index_of_parent_bucket.value()].second.size());
    }
    buckets[bucket_index].second.push_back(entry);

    entry_to_bucket_index[entry] = bucket_index;
}

template <class Entry>
Entry LWOpenList<Entry>::remove_min()
{
    if (index_of_most_recently_selected_bucket.has_value()) {
        assert(index_of_most_recently_selected_entry.has_value());

        Bucket& bucket = buckets[index_of_most_recently_selected_bucket.value()];

        entry_to_bucket_index.erase(bucket.second[index_of_most_recently_selected_entry.value()]);
        utils::swap_and_pop_from_vector(bucket.second,
                                        index_of_most_recently_selected_entry.value());
        outl("removed from bucket #" << index_of_most_recently_selected_bucket.value() << " position #" << index_of_most_recently_selected_entry.value());

        if (bucket.second.empty()) {

            // Deleting a bucket invalidates the entry_to_bucket_index for entries of the last bucket.
            // We manually address this here, but this is really an architectural problem and
            // (TODO:) should be addressed with better data structures.
            const size_t index_of_last_bucket = buckets.size() - 1;
            if (index_of_last_bucket != index_of_most_recently_selected_bucket.value()) {
                for (auto &entry : buckets[index_of_last_bucket].second) {
                    entry_to_bucket_index[entry] = index_of_most_recently_selected_bucket.value();
                }
            }

            utils::swap_and_pop_from_vector(
                buckets, index_of_most_recently_selected_bucket.value());
            outl("deleted bucket #" << index_of_most_recently_selected_bucket.value());
        }
    }

    index_of_most_recently_selected_bucket.emplace(rng->random(buckets.size()));
    Bucket& bucket = buckets[index_of_most_recently_selected_bucket.value()];
    index_of_most_recently_selected_entry.emplace(rng->random(bucket.second.size()));
    outl("selected in bucket #" << index_of_most_recently_selected_bucket.value() << " position #" << index_of_most_recently_selected_entry.value());
    return bucket.second[index_of_most_recently_selected_entry.value()];
}

template <class Entry>
bool LWOpenList<Entry>::empty() const
{
    return buckets.empty();
}

template <class Entry>
void LWOpenList<Entry>::clear()
{
    buckets.clear();
}

// TODO: Verify that this is correct behavior. This was taken from best_first_open_list.
template <class Entry>
bool LWOpenList<Entry>::is_dead_end(
    EvaluationContext& eval_context) const
{
    return eval_context.is_evaluator_value_infinite(evaluator.get());
}

// TODO: Verify that this is correct behavior. This was taken from best_first_open_list.
template <class Entry>
bool LWOpenList<Entry>::is_reliable_dead_end(
    EvaluationContext& eval_context) const
{
    return is_dead_end(eval_context) && evaluator->dead_ends_are_reliable();
}

template <class Entry>
void LWOpenList<Entry>::get_path_dependent_evaluators(std::set<Evaluator*>& evals)
{
    evaluator->get_path_dependent_evaluators(evals);
}

template <class Entry>
LWOpenList<Entry>::LWOpenList(
    const std::shared_ptr<Evaluator>& evaluator, const int random_seed)
    : evaluator(evaluator),
      rng(utils::get_rng(random_seed)) {}

LWOpenListFactory::LWOpenListFactory(
    const std::shared_ptr<Evaluator>& evaluator, const int random_seed)
    : evaluator(evaluator), random_seed(random_seed) {}

std::unique_ptr<StateOpenList> LWOpenListFactory::create_state_open_list()
{
    return utils::make_unique_ptr<LWOpenList<StateOpenListEntry>>(evaluator,
        random_seed);
}

std::unique_ptr<EdgeOpenList> LWOpenListFactory::create_edge_open_list()
{
    return utils::make_unique_ptr<LWOpenList<EdgeOpenListEntry>>(evaluator,
                                                                 random_seed);
}

class LWOpenListFeature
    : public plugins::TypedFeature<OpenListFactory, LWOpenListFactory>
{
protected:
    [[nodiscard]] std::shared_ptr<LWOpenListFactory>
    create_component(const plugins::Options& opts, const utils::Context&) const override
    {
        return plugins::make_shared_from_arg_tuples<LWOpenListFactory>(
            opts.get<std::shared_ptr<Evaluator>>("eval"),
            utils::get_rng_arguments_from_options(opts));
        // TODO: best_first_open_list includes this. Is this needed?
        // get_open_list_arguments_from_options(opts));
    }

public:
    LWOpenListFeature() : TypedFeature("lw")
    {
        document_title("Low-water mark open list");
        document_synopsis("");

        add_option<std::shared_ptr<Evaluator>>(
            "eval", "Evaluator used to determine the bucket for each entry.");
        utils::add_rng_options_to_feature(*this);
        // TODO: See todo above.
        // add_open_list_options_to_feature(*this);
    }
};

static plugins::FeaturePlugin<LWOpenListFeature> _plugin;
}
