#include "eps_open_list.h"

#include "../evaluator.h"
#include "../utils/rng.h"
#include "../utils/rng_options.h"

namespace eps_open_list {

using RNG = std::shared_ptr<utils::RandomNumberGenerator>;

template<class Entry> class EpsOpenList : public OpenList<Entry>
{
    const std::unique_ptr<OpenList<Entry>> main;
    const std::unique_ptr<OpenList<Entry>> alt;
    bool prev_was_main = false;
    int main_wins = 0;
    int alt_wins = 0;
    const double epsilon;
    RNG rng;

protected:
    void do_insertion(EvaluationContext &eval_context, const Entry &entry) override
    {
        main->insert(eval_context, entry);
        alt->insert(eval_context, entry);
    }

public:
    EpsOpenList(
        const std::shared_ptr<OpenListFactory>& main_factory,
        const std::shared_ptr<OpenListFactory>& alt_factory,
        const double epsilon,
        const int random_seed
    ) : main(main_factory->create_open_list<Entry>()),
        alt(alt_factory->create_open_list<Entry>()),
        epsilon(epsilon)
    {
        rng = utils::get_rng(random_seed);
    }

    void notify_new_expansion(const Entry& parent_entry) override
    {
        main->notify_new_expansion(parent_entry);
        alt->notify_new_expansion(parent_entry);
    }

    void notify_prev_was_closed() override
    {
        if (prev_was_main) {
            --main_wins;
        } else {
            --alt_wins;
        }
    }

    void print_statistics(utils::LogProxy& log) override
    {
        log << "Exploration ratio: " << static_cast<double>(alt_wins) / main_wins << std::endl;
    }

    Entry remove_min() override
    {
        if ((rng->random() < epsilon && !alt->empty()) || main->empty()) {
            prev_was_main = false;
            ++alt_wins;
            return alt->remove_min();
        }
        prev_was_main = true;
        ++main_wins;
        return main->remove_min();
    }

    bool empty() const override
    {
        return main->empty() && alt->empty();
    }

    void clear() override
    {
        main->clear();
        alt->clear();
    }

    bool is_dead_end(EvaluationContext &eval_context) const override
    {
        return is_reliable_dead_end(eval_context) || (main->is_dead_end(eval_context) && alt->is_dead_end(eval_context));
    }

    bool is_reliable_dead_end(EvaluationContext &eval_context) const override
    {
        return main->is_reliable_dead_end(eval_context) || alt->is_reliable_dead_end(eval_context);
    }

    void get_path_dependent_evaluators(std::set<Evaluator *> &evals) override
    {
        main->get_path_dependent_evaluators(evals);
        alt->get_path_dependent_evaluators(evals);
    }
};

class EpsOpenListFactory : public OpenListFactory
{
    const std::shared_ptr<OpenListFactory> main;
    const std::shared_ptr<OpenListFactory> alt;
    const double epsilon;
    const int random_seed;

public:
    EpsOpenListFactory(
        const std::shared_ptr<OpenListFactory> main,
        const std::shared_ptr<OpenListFactory> alt,
        const double epsilon,
        const int random_seed
    ) : main(main),
        alt(alt),
        epsilon(epsilon),
        random_seed(random_seed)
    {}

    std::unique_ptr<StateOpenList> create_state_open_list() override
    {
        return utils::make_unique_ptr<EpsOpenList<StateOpenListEntry>>(main, alt, epsilon, random_seed);
    }

    std::unique_ptr<EdgeOpenList> create_edge_open_list() override
    {
        return utils::make_unique_ptr<EpsOpenList<EdgeOpenListEntry>>(main, alt, epsilon, random_seed);
    }
};

class EpsOpenListFeature : public plugins::TypedFeature<OpenListFactory, EpsOpenListFactory>
{
protected:
    [[nodiscard]] std::shared_ptr<EpsOpenListFactory> create_component(const plugins::Options& opts, const utils::Context&) const override
    {
        return plugins::make_shared_from_arg_tuples<EpsOpenListFactory>(
            opts.get<std::shared_ptr<OpenListFactory>>("main"),
            opts.get<std::shared_ptr<OpenListFactory>>("alt"),
            opts.get<double>("epsilon"),
            utils::get_rng_arguments_from_options(opts)
        );
    }

public:
    EpsOpenListFeature() : TypedFeature("eps")
    {
        add_option<std::shared_ptr<OpenListFactory>>("main");
        add_option<std::shared_ptr<OpenListFactory>>("alt");
        add_option<double>("epsilon");
        utils::add_rng_options_to_feature(*this);
    }
};

[[maybe_unused]] static plugins::FeaturePlugin<EpsOpenListFeature> _plugin;

}
