#ifndef OPEN_LISTS_ONE_OPEN_LIST_UU_H
#define OPEN_LISTS_ONE_OPEN_LIST_UU_H

#include "../one_open_list.h"

template<typename BucketKey, typename BucketItem> class UUBuckets
{
    using Bucket = UBucket<BucketItem>;

    struct Item
    {
        BucketKey key;
        Bucket bucket;
    };

    std::vector<Item> items;
    utils::HashMap<BucketKey, std::size_t> position_by_bucket_key;

    void add_bucket(const BucketKey& bucket_key)
    {
        position_by_bucket_key[bucket_key] = items.size();
        items.push_back(Item{.key = bucket_key, .bucket = Bucket()});
    }

public:
    template<typename TypeInfo> void bucket_add(const TypeInfo& info, const BucketItem& entry)
    {
        if (!position_by_bucket_key.contains(info.key)) {
            add_bucket(info.key);
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
        std::size_t position = rng->random(items.size());

        /* Select entry. */
        assert(utils::in_bounds(position, items));
        Bucket& bucket = items[position].bucket;
        auto entry = bucket.remove_random(rng);

        /* Delete bucket if empty. */
        if (bucket.empty()) {
            position_by_bucket_key.erase(items[position].key);
            if (position != items.size() - 1) {
                auto& swap_item = items.back();
                position_by_bucket_key.at(swap_item.key) = position;
                items[position] = std::move(items.back());
            }
            items.pop_back();
        }

        return entry;
    }

    [[nodiscard]] bool empty() const
    {
        return position_by_bucket_key.empty();
    }

    void clear()
    {
        items.clear();
        position_by_bucket_key.clear();
    }
};

#endif
