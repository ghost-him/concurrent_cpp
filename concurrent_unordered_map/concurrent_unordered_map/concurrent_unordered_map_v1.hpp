#pragma once

#include <iostream>
#include <vector>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <iterator>
#include <map>

template<typename Key, typename Value, typename Hash = std::hash<Key>>
class concurrent_unordered_map {
private:
    class bucket_type {
        friend class concurrent_unordered_map;

    private:
        //存储元素的类型为pair，由key和value构成
        typedef std::pair<Key, Value> bucket_value;
        //由链表存储元素构
        typedef std::list<bucket_value> bucket_data;
        //链表的迭代器
        //如果要让编译器知道iterator是一个类型，所以需要手动指定typename
        // 所以语法就变成了 typename [类型1] [类型2]，把[类型1]定义成[类型2]
        typedef typename bucket_data::iterator bucket_iterator;
        //链表数据
        bucket_data data;
        //改用共享锁
        mutable std::shared_mutex mutex;

        bucket_iterator find_entry_for(const Key& key) {
            return std::find_if(data.begin(), data.end(), [&](const bucket_value& item) {
                return item.first == key;
            });
        }
    public:
        // 查找key值，找到则返回value,否则返回默认值
        Value value_for(const Key& key, const Value& default_value) {
            std::shared_lock<std::shared_mutex> guard(mutex);
            const bucket_iterator found_entry = find_entry_for(key);
            return (found_entry == data.end()) ? default_value : found_entry->second;
        }

        // 添加一个key与value，找到则更新，没找到则添加
        void add_or_update_mapping(const Key & key, const Value& value) {
            std::unique_lock<std::shared_mutex> guard(mutex);
            const bucket_iterator found_entry = find_entry_for(key);
            if (found_entry == data.end()) {
                data.push_back(bucket_value(key,value));
            } else {
                found_entry->second = value;
            }
        }

        // 删除对应的key
        void remove_mapping(const Key& key) {
            std::unique_lock<std::shared_mutex> guard(mutex);
            const bucket_iterator found_entry = find_entry_for(key);
            if (found_entry != data.end()) {
                data.erase(found_entry);
            }
        }
    };

    // 使用vector存储桶的类型
    std::vector<std::unique_ptr<bucket_type>> buckets;
    // hash<key> 哈希表，用于根据key生成哈希值
    Hash hasher;

    bucket_type& get_bucket(const Key& key) const {
        // 计算一个散列值
        const std::size_t bucket_index = hasher(key) % buckets.size();
        return *(buckets[bucket_index]);
    }
public:
    // 桶的大小尽量要使用一个质数。如果不是质数，可能会导致更大概率的哈希冲突
    concurrent_unordered_map(unsigned num_buckets = 19, const Hash & hasher_ = Hash()):buckets(num_buckets), hasher(hasher_) {
        // 表示大小最好用unsigned
        for (unsigned i = 0; i < num_buckets; i ++) {
            buckets[i].reset(new bucket_type);
        }
    }

    concurrent_unordered_map(const concurrent_unordered_map& other) = delete;
    concurrent_unordered_map& operator=(const concurrent_unordered_map& other) = delete;

    Value value_for(const Key& key, const Value& default_value = Value()) {
        return get_bucket(key).value_for(key, default_value);
    }

    void add_or_update_mapping(const Key&key, const Value& value) {
        return get_bucket(key).add_or_update_mapping(key, value);
    }

    void remove_mapping(const Key& key) {
        return get_bucket(key).remove_mapping(key);
    }

    // 返回当前保存的东西
    // 一般不推荐，因为通常在读完以后，会马上就发生更改，所以返回的值在很短的时候内就会变成旧值
    std::map<Key, Value> get_map() {
        std::vector<std::unique_lock<std::shared_mutex>> locks;
        for (unsigned i = 0; i < buckets.size(); i ++) {
            locks.push_back(std::unique_lock<std::shared_mutex>(buckets[i]->mutex));
        }

        std::map<Key, Value> result;
        for (unsigned i = 0; i < buckets.size(); i ++) {
            for (auto it = buckets[i]->data.begin(); it != buckets[i]->data.end(); ++it) {
                result.insert(*it);
            }
        }
        return result;
    }
};
