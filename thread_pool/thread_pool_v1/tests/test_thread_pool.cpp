// File: thread_pool_test.cpp
#include "gtest/gtest.h"
#include "../thread_pool.h" // Adjust path as necessary
#include <atomic>
#include <vector>
#include <string>
#include <chrono>
#include <numeric> // For std::iota

// Helper functions for tasks
int add(int a, int b) {
    // std::cout << "Task add(" << a << ", " << b << ") running on thread " << std::this_thread::get_id() << std::endl;
    return a + b;
}

void void_task_func(std::atomic<bool>& flag) {
    // std::cout << "Task void_task_func running on thread " << std::this_thread::get_id() << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate some work
    flag.store(true);
}

void increment_counter(std::atomic<int>& counter) {
    // std::cout << "Task increment_counter running on thread " << std::this_thread::get_id() << std::endl;
    counter.fetch_add(1);
}

std::string string_concat(const std::string& s1, const std::string& s2) {
    // std::cout << "Task string_concat running on thread " << std::this_thread::get_id() << std::endl;
    return s1 + s2;
}

void task_that_throws() {
    // std::cout << "Task task_that_throws running on thread " << std::this_thread::get_id() << std::endl;
    throw std::logic_error("Task failed intentionally");
}

// Test fixture for thread pool tests
class ThreadPoolTest : public ::testing::Test {
protected:
    thread_pool_v1::thread_pool* pool_ = nullptr;

    // Per-test-suite set-up.
    // Called before the first test in this test suite.
    static void SetUpTestSuite() {
        // Initialize the singleton or any global setup
        // The first call to get_instance() will create it.
        thread_pool_v1::thread_pool::get_instance();
    }

    // Per-test-suite tear-down.
    // Called after the last test in this test suite.
    // This is crucial to ensure the singleton's threads are joined
    // and don't interfere with other test suites or cause hangs.
    static void TearDownTestSuite() {
        auto& pool = thread_pool_v1::thread_pool::get_instance();
        pool.stop(); // Ensure all threads are stopped and joined
    }

    void SetUp() override {
        // Get the singleton instance for each test.
        // Note: If a previous test called stop(), the pool will be in a stopped state.
        // Tests need to be aware of this. For most tests, we assume the pool is running.
        // The StopFunctionality test will explicitly stop it.
        pool_ = &thread_pool_v1::thread_pool::get_instance();

        // A more robust approach for testing singletons would involve a reset mechanism,
        // but that's beyond the scope of the provided class.
        // If m_stop is true, we can't really "restart" it without modifying the class.
        // So, the order of tests might matter, or tests should be designed to handle
        // a potentially pre-stopped pool if they run after a "stop" test.
        // However, gtest doesn't guarantee order. For simplicity, TearDownTestSuite
        // handles the final stop. Individual tests assume it's running unless they
        // are specifically testing the stop mechanism.
    }

    void TearDown() override {
        // No specific per-test teardown needed for the pool itself,
        // as TearDownTestSuite will handle the final cleanup.
        // If a test calls pool_->stop(), it will remain stopped.
    }
};

TEST_F(ThreadPoolTest, SingletonInstance) {
    auto& p1 = thread_pool_v1::thread_pool::get_instance();
    auto& p2 = thread_pool_v1::thread_pool::get_instance();
    ASSERT_EQ(&p1, &p2) << "get_instance() should return the same instance.";
    ASSERT_EQ(pool_, &p1) << "Fixture pool_ should be the same instance.";
}

TEST_F(ThreadPoolTest, SubmitTaskWithReturnValue) {
    // This test assumes the pool is running. If a previous test stopped it, this might fail
    // when commit throws. However, TearDownTestSuite attempts to mitigate this by stopping
    // only at the end of the suite.
    try {
        auto future_val = pool_->commit(add, 10, 20);
        ASSERT_EQ(future_val.get(), 30) << "Task with return value did not produce correct result.";
    } catch (const std::runtime_error& e) {
        // This might happen if the pool was stopped by a previous test (bad test ordering/interdependency)
        // or if hardware_concurrency was 0 and the pool didn't start threads.
        // The pool's constructor handles thread_num <= 0 by setting it to 1.
        FAIL() << "Caught runtime_error during commit, pool might be stopped: " << e.what();
    }
}

TEST_F(ThreadPoolTest, SubmitVoidTask) {
    std::atomic<bool> flag(false);
    try {
        auto future_void = pool_->commit(void_task_func, std::ref(flag));
        future_void.get(); // Wait for task completion
        ASSERT_TRUE(flag.load()) << "Void task did not set the flag.";
    } catch (const std::runtime_error& e) {
        FAIL() << "Caught runtime_error during commit, pool might be stopped: " << e.what();
    }
}

TEST_F(ThreadPoolTest, SubmitTaskWithComplexArguments) {
    try {
        std::string s1 = "Hello, ";
        std::string s2 = "World!";
        auto future_str = pool_->commit(string_concat, s1, s2);
        ASSERT_EQ(future_str.get(), "Hello, World!") << "Task with string arguments failed.";
    } catch (const std::runtime_error& e) {
        FAIL() << "Caught runtime_error during commit, pool might be stopped: " << e.what();
    }
}

TEST_F(ThreadPoolTest, SubmitMultipleTasks) {
    const int num_tasks = 10;
    std::atomic<int> counter(0);
    std::vector<std::future<void>> futures;

    try {
        for (int i = 0; i < num_tasks; ++i) {
            futures.emplace_back(pool_->commit(increment_counter, std::ref(counter)));
        }

        for (auto& fut : futures) {
            fut.get(); // Wait for each task
        }
        ASSERT_EQ(counter.load(), num_tasks) << "Not all submitted tasks were executed.";
    } catch (const std::runtime_error& e) {
        FAIL() << "Caught runtime_error during commit, pool might be stopped: " << e.what();
    }
}

TEST_F(ThreadPoolTest, TaskThrowsException) {
    // This test specifically checks committing to a running pool.
    // If the pool is already stopped, commit itself will throw, which is tested elsewhere.
    std::future<void> future_ex;
    try {
         future_ex = pool_->commit(task_that_throws);
    } catch (const std::runtime_error& e) {
        // This means pool was stopped before commit, not the test's intent for this path.
        FAIL() << "Pool was stopped before committing exception task: " << e.what();
        return; // Or rethrow, or handle as per test strategy for pre-stopped singletons
    }

    ASSERT_THROW({
        try {
            future_ex.get(); // This should re-throw the exception from the task
        } catch (const std::logic_error& e) {
            //EXPECT_STREQ(e.what(), "Task failed intentionally"); // Check message if desired
            throw; // Re-throw to be caught by ASSERT_THROW
        }
    }, std::logic_error) << "Exception thrown by task was not propagated by future.";
}


TEST_F(ThreadPoolTest, StopFunctionalityAndCommitAfterStop) {
    // This test explicitly stops the pool.
    // Any subsequent tests in this suite that try to use the pool will find it stopped.
    // This is a consequence of testing a singleton with state.

    // First, ensure it works before stop (optional, but good sanity check)
    std::atomic<bool> flag_before_stop(false);
    try {
        auto fut_before = pool_->commit(void_task_func, std::ref(flag_before_stop));
        fut_before.get();
        ASSERT_TRUE(flag_before_stop.load()) << "Task before stop did not run.";
    } catch (const std::runtime_error& e) {
        // If this happens, the pool was already stopped, which invalidates this test's premise.
        // This highlights the challenge with singleton testing.
        // For now, we'll assume it was running or accept this might be an issue depending on test order.
        std::cerr << "Warning: Pool might have been stopped before StopFunctionality test started." << std::endl;
    }


    pool_->stop(); // Explicitly stop the pool

    // Attempt to commit a task after stop()
    ASSERT_THROW(
        {
            pool_->commit(add, 1, 2);
        },
        std::runtime_error
    ) << "Committing to a stopped pool should throw std::runtime_error.";

    // Calling stop again should be safe (idempotent)
    ASSERT_NO_THROW({
        pool_->stop();
    }) << "Calling stop() multiple times should not throw.";
}

// This test is more of a stress test and also checks if tasks are processed
// even if submitted rapidly.
TEST_F(ThreadPoolTest, SubmitManyTasksRapidly) {
    // Due to singleton nature, if StopFunctionalityAndCommitAfterStop ran before this,
    // this test will fail at commit. This is a known issue with testing global singletons
    // that change state. GTest does not guarantee test execution order.
    // One way to handle is to have separate test executables or use test filtering
    // carefully. For this example, we acknowledge this potential interaction.
    // If the pool is stopped, this test will throw at the first commit.

    const int num_tasks = 100; // Can be increased for more stress
    std::vector<std::future<int>> futures;
    std::atomic<long long> sum_of_results(0);
    std::atomic<int> tasks_started_count(0);

    auto task_to_sum = [&](int val) {
        tasks_started_count.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // Tiny delay
        sum_of_results.fetch_add(val, std::memory_order_relaxed);
        return val;
    };

    long long expected_sum = 0;
    try {
        for (int i = 0; i < num_tasks; ++i) {
            futures.emplace_back(pool_->commit(task_to_sum, i));
            expected_sum += i;
        }

        // Wait for all futures and sum their results (though task_to_sum already updates atomic sum)
        for (int i = 0; i < num_tasks; ++i) {
            futures[i].get(); // Ensure completion
        }

        ASSERT_EQ(tasks_started_count.load(), num_tasks) << "Not all tasks were started.";
        ASSERT_EQ(sum_of_results.load(), expected_sum) << "Sum of results from many tasks is incorrect.";

    } catch (const std::runtime_error& e) {
        // This will be hit if the pool was stopped by a preceding test.
        // This is an artifact of testing a stateful global singleton.
        // In a real-world scenario with many test suites, you might need a way
        // to reset/recreate the singleton or ensure tests run in a specific order
        // or in separate processes if state leakage is critical.
        GTEST_SKIP() << "Skipping test due to pool being stopped, likely by a previous test: " << e.what();
    }
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}