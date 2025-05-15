//
// Created by ghost-him on 25-5-15.
//

#include "thread_pool.h"

namespace thread_pool_v1 {
    void thread_pool::stop() {
        m_stop = true;
        for (auto& jthread: m_jthreads) {
            jthread.request_stop();
        }
        m_cond.notify_all();
    }

    thread_pool::thread_pool(size_t thread_num) {
        if (thread_num <= 0) {
            thread_num = 1;
        }
        // 启动指定个数的线程
        for (int i = 0; i < thread_num; i++) {
            m_jthreads.emplace_back([this](std::stop_token stop_token) {
                // 只要还不需要暂停时，就一直处理这些工作
                while (!stop_token.stop_requested()) {
                    runtime_task task;
                    {
                        std::unique_lock<std::mutex> guard(m_queue_lock);
                        m_cond.wait(guard, [this, &stop_token]() {
                            return stop_token.stop_requested() || !m_task_queue.empty();
                        });
                        if (stop_token.stop_requested()) {
                            return ;
                        }
                        task = std::move(m_task_queue.front());
                        m_task_queue.pop();
                    }

                    task();
                }
            });
        }

    }

    thread_pool::~thread_pool() {
        // 回收线程
        stop();
    }
} // thread_pool_v1