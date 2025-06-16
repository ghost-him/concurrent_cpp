#include "gtest/gtest.h"
#include "../concurrent_queue.hpp" // 引入你的并发队列头文件
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <set>
#include <atomic>

// 注意：你的原始代码中包含 `std::cout` 语句。在运行并发测试时，
// 这会产生大量的控制台输出，可能会影响性能和可读性。
// 在生产级代码中，建议移除这些调试输出。

// 测试套件名称为 ConcurrentQueueV3Test
class ConcurrentQueueV3Test : public ::testing::Test {
protected:
    concurrent_queue_v3<int> q;
};

// 1. 基本功能测试：单线程下推入和弹出
TEST_F(ConcurrentQueueV3Test, BasicPushPop) {
    // 推入一个元素
    q.push(42);

    // 弹出一个元素
    std::unique_ptr<int> val = q.pop();

    // 验证弹出的值
    ASSERT_NE(val, nullptr); // 确保指针非空
    EXPECT_EQ(*val, 42);

    // 再次弹出，队列应该为空
    val = q.pop();
    EXPECT_EQ(val, nullptr); // 确保返回空指针
}

// 2. 边缘情况测试：从空队列弹出
TEST_F(ConcurrentQueueV3Test, PopFromEmpty) {
    // 直接从空队列弹出
    std::unique_ptr<int> val = q.pop();

    // 应该安全地返回一个空指针
    EXPECT_EQ(val, nullptr);
}

// 3. 核心并发测试：多生产者，多消费者
TEST_F(ConcurrentQueueV3Test, MultipleProducersMultipleConsumers) {
    concurrent_queue_v3<int> concurrent_q;

    // --- 配置 ---
    const int num_producers = 8; // 生产者线程数
    const int num_consumers = 8; // 消费者线程数
    const int items_per_producer = 5000; // 每个生产者生产的物品数量
    const int total_items = num_producers * items_per_producer;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // 每个消费者线程收集到的结果
    std::vector<std::vector<int>> consumer_results(num_consumers);

    // 用于通知消费者所有生产者都已完成任务
    std::atomic<bool> producers_finished(false);

    // --- 创建并启动生产者线程 ---
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < items_per_producer; ++j) {
                // 每个生产者推入一组唯一的值，便于后续验证
                int value = i * items_per_producer + j;
                concurrent_q.push(value);
            }
        });
    }

    // --- 创建并启动消费者线程 ---
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&, i]() {
            // 当生产者还在工作时，持续尝试弹出
            while (!producers_finished.load()) {
                std::unique_ptr<int> value_ptr = concurrent_q.pop();
                if (value_ptr) {
                    consumer_results[i].push_back(*value_ptr);
                }
            }
            // 生产者完成后，清空队列中剩余的元素
            while (true) {
                std::unique_ptr<int> value_ptr = concurrent_q.pop();
                if (value_ptr) {
                    consumer_results[i].push_back(*value_ptr);
                } else {
                    // 当 pop 返回 nullptr 时，意味着队列此时为空，可以退出
                    break;
                }
            }
        });
    }

    // --- 等待所有生产者完成 ---
    for (auto& t : producers) {
        t.join();
    }

    // --- 通知消费者生产者已完成 ---
    producers_finished.store(true);

    // --- 等待所有消费者完成 ---
    for (auto& t : consumers) {
        t.join();
    }

    // --- 验证结果 ---
    // 1. 将所有消费者的结果汇总到一个 vector 中
    std::vector<int> all_results;
    size_t total_popped = 0;
    for (const auto& vec : consumer_results) {
        total_popped += vec.size();
        all_results.insert(all_results.end(), vec.begin(), vec.end());
    }

    // 2. 验证数量：弹出的总数必须等于推入的总数
    ASSERT_EQ(total_popped, total_items) << "Mismatch between pushed and popped item counts.";

    // 3. 验证内容：所有推入的数字都必须被弹出，且没有重复或丢失
    // 为了方便验证，我们对结果进行排序
    std::sort(all_results.begin(), all_results.end());

    // 创建一个期望的结果序列（0, 1, 2, ..., total_items - 1）
    std::vector<int> expected_results(total_items);
    std::iota(expected_results.begin(), expected_results.end(), 0);

    // 逐个比较
    EXPECT_EQ(all_results, expected_results) << "The set of popped items does not match the set of pushed items.";
}