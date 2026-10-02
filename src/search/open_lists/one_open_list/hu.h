#ifndef OPEN_LISTS_ONE_OPEN_LIST_HU_H
#define OPEN_LISTS_ONE_OPEN_LIST_HU_H

#include "../one_open_list.h"

template<typename BucketKey, typename BucketItem> class HUBuckets
{
    using Bucket = UBucket<BucketItem>;

    struct Item
    {
        BucketKey key;
        Bucket bucket;
        int bias{};
        std::size_t position_in_group{};
    };

    std::vector<Item> items;
    utils::HashMap<BucketKey, std::size_t> position_by_bucket_key;

    struct Group
    {
        std::vector<std::size_t> positions;
    };

    utils::HashMap<int, Group> group_by_bias;
    double bucket_temperature;

    [[nodiscard]] double compute_weight(const int bias, const int shift) const
    {
        const double weight = std::exp(-(bias - shift) / bucket_temperature);
        if (!std::isfinite(weight)) {
            printf("std::exp overflow!\n");
            exit(1);
        }
        return weight;
    }

    void add_to_group(std::size_t position)
    {
        assert(utils::in_bounds(position, items));
        auto bias = items[position].bias;
        auto& group = group_by_bias[bias];
        items[position].position_in_group = group.positions.size();
        group.positions.push_back(position);
    }

    void remove_from_group(std::size_t position)
    {
        assert(utils::in_bounds(position, items));
        auto& item = items[position];
        auto& group = group_by_bias.at(item.bias);
        if (item.position_in_group != group.positions.size() - 1) {
            assert(utils::in_bounds(item.position_in_group, group.positions));
            group.positions[item.position_in_group] = group.positions.back();
            assert(utils::in_bounds(group.positions[item.position_in_group], items));
            items[group.positions[item.position_in_group]].position_in_group = item.position_in_group;
        }
        group.positions.pop_back();
        if (group.positions.empty()) {
            group_by_bias.erase(item.bias);
        }
    }

    void add_bucket(const BucketKey& bucket_key, int bias)
    {
        position_by_bucket_key[bucket_key] = items.size();
        items.push_back(Item{.key = bucket_key, .bucket = Bucket(), .bias = bias});
        add_to_group(items.size() - 1);
    }

public:
    explicit HUBuckets(
        const double bucket_temperature
    ) : bucket_temperature(bucket_temperature)
    {
    }

    template<typename TypeInfo> void bucket_add(const TypeInfo& info, const BucketItem& entry)
    {
        if (!position_by_bucket_key.contains(info.key)) {
            add_bucket(info.key, info.h);
        }
        auto position = position_by_bucket_key.at(info.key);
        assert(utils::in_bounds(position, items));
        Bucket& bucket = items[position].bucket;
        int min_h_before = bucket.get_min_h();
        bucket.add(entry, info.h);
        if (bucket.get_min_h() != min_h_before) {
            remove_from_group(position);
            items[position].bias = bucket.get_min_h();
            add_to_group(position);
        }
    }

    BucketItem remove_random(RNG& rng)
    {
        assert(!empty());

        /* Select bucket. */
        int min_bias = EvaluationResult::INFTY;
        for (auto& bias : group_by_bias | std::views::keys) {
            min_bias = std::min(min_bias, bias);
        }
        double total_weight = 0;
        for (auto& bias : group_by_bias | std::views::keys) {
            total_weight += compute_weight(bias, min_bias);
        }
        double x = rng->random() * total_weight;
        const Group* group = nullptr;
        for (const auto& [candidate_bias, candidate_group] : group_by_bias) {
            group = &candidate_group;
            const double weight = compute_weight(candidate_bias, min_bias);
            if (x < weight) {
                break;
            }
            x -= weight;
        }
        assert(!group->positions.empty());
        auto position_in_group = rng->random(group->positions.size());
        assert(utils::in_bounds(position_in_group, group->positions));
        std::size_t position = group->positions[position_in_group];

        /* Select entry. */
        assert(utils::in_bounds(position, items));
        Bucket& bucket = items[position].bucket;
        int min_h_before = bucket.get_min_h();
        auto entry = bucket.remove_random(rng);

        /* Delete bucket if empty. */
        if (bucket.empty()) {
            remove_from_group(position);
            position_by_bucket_key.erase(items[position].key);
            if (position != items.size() - 1) {
                auto& swap_item = items.back();
                group_by_bias.at(swap_item.bias).positions[swap_item.position_in_group] = position;
                position_by_bucket_key.at(swap_item.key) = position;
                items[position] = std::move(items.back());
            }
            items.pop_back();
        }

        /* else, update group if min h changed. */
        else if (bucket.get_min_h() != min_h_before) {
            remove_from_group(position);
            items[position].bias = bucket.get_min_h();
            add_to_group(position);
        }

        return entry;
    }

    [[nodiscard]] bool empty() const
    {
        return group_by_bias.empty();
    }

    void clear()
    {
        items.clear();
        group_by_bias.clear();
        position_by_bucket_key.clear();
    }
};

#endif
