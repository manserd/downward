#ifndef OPEN_LISTS_V0_OPEN_LIST_H
#define OPEN_LISTS_V0_OPEN_LIST_H

#include "../open_list_factory.h"

namespace v0_open_list {

class V0OpenListFactory : public OpenListFactory {

    std::shared_ptr<Evaluator> heuristic;
    std::vector<std::shared_ptr<Evaluator>> evaluators;
    int random_seed;

public:

    V0OpenListFactory(const std::shared_ptr<Evaluator> &heuristic, const std::vector<std::shared_ptr<Evaluator>> &evaluators, int random_seed);

    virtual std::unique_ptr<StateOpenList> create_state_open_list() override;
    virtual std::unique_ptr<EdgeOpenList> create_edge_open_list() override;
};

}

#endif
