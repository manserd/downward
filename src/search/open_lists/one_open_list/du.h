#ifndef OPEN_LISTS_ONE_OPEN_LIST_DU_H
#define OPEN_LISTS_ONE_OPEN_LIST_DU_H

#include "../one_open_list.h"

template<typename BucketKey, typename BucketItem> class DUBuckets
{
    using Bucket = UBucket<BucketItem>;

    struct Group
    {
        std::vector<std::size_t> positions;
        double weight{};
    };

    struct Item
    {
        BucketKey key;
        Bucket bucket;
        int bias{};
        std::size_t position_in_group{};
    };

    std::vector<Item> items;
    utils::HashMap<int, Group> group_by_bias;
    utils::HashMap<BucketKey, std::size_t> position_by_bucket_key;
    double total_weight = 0;
    double bucket_temperature;

    void add_to_group(std::size_t position)
    {
        assert(utils::in_bounds(position, items));
        auto bias = items[position].bias;
        const bool is_new_group = !group_by_bias.contains(bias);
        auto& group = group_by_bias[bias];
        if (is_new_group) {
            group.weight = std::exp(-bias / bucket_temperature);
            total_weight += group.weight;
        }
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
            total_weight -= group.weight;
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
    explicit DUBuckets(
        const double bucket_temperature
    ) : bucket_temperature(bucket_temperature)
    {
    }

    template<typename TypeInfo> void bucket_add(const TypeInfo& info, const BucketItem& entry)
    {
        if (!position_by_bucket_key.contains(info.key)) {
            add_bucket(info.key, -info.depth);
        }
        auto position = position_by_bucket_key.at(info.key);
        assert(utils::in_bounds(position, items));
        Bucket& bucket = items[position].bucket;
        bucket.add(entry, info.h);
    }

    BucketItem remove_random(RNG& rng)
    {
        assert(!empty());

        /* Select bucket. */
        double x = rng->random() * total_weight;
        const Group* group = nullptr;
        for (const auto& [candidate_bias, candidate_group] : group_by_bias) {
            group = &candidate_group;
            if (x < candidate_group.weight) {
                break;
            }
            x -= candidate_group.weight;
        }
        assert(!group->positions.empty());
        auto position_in_group = rng->random(group->positions.size());
        assert(utils::in_bounds(position_in_group, group->positions));
        std::size_t position = group->positions[position_in_group];

        /* Select entry. */
        assert(utils::in_bounds(position, items));
        Bucket& bucket = items[position].bucket;
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
        total_weight = 0;
    }
};

#endif
