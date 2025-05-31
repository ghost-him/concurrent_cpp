# pragma once

#include <exception>
#include <mutex>
#include <stack>
#include <condition_variable>

struct empty_stack: std::exception {
    const char* what() const throw();
};

template<typename T>
class concurrent_stack_v1 {
private:
    std::stack<T> m_data;
    // 这里需要使用mutable，因为在读这个栈的时候依然要加锁，而读本身为const的，在const函数中无法修改成员变量
    // 而加了mutable后，在const函数也可以实现加锁，从而实现线程安全
    mutable std::mutex m_mutex;
public:
    concurrent_stack_v1() {}

    concurrent_stack_v1(const concurrent_stack_v1& other) {
        std::lock_guard<std::mutex> guard(other.m_mutex);
        m_data = other.m_data;
    }

    concurrent_stack_v1& operator=(const concurrent_stack_v1&) = delete;

    void push(T new_value) {
        std::lock_guard<std::mutex> guard(m_mutex);
        // 当发生异常时，原来的数据不会受到影响，这里不会出现问题
        m_data.push(std::move(new_value));
    }

    // 两种pop的方式
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_data.empty()) throw empty_stack();
        const std::shared_ptr<T> result (std::make_shared<T>(std::move(m_data.top())));
        m_data.pop();
        return result;
    }

    void pop(T& value) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_data.empty()) throw empty_stack();
        value = std::move(m_data.top());
        m_data.pop();
    }

    bool empty() const {
        // 这里是在const函数中对mutex加锁，而m_mutex为mutable的，所以是可以正常加上锁的
        std::lock_guard<std::mutex> guard(m_mutex);
        return m_data.empty();
    }
};

/// 这个版本为v1版本的改进版，改进的点在于：
template<typename T>
class concurrent_stack_v2 {
private:
    std::stack<T> m_data;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv; // My_Condition_Variable

public:
    concurrent_stack_v2() {}

    concurrent_stack_v2(const concurrent_stack_v2& other) {
        std::lock_guard<std::mutex> guard(other.m_mutex);
        m_data = other.m_data;
    }

    concurrent_stack_v2& operator=(const concurrent_stack_v2&) = delete;

    void push(T new_value) {
        std::lock_guard<std::mutex> lock(m);
        m_data.push(std::move(new_value));
        m_cv.notify_one();
    }

    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> guard(m_mutex);
        // cv的操作：1.将线程挂起 2.解锁，让其他线程可以访问锁
        // cv可以与unique_lock配合使用
        m_cv.wait(guard, [this] {
            if (m_data.empty()) {
                return false;
            }
            return true;
        });
        // 此时，cv给guard重新上锁，guard持有锁，当前区域互斥访问
        // 这一行代码有可能会抛出异常，解决异常比较好的方法是，使用智能指针存储数据
        // 可能导致异常的原因：
        // 1.shared_ptr本身在创建新的对象时std::bad_alloc
        // 2.如果是自己定义的T类型，本身的移动构造函数可能会抛出异常
        const std::shared_ptr<T> result (std::make_shared<T>(std::move(m_data.top())));
        m_data.pop();
        return result;
    }

    void wait_and_pop(T & value) {
        std::unique_lock<std::mutex> guard(m_mutex);
        m_cv.wait(guard, [this]() {
            if (m_data.empty()) {
                return false;
            }
            return true;
        });

        value = std::move(m_data.top());
        m_data.pop();
    }

    bool empty() const {
        std::lock_guard<std::mutex> guard(m_mutex);
        return m_data.empty();
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_data.empty()) {
            return false;
        }

        value = std::move(m_data.top());
        m_data.pop();
        return true;
    }

    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (m_data.empty()) {
            return std::shared_ptr<T>();
        }

        std::shared_ptr<T> result(std::make_shared<T>(std::move(m_data.top())));
        m_data.pop();
        return result;
    }

};