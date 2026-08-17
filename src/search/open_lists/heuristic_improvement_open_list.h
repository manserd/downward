#ifndef OPEN_LISTS_HEURISTIC_IMPROVEMENT_OPEN_LIST_H
#define OPEN_LISTS_HEURISTIC_IMPROVEMENT_OPEN_LIST_H

#include "../open_list_factory.h"

namespace heuristic_improvement_open_list {
class HeuristicImprovementOpenListFactory : public OpenListFactory
{
    std::shared_ptr<Evaluator> evaluator;
    int random_seed;

public:
    HeuristicImprovementOpenListFactory(
        const std::shared_ptr<Evaluator>& evaluator,
        int random_seed);

    virtual std::unique_ptr<StateOpenList> create_state_open_list() override;
    virtual std::unique_ptr<EdgeOpenList> create_edge_open_list() override;
};
}

#endif
