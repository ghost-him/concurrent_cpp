//
// Created by ghost-him on 25-5-31.
//


#include <gtest/gtest.h>

#include "concurrent_stack_v2.hpp"

// 测试套件，用于组织相关的测试
class ConcurrentStackV2Test : public ::testing::Test {
protected:
    concurrent_stack_v2<int> stack;
};

// 测试1: 单线程环境下的基本功能正确性
TEST_F(ConcurrentStackV2Test, SingleThreadCorrectness) {
    // 1. 从空栈中弹出，应返回 nullptr
    EXPECT_EQ(stack.pop(), nullptr);

    // 2. 推入一个元素并弹出
    stack.push(42);
    std::shared_ptr<int> val1 = stack.pop();
    ASSERT_NE(val1, nullptr);
    EXPECT_EQ(*val1, 42);

    // 3. 栈现在应该是空的
    EXPECT_EQ(stack.pop(), nullptr);

    // 4. 测试后进先出 (LIFO) 顺序
    stack.push(1);
    stack.push(2);
    stack.push(3);

    auto pop3 = stack.pop();
    auto pop2 = stack.pop();
    auto pop1 = stack.pop();

    ASSERT_NE(pop3, nullptr);
    EXPECT_EQ(*pop3, 3);
    ASSERT_NE(pop2, nullptr);
    EXPECT_EQ(*pop2, 2);
    ASSERT_NE(pop1, nullptr);
    EXPECT_EQ(*pop1, 1);

    // 5. 栈最终为空
    EXPECT_EQ(stack.pop(), nullptr);
}

// 测试2: 多线程先并发推入，再并发弹出
TEST_F(ConcurrentStackV2Test, MultiThreadPushThenPop) {
    const int num_threads = 10;
    const int items_per_thread = 1000;
    const int total_items = num_threads * items_per_thread;

    std::vector<std::thread> threads;
    std::set<int> expected_values;

    // --- 推入阶段 ---
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < items_per_thread; ++j) {
                int value = i * items_per_thread + j;
                stack.push(value);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    threads.clear();

    // --- 弹出阶段 ---
    std::vector<int> popped_values;
    std::mutex mtx; // 保护 popped_values 的并发写入
    std::atomic<int> pop_count(0);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            // 每个线程持续弹出，直到所有元素都被取出
            while(pop_count.load() < total_items) {
                auto val_ptr = stack.pop();
                if (val_ptr) {
                    std::lock_guard<std::mutex> lock(mtx);
                    popped_values.push_back(*val_ptr);
                    pop_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // --- 验证阶段 ---
    // 1. 验证弹出的元素总数是否正确
    ASSERT_EQ(popped_values.size(), total_items);

    // 2. 将弹出的值放入 set 中，以检查是否有重复值，并方便与期望值比较
    std::set<int> popped_set(popped_values.begin(), popped_values.end());
    ASSERT_EQ(popped_set.size(), total_items) << "Duplicate values were popped from the stack.";

    // 3. 构造期望值的集合
    for (int i = 0; i < total_items; ++i) {
        expected_values.insert(i);
    }

    // 4. 比较实际弹出的集合和期望的集合是否完全一致
    EXPECT_EQ(popped_set, expected_values);

    // 5. 最终栈应为空
    EXPECT_EQ(stack.pop(), nullptr);
}


// 测试3: 多线程混合并发推入和弹出
TEST_F(ConcurrentStackV2Test, MultiThreadMixedPushAndPop) {
    const int num_producer_threads = 8;
    const int num_consumer_threads = 8;
    const int items_per_producer = 1000;
    const int total_items = num_producer_threads * items_per_producer;

    std::vector<std::thread> threads;
    std::set<int> popped_values;
    std::mutex mtx; // 保护 popped_values 的并发写入

    // --- 创建生产者线程 ---
    for (int i = 0; i < num_producer_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < items_per_producer; ++j) {
                int value = i * items_per_producer + j;
                stack.push(value);
            }
        });
    }

    // --- 创建消费者线程 ---
    std::atomic<int> consumed_count(0);
    for (int i = 0; i < num_consumer_threads; ++i) {
        threads.emplace_back([&]() {
            // 当已消费数量小于总数时，持续尝试消费
            while (consumed_count.load() < total_items) {
                auto val_ptr = stack.pop();
                if (val_ptr) {
                    // 如果成功消费一个，就加锁并记录
                    std::lock_guard<std::mutex> lock(mtx);
                    // 再次检查，防止多余的消费线程进入
                    if (popped_values.size() < total_items) {
                        popped_values.insert(*val_ptr);
                        consumed_count++;
                    }
                } else {
                    // 如果栈为空，让出CPU时间片，避免忙等待
                    std::this_thread::yield();
                }
            }
        });
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    // --- 验证阶段 ---
    // 1. 验证弹出的元素总数是否正确
    ASSERT_EQ(popped_values.size(), total_items);

    // 2. 构造期望值的集合
    std::set<int> expected_values;
    for (int i = 0; i < total_items; ++i) {
        expected_values.insert(i);
    }

    // 3. 比较实际弹出的集合和期望的集合是否完全一致
    EXPECT_EQ(popped_values, expected_values);

    // 4. 最终栈应为空
    EXPECT_EQ(stack.pop(), nullptr);
}