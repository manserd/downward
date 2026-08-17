#ifndef OPEN_LISTS_LW_OPEN_LIST_H
#define OPEN_LISTS_LW_OPEN_LIST_H

#include "../open_list_factory.h"
#include "../utils/rng.h"

#include <optional>

namespace lw_open_list {

template <class Entry>
class LWOpenList : public OpenList<Entry>
{
    std::shared_ptr<Evaluator> evaluator;
    std::shared_ptr<utils::RandomNumberGenerator> rng;

    using Bucket = std::pair<int, std::vector<Entry>>;
    std::vector<Bucket> buckets;
    std::optional<size_t> index_of_most_recently_selected_bucket;
    std::optional<size_t> index_of_most_recently_selected_entry;
    utils::HashMap<Entry, size_t> entry_to_bucket_index;
    std::optional<size_t> index_of_parent_bucket;
    utils::HashMap<int, int> h_to_bucket_index;

protected:
    void do_insertion(EvaluationContext& eval_context, const Entry& entry) override;

public:
    explicit LWOpenList(const std::shared_ptr<Evaluator>& evaluator, int random_seed);
    Entry remove_min() override;
    [[nodiscard]] bool empty() const override;
    void clear() override;
    bool is_dead_end(EvaluationContext& eval_context) const override;
    bool is_reliable_dead_end(EvaluationContext& eval_context) const override;
    void get_path_dependent_evaluators(std::set<Evaluator*>& evals) override;
    void notify_new_expansion(const Entry& parent_entry) override;
};

class LWOpenListFactory : public OpenListFactory
{
    std::shared_ptr<Evaluator> evaluator;
    int random_seed;

public:
    LWOpenListFactory(
        const std::shared_ptr<Evaluator>& evaluator,
        int random_seed);

    virtual std::unique_ptr<StateOpenList> create_state_open_list() override;
    virtual std::unique_ptr<EdgeOpenList> create_edge_open_list() override;
};

}

#endif
