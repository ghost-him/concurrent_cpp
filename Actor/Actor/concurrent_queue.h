//
// Created by ghost-him on 25-6-21.
//

#ifndef CONCURRENT_QUEUE_H
#define CONCURRENT_QUEUE_H


#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>

/// 由于上述的版本是采用标准库来实现的，而这会导致使用同一把锁来管理push与pop，而队列本身是可以把这两个操作用两个锁来管理的。
/// 所以就有了v3版本
template<typename T>
class concurrent_queue_v3 {
private:
    // 队列可以看成是由一个一个结点组成的，而m_data表示当前结点存的值，而next表示下一个结点的地址
    struct node {
        std::shared_ptr<T> m_data;
        std::unique_ptr<node> m_next;
    };
    std::mutex m_head_mutex;
    // 头指针指向当前数据的位置
    std::unique_ptr<node> m_head;
    std::mutex m_tail_mutex;
    // 尾指针指向下一个数据应该插入的地址
    // 因此，如果头与尾指向了同一个地址，则说明当前为空
    node* m_tail;
    std::condition_variable m_cv;

    std::atomic<bool> m_stop;

    node* get_tail() {
        std::lock_guard<std::mutex> guard(m_tail_mutex);
        return m_tail;
    }

    std::unique_ptr<node> pop_head() {
        std::unique_ptr<node> old_head  = std::move(m_head);
        m_head = std::move(old_head->m_next);
        return old_head;
    }
    std::unique_lock<std::mutex> wait_for_data() {
        std::unique_lock<std::mutex> head_lock(m_head_mutex);
        m_cv.wait(head_lock, [this] { return (m_stop.load() == true) || (m_head.get() != get_tail());});
        return head_lock;
    }

    std::unique_ptr<node> wait_pop_head() {
        std::unique_lock<std::mutex> head_lock(wait_for_data());

        if (m_stop.load())
        {
            return nullptr;
        }

        return pop_head();
    }

    std::unique_ptr<node> wait_pop_head(T& value) {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        if (m_stop.load())
        {
            return nullptr;
        }
        value = std::move(*m_head->data);
        return pop_head();
    }
    std::unique_ptr<node> try_pop_head() {
        std::lock_guard<std::mutex> head_lock(m_head_mutex);
        if (m_head.get() == get_tail()) {
            return std::unique_ptr<node>();
        }
        return pop_head();
    }
    std::unique_ptr<node> try_pop_head(T& value) {
        std::lock_guard<std::mutex> head_lock(m_head_mutex);
        if (m_head.get() == get_tail()) {
            return std::unique_ptr<node>();
        }
        value = std::move(*m_head->data);
        return pop_head();
    }
public:
    concurrent_queue_v3(): m_head(new node), m_tail(m_head.get()), m_stop(false) {}
    concurrent_queue_v3(const concurrent_queue_v3& other) = delete;
    concurrent_queue_v3& operator=(const concurrent_queue_v3& other) = delete;

    void notify_stop()
    {
        m_stop.store(true);
        m_cv.notify_one();
    }

    std::shared_ptr<T> wait_and_pop() {
        const std::unique_ptr<node> old_head = wait_pop_head();
        if (old_head == nullptr)
        {
            return nullptr;
        }
        return old_head->m_data;
    }

    void wait_and_pop(T& value) {
        const std::unique_ptr<node> old_head = wait_pop_head(value);
    }

    std::shared_ptr<T> try_pop() {
        std::unique_ptr<node> old_head = try_pop_head();
        return old_head? old_head->m_data: std::shared_ptr<T>();
    }

    bool try_pop(T& value) {
        const std::unique_ptr<node> old_head = try_pop_head(value);
        return old_head;
    }

    bool empty() {
        std::lock_guard<std::mutex> head_lock(m_head_mutex);
        return (m_head.get() == get_tail());
    }

    // 存数据时，先创建一个节点，然后把数据放到这个节点中，然后将尾指针的next设置为当前的节点
    // 最后更新尾指针设置为这个新的节点的地址。此时，这个新的节点就是队列的新的尾
    void push(T new_value) {
        std::shared_ptr<T> new_data (std::make_shared<T>(std::move(new_value)));
        std::unique_ptr<node> p(new node);
        {
            std::lock_guard<std::mutex> tail_lock(m_tail_mutex);
            m_tail->m_data = new_data;
            node* const new_tail = p.get();
            m_tail->m_next = std::move(p);
            m_tail = new_tail;
        }
        m_cv.notify_one();
    }
};

#endif //CONCURRENT_QUEUE_H
