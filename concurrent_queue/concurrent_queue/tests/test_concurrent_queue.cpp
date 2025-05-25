//
// Created by ghost-him on 25-5-25.
//
// queue_test.cpp
#include "gtest/gtest.h"
#include "../concurrent_queue.hpp" // 包含你的 concurrent_queue.h 头文件
#include <thread>
#include <vector>
#include <numeric>
#include <atomic>
#include <set>
#include <algorithm> // For std::sort
#include <chrono>    // For std::chrono::milliseconds

// Test fixture for common setup/teardown (optional, but good practice)
// For simple tests, direct TEST_F is also fine.
template<typename T, size_t Cap>
class ConcurrentQueueTest : public ::testing::Test {
protected:
    concurrent_queue<T, Cap>* queue;

    void SetUp() override {
        queue = new concurrent_queue<T, Cap>();
    }

    void TearDown() override {
        delete queue;
    }
};

// Define a test case for a specific capacity
using ConcurrentQueueInt5 = ConcurrentQueueTest<int, 5>;
using ConcurrentQueueInt1 = ConcurrentQueueTest<int, 1>;
using ConcurrentQueueInt10 = ConcurrentQueueTest<int, 10>;
using ConcurrentQueueInt100 = ConcurrentQueueTest<int, 100>;


// --- Basic Functionality Tests ---

TEST_F(ConcurrentQueueInt5, IsEmptyInitially) {
    ASSERT_TRUE(queue->is_empty());
    ASSERT_FALSE(queue->is_full());
}

TEST_F(ConcurrentQueueInt5, PushOneElement) {
    int val = 42;
    ASSERT_TRUE(queue->push(val));
    ASSERT_FALSE(queue->is_empty());
    ASSERT_FALSE(queue->is_full()); // Not full yet (cap is 5)
}

TEST_F(ConcurrentQueueInt5, PopFromEmptyReturnsFalse) {
    int val;
    ASSERT_FALSE(queue->pop(val));
}

TEST_F(ConcurrentQueueInt5, PushThenPopOneElement) {
    int pushed_val = 123;
    ASSERT_TRUE(queue->push(pushed_val));
    ASSERT_FALSE(queue->is_empty());

    int popped_val;
    ASSERT_TRUE(queue->pop(popped_val));
    ASSERT_EQ(pushed_val, popped_val);
    ASSERT_TRUE(queue->is_empty());
}

TEST_F(ConcurrentQueueInt1, PushPopWithCapacityOne) {
    int val = 10;
    ASSERT_TRUE(queue->push(val));
    ASSERT_FALSE(queue->is_empty());
    ASSERT_TRUE(queue->is_full()); // Should be full with capacity 1

    int popped_val;
    ASSERT_TRUE(queue->pop(popped_val));
    ASSERT_EQ(val, popped_val);
    ASSERT_TRUE(queue->is_empty());
    ASSERT_FALSE(queue->is_full());
}

TEST_F(ConcurrentQueueInt10, PushTillFull) {
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(queue->push(i));
    }
    ASSERT_TRUE(queue->is_full());
    ASSERT_FALSE(queue->is_empty());

    // Try to push one more, should fail
    ASSERT_FALSE(queue->push(99));
}

TEST_F(ConcurrentQueueInt10, PushAndPopAllElements) {
    std::vector<int> pushed_values;
    for (int i = 0; i < 10; ++i) {
        pushed_values.push_back(i * 2);
        ASSERT_TRUE(queue->push(i * 2));
    }
    ASSERT_TRUE(queue->is_full());

    std::vector<int> popped_values;
    int val;
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(queue->pop(val));
        popped_values.push_back(val);
    }
    ASSERT_TRUE(queue->is_empty());
    ASSERT_FALSE(queue->is_full());

    // Check if values are in correct order (FIFO)
    ASSERT_EQ(pushed_values.size(), popped_values.size());
    for (size_t i = 0; i < pushed_values.size(); ++i) {
        ASSERT_EQ(pushed_values[i], popped_values[i]);
    }
}

TEST_F(ConcurrentQueueInt10, PushPopCycle) {
    // Fill half
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(queue->push(i));
    }

    // Pop half
    int val;
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(queue->pop(val));
        ASSERT_EQ(i, val);
    }
    ASSERT_TRUE(queue->is_empty());

    // Fill again
    for (int i = 10; i < 20; ++i) {
        ASSERT_TRUE(queue->push(i));
    }
    ASSERT_TRUE(queue->is_full());

    // Pop again
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(queue->pop(val));
        ASSERT_EQ(10 + i, val);
    }
    ASSERT_TRUE(queue->is_empty());
}

// --- Concurrency Tests ---

TEST(ConcurrentQueueConcurrencyTest, ProducerConsumerStressTest) {
    constexpr size_t CAPACITY = 100; // Queue capacity
    constexpr int NUM_PRODUCERS = 4;
    constexpr int NUM_CONSUMERS = 4;
    constexpr int ITEMS_PER_PRODUCER = 10000;
    constexpr int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    concurrent_queue<int, CAPACITY> q;
    std::atomic<int> pushed_count{0};
    std::atomic<int> popped_count{0};
    std::mutex popped_values_mutex;
    std::vector<int> popped_values; // Store all popped values for verification

    // Producer function
    auto producer_func = [&](int producer_id) {
        for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
            int item_to_push = producer_id * ITEMS_PER_PRODUCER + i;
            while (!q.push(item_to_push)) {
                // If queue is full, yield to allow consumers to empty it
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            pushed_count.fetch_add(1, std::memory_order_relaxed);
        }
    };

    // Consumer function
    auto consumer_func = [&]() {
        int val;
        // Consumers continue until all items are popped,
        // or a reasonable timeout if the queue might legitimately be empty for periods.
        // For this test, we expect all items to eventually be popped.
        while (popped_count.load(std::memory_order_relaxed) < TOTAL_ITEMS) {
            if (q.pop(val)) {
                std::lock_guard<std::mutex> lock(popped_values_mutex);
                popped_values.push_back(val);
                popped_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                // If queue is empty, yield to allow producers to fill it
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    };

    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back(producer_func, i);
    }

    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back(consumer_func);
    }

    // Join producer threads first
    for (auto& p_thread : producers) {
        p_thread.join();
    }

    // Now, let consumers finish popping any remaining items
    // (they will naturally exit when popped_count reaches TOTAL_ITEMS)
    for (auto& c_thread : consumers) {
        c_thread.join();
    }

    // Verify results
    ASSERT_EQ(pushed_count.load(), TOTAL_ITEMS);
    ASSERT_EQ(popped_count.load(), TOTAL_ITEMS);
    ASSERT_EQ(popped_values.size(), TOTAL_ITEMS);

    // Sort popped values to easily check for uniqueness and correctness
    std::sort(popped_values.begin(), popped_values.end());

    // Check for duplicates
    for (size_t i = 0; i < popped_values.size(); ++i) {
        if (i > 0) {
            ASSERT_NE(popped_values[i], popped_values[i-1]) << "Duplicate found: " << popped_values[i];
        }
    }

    // Verify all original items are present and in the correct range
    std::set<int> expected_items;
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        for (int j = 0; j < ITEMS_PER_PRODUCER; ++j) {
            expected_items.insert(i * ITEMS_PER_PRODUCER + j);
        }
    }

    std::set<int> actual_items(popped_values.begin(), popped_values.end());

    ASSERT_EQ(expected_items.size(), actual_items.size());
    ASSERT_EQ(expected_items, actual_items); // Ensures all expected items are present and nothing extra
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}