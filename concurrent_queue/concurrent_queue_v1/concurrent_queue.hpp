#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>

/// 这个代码的讲解可以去看并发栈v1版本。这两个的设计思路基本一样，所以这个代码就不写注释了

template<typename T>
class concurrent_queue_v1 {
private:
    mutable std::mutex m_mutex;
    std::queue<T> m_data;
    std::condition_variable m_cv;

public:
    concurrent_queue_v1() {}

    void push(T new_value) {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_data.push(std::move(new_value));
        m_cv.notify_one();
    }

    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> guard(m_mutex);
        m_cv.wait(guard, [this] {return !m_data.empty();});
        value = std::move(m_data.front());
        m_data.pop();
    }

    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> guard(m_mutex);
        m_cv.wait(guard, [this]{return !m_data.empty();});
        std::shared_ptr<T> result(std::make_shared<T>(std::move(m_data.front())));
        m_data.pop();
        return result;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_data.empty()) {
            return false;
        }
        value = std::move(m_data.front());
        m_data.pop();
        return true;
    }

    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_data.empty()) {
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> result(std::make_shared<T>(std::move(m_data.front())));
        m_data.pop();
        return result;
    }

    bool empty() const {
        std::lock_guard<std::mutex> guard(m_mutex);
        return m_data.empty();
    }
};

/// 在wait_and_pop时，在取数据时，可能会出现异常，从而导致没取出来。此时不会执行m_data.pop()，这会导致有一个元素没有被消费掉
/// 也有可能在移动数据时，因为T是自己写的数据类型，这就有可能移动本身出现问题，这也会导致代码出现了问题。
/// 同时，在构造智能指针时，也可能会出现问题
/// 为了代码的robust，这里可以通过智能指针的方式来优化

template<typename T>
class concurrent_queue_v2 {
private:
    mutable std::mutex m_mutex;
    std::queue<std::shared_ptr<T>> m_data;
    std::condition_variable m_cv;
public:
    concurrent_queue_v2() {}
    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> guard(m_mutex);
        m_cv.wait(guard, [this] {return !m_data.empty();});
        // 这个可以解决内存的问题（T本身已经在外面被定义好了），如果是移动本身出现了问题，则需要在通过逻辑来解决
        value = std::move(*m_data.front());
        m_data.pop();
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_data.empty()) {
            return false;
        }
        value = std::move(*m_data.front());
        m_data.pop();
        return true;
    }

    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> guard(m_mutex);
        m_cv.wait(guard, [this]{return !m_data.empty();});
        // 这里使用的是栈上的空间，而“内存耗尽”一般是指堆上的空间，所以这个代码也没问题
        // 为什么上面的代码会有问题？因为在构造智能指针时，存在malloc，在malloc的过程中就有可能会出现问题
        std::shared_ptr<T> result = m_data.front();
        m_data.pop();
        return result;
    }

    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_data.empty()) {
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> result = m_data.front();
        m_data.pop();
        return result;
    }

    void push(T new_value) {
        std::shared_ptr<T> data(std::make_shared<T>(std::move(new_value)));
        std::lock_guard<std::mutex> guard(m_mutex);
        m_data.push(data);
        m_cv.notify_one();
    }
    bool empty() const {
        std::lock_guard<std::mutex> guard(m_mutex);
        return m_data.empty();
    }
};


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

    node* get_tail() {
        std::lock_guard<std::mutex> guard(m_tail_mutex);
        return m_tail;
    }

    std::unique_ptr<node> pop_head() {
        std::unique_ptr<node> old_head  = std::move(m_head);
        m_head = std::move(old_head->next);
        return old_head;
    }
    std::unique_lock<std::mutex> wait_for_data() {
        std::unique_lock<std::mutex> head_lock(m_head_mutex);
        m_cv.wait(head_lock, [this] { return m_head.get() != get_tail();});
        return head_lock;
    }

    std::unique_ptr<node> wait_pop_head() {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        return pop_head();
    }

    std::unique_ptr<node> wait_pop_head(T& value) {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
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
    concurrent_queue_v3(): m_head(new node), m_tail(m_head.get()) {}
    concurrent_queue_v3(const concurrent_queue_v3& other) = delete;
    concurrent_queue_v3& operator=(const concurrent_queue_v3& other) = delete;

    std::shared_ptr<T> wait_and_pop() {
        const std::unique_ptr<node> old_head = wait_pop_head();
        return old_head->data;
    }

    void wait_and_pop(T& value) {
        const std::unique_ptr<node> old_head = wait_pop_head(value);
    }

    std::shared_ptr<T> try_pop() {
        std::unique_ptr<node> old_head = try_pop_head();
        return old_head? old_head->data: std::shared_ptr<T>();
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
            const node* new_tail = p.get();
            m_tail->next = std::move(p);
            m_tail = new_tail;
        }
        m_cv.notify_one();
    }
};
