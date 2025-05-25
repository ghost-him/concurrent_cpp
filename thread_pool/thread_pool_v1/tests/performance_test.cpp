#include <iostream>
#include <vector>
#include <thread> // For std::jthread and std::this_thread
#include <future>
#include <chrono>
#include "../enable_singleton.h"
#include <functional>
#include <numeric>   // For std::accumulate (optional, for using results)
#include <iomanip>   // For std::fixed and std::setprecision

// 确保你的线程池头文件路径正确
#include "../thread_pool.h"
// #include "enable_singleton.h" // 通常 thread_pool.h 会包含它

// --- 参数调整区 ---
// 任务总数
static constexpr int NUM_TASKS = 500000;
// 每个任务的计算复杂度（迭代次数）
static constexpr int TASK_COMPLEXITY = 10000;
// 线程池中的线程数量 (如果为0，则使用 thread_pool 的默认值 hardware_concurrency())
// 注意: 这个参数只在你想覆盖线程池默认构造行为时才需要手动在线程池构造函数中处理
// 对于当前的 thread_pool 设计，它是通过构造函数参数传递的，
// 而单例模式通常在 get_instance() 首次调用时固定。
// 如果你想测试不同线程数的池，你需要修改线程池的 get_instance 或者提供一个 reinit(size_t) 方法。
// 或者，简单地，如果线程池的构造函数可以被单例访问，那么 get_instance() 第一次被调用时会使用默认值。
// 这里我们假设线程池使用其默认的线程数。
// static constexpr size_t THREAD_POOL_WORKER_COUNT = 4; // 例如，如果想固定线程池大小

// --- CPU 密集型任务 ---
// 返回一个值以防止编译器过度优化掉计算
long long cpu_intensive_task(int id, int complexity) {
    long long result = 0;
    // std::cout << "Task " << id << " starting on thread " << std::this_thread::get_id() << std::endl;
    for (int i = 0; i < complexity; ++i) {
        result += (long long)i * id - (long long)(i - 1) * (id - 1) + (long long)i * 5;
        result %= 1000000007; // 保持在范围内，增加一些操作
    }
    // std::cout << "Task " << id << " finished on thread " << std::this_thread::get_id() << std::endl;
    return result;
}

int main() {
    std::cout << "Benchmark Configuration:" << std::endl;
    std::cout << "------------------------" << std::endl;
    std::cout << "Number of tasks: " << NUM_TASKS << std::endl;
    std::cout << "Task complexity (iterations): " << TASK_COMPLEXITY << std::endl;
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << std::endl;
    std::cout << "------------------------" << std::endl << std::endl;

    std::vector<long long> results_pool(NUM_TASKS);
    std::vector<long long> results_jthread(NUM_TASKS);


    // --- 1. 测试使用线程池 ---
    std::cout << "Running test WITH Thread Pool..." << std::endl;
    auto start_pool = std::chrono::high_resolution_clock::now();
    {
        auto& pool = thread_pool_v1::thread_pool::get_instance();
        std::vector<std::future<long long>> futures;
        futures.reserve(NUM_TASKS);

        for (int i = 0; i < NUM_TASKS; ++i) {
            futures.emplace_back(pool.commit(cpu_intensive_task, i, TASK_COMPLEXITY));
        }

        for (int i = 0; i < NUM_TASKS; ++i) {
            results_pool[i] = futures[i].get(); // 等待任务完成并获取结果
        }
        // 线程池会在程序结束时由单例的析构函数自动清理并停止线程。
        // 或者可以显式调用 pool.stop(); 如果你想在特定点停止。
        // pool.stop(); // 如果你的 stop() 不是幂等的，或者你希望测试后立即停止
    } // pool 引用超出作用域，但单例依然存在直到程序结束
    auto end_pool = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_pool = end_pool - start_pool;
    std::cout << "Thread Pool test finished." << std::endl;


    // --- 2. 测试不使用线程池 (使用 std::jthread) ---
    // 为了确保公平比较，如果线程池被停止了，我们需要确保它在单例模式下可以被“重新启动”
    // 或者干脆不显式停止它，让它在程序结束时自然停止。
    // 对于这个 benchmark，假设单例在第一次 get_instance 后一直存活。
    // 如果 pool.stop() 被调用且其析构函数未运行，则线程池可能处于停止状态。
    // 在这里，我们不显式调用 stop()，让单例的生命周期管理它。

    std::cout << "\nRunning test WITHOUT Thread Pool (using std::jthread)..." << std::endl;
    auto start_jthread = std::chrono::high_resolution_clock::now();
    {
        std::vector<std::jthread> jthreads;
        jthreads.reserve(NUM_TASKS);
        // 注意：如果 NUM_TASKS 非常大，一次性创建这么多线程可能会耗尽系统资源
        // 线程池的优势之一就是限制并发线程数。
        // 为了更公平地比较，这里也应该限制并发线程数，或者理解这种比较的含义。
        // 典型的“不使用线程池”场景是为每个任务创建一个新线程。

        // 为了存储 std::jthread 的结果，我们需要 std::packaged_task 和 std::future
        std::vector<std::future<long long>> jthread_futures;
        jthread_futures.reserve(NUM_TASKS);

        for (int i = 0; i < NUM_TASKS; ++i) {
            // 使用 packaged_task 来获取 future
            std::packaged_task<long long(int, int)> task(cpu_intensive_task);
            jthread_futures.emplace_back(task.get_future());
            jthreads.emplace_back(std::move(task), i, TASK_COMPLEXITY);
        }

        for (int i = 0; i < NUM_TASKS; ++i) {
            results_jthread[i] = jthread_futures[i].get();
        }
        // std::jthread 会在析构时自动 join
    } // jthreads vector goes out of scope, jthreads are joined.
    auto end_jthread = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration_jthread = end_jthread - start_jthread;
    std::cout << "std::jthread test finished." << std::endl;


    // --- 3. 输出结果 ---
    std::cout << "\n--- Benchmark Results ---" << std::endl;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Time with Thread Pool:    " << duration_pool.count() << " ms" << std::endl;
    std::cout << "Time with std::jthread:   " << duration_jthread.count() << " ms" << std::endl;

    // 可选：验证结果是否一致（如果任务是确定性的）
    bool results_match = true;
    for(size_t i=0; i<NUM_TASKS; ++i) {
        if (results_pool[i] != results_jthread[i]) {
            results_match = false;
            std::cerr << "Mismatch at index " << i << ": pool=" << results_pool[i] << ", jthread=" << results_jthread[i] << std::endl;
            break;
        }
    }
    if (results_match) {
        std::cout << "Results from both methods match." << std::endl;
    } else {
        std::cout << "WARNING: Results from methods DO NOT match!" << std::endl;
    }

    // 确保单例线程池在程序结束前被正确关闭和清理
    // 如果没有显式调用 stop()，单例的析构函数应处理此问题
    // 如果之前调用了 pool.stop()，这里可以不用再调用
    // thread_pool_v1::thread_pool::get_instance().stop(); // 可选，取决于你的单例管理和stop的幂等性

    return 0;
}