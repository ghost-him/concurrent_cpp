//
// Created by ghost-him on 25-5-15.
//

#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <functional>
#include <future>

#include "enable_singleton.h"
#include <thread>
#include <queue>
#include <mutex>

namespace thread_pool_v1 {
    class thread_pool : public enable_singleton<thread_pool> {
    friend class enable_singleton<thread_pool>;
    public:
        using runtime_task = std::function<void()>;

        /// 向线程池提示一个任务
        template <typename F, typename... Args>
        auto commit(F&& function, Args&&... args) -> std::future<decltype(function(args...))> {
            if (m_stop.load()) {
                throw std::runtime_error("thread pool has been stoped");
            }
            // 获得这个任务的返回类型
            using return_type = decltype(function(args...));
            // 构造这个任务的对象
            auto pool_task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(function), std::forward<Args>(args)...));
            // 将任务的返回值与future绑定
            std::future<return_type> return_value = pool_task->get_future();
            {
                std::lock_guard<std::mutex> guard(m_queue_lock);
                // 向队列中添加
                m_task_queue.emplace([pool_task]() {
                    // 通过值传递获得这个任务
                    (*pool_task)();
                });
            }
            // 通知一个线程来运行这个任务
            m_cond.notify_one();
            return return_value;
        }

        void stop();

    private:
        thread_pool(size_t thread_num = std::thread::hardware_concurrency());
        ~thread_pool();
        /// 中止
        std::atomic<bool> m_stop = false;

        /// 任务队列
        std::queue<runtime_task> m_task_queue;
        std::mutex m_queue_lock;
        /// 用于通知线程运行任务的条件变量
        std::condition_variable m_cond;
        /// 维护所有的线程
        std::vector<std::jthread> m_jthreads;
    };

} // thread_pool_v1

#endif //THREAD_POOL_H
