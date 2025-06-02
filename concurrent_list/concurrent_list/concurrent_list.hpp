#pragma once

#include <algorithm>
#include <mutex>
#include <memory>

template<typename T>
class concurrent_list {
    struct node {
        std::mutex m_mutex;
        std::shared_ptr<T> m_data;
        std::unique_ptr<node> m_next = nullptr;
        node() {}
        node(const T& value) : m_data(std::make_shared<T>(value)) {}
    };
    node m_head;

    // 尾指针，用于实现高效的尾插入
    node * m_last_node_ptr;
    // 互斥访问尾指针，防止尾插入出现并发问题
    std::mutex m_last_ptr_mutex;
public:
    concurrent_list() {
        m_last_node_ptr = &m_head;
    }
    ~concurrent_list() {
        // 始终返回true,所以会删除所有的结点
        remove_if([](const T&){return true;});
    }

    concurrent_list(const concurrent_list& other) = delete;
    concurrent_list& operator=(const concurrent_list& other) = delete;

    // 删除满足条件的所有的结点，其中，条件指的是p
    // 一般来说，p是一个lambda表达式
    template<typename Predicate>
    void remove_if(Predicate p) {
        // current指当前的结点
        node* current = &m_head;
        // guard也始终是锁当前的结点
        std::unique_lock<std::mutex> guard(m_head.m_mutex);
        // next指当前结点的下一个结点，而判断时，一直是判断下一个结点的情况
        while (node* const next = current->m_next.get()) {
            std::unique_lock<std::mutex> next_guard(next->m_mutex);
            if (p(*next->m_data)) {
                // 如果谓词是满足的，则说明要删除这个谓词
                std::unique_ptr<node> old_next = std::move(current->m_next);
                current->m_next = std::move(next->m_next);
                // 如果删除的这个结点是最后一个结点了，则需要更新尾指针
                if (current->m_next == nullptr) {
                    std::lock_guard<std::mutex> last_guard(m_last_ptr_mutex);
                    m_last_node_ptr = current;
                }
                next_guard.unlock();
            } else {
                guard.unlock();
                current = next;
                guard = std::move(next_guard);
            }
        }
    }

    template<typename Predicate>
    bool remove_first(Predicate p) {
        node* current = &m_head;
        std::unique_lock<std::mutex> guard(m_head.m_mutex);
        while (node* const next = current->m_next.get()) {
            std::unique_lock<std::mutex> next_guard(next->m_mutex);
            if (p(*next->m_data)) {
                std::unique_ptr<node> old_next = std::move(current->m_next);
                current->m_next = std::move(next->m_next);
                // 如果删除的已经是最后一个结点了
                if (current->m_next == nullptr) {
                    std::lock_guard<std::mutex> last_guard(m_last_ptr_mutex);
                    m_last_node_ptr = current;
                }
                next_guard.unlock();
                return true;
            }
            guard.unlock();
            current = next;
            guard = std::move(next_guard);
        }
        return false;
    }

    template<typename Predicate>
    std::shared_ptr<T> find_first_if(Predicate p) {
        node* current = &m_head;
        std::unique_lock<std::mutex> guard(m_head.m_mutex);
        while (node* const next = current->m_next.get()) {
            std::unique_lock<std::mutex> next_guard(next->m_mutex);
            // 结束对next中变量的访问，释放锁
            guard.unlock();
            if (p(*next->m_data)) {
                return next->m_data;
            }
            current = next;
            guard = std::move(next_guard);
        }
        // 如果没有找到结点，则返回空
        return std::shared_ptr<T>();
    }

    // 使用头插法，插入一个新的结点
    void push_front(const T& value) {
        std::unique_ptr<node> new_node(new node(value));
        std::lock_guard<std::mutex> guard(m_head.m_mutex);
        new_node->m_next = std::move(m_head.m_next);
        m_head.m_next = std::move(new_node);
        // 更新尾指针，如果插入的是第一个结点，则更新
        if (m_head.m_next->m_next == nullptr) {
            std::lock_guard<std::mutex> last_guard(m_last_ptr_mutex);
            m_last_node_ptr = m_head.m_next.get();
        }
    }

    // 使用尾插法，插入一个新的结点
    void push_back(const T& value) {
        std::unique_ptr<node> new_node(new node(value));
        // 这里要对这两个同时加锁，不然可能会与删除操作产生数据竞争
        std::lock(m_last_node_ptr->m_mutex, m_last_ptr_mutex);
        std::unique_lock<std::mutex> guard(m_last_node_ptr->m_mutex, std::adopt_lock);
        std::unique_lock<std::mutex> last_guard(m_last_ptr_mutex, std::adopt_lock);
        // 添加新的结点
        m_last_node_ptr->m_next = std::move(new_node);
        // 更新尾指针
        m_last_node_ptr = m_last_node_ptr->m_next.get();
    }



    // 遍历所有的结点，对每一个结点都使用f调用一下
    template<typename Function>
    void for_each(Function f) {
        node* current = &m_head;
        std::unique_lock<std::mutex> guard(m_head.m_mutex);
        while (node* const next = current->m_next.get()) {
            std::unique_lock<std::mutex> next_guard(next->m_mutex);
            guard.unlock();
            f(*next->m_data);
            current = next;
            guard = std::move(next_guard);
        }
    }


};