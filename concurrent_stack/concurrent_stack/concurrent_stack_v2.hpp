//
// Created by ghost-him on 25-6-7.
//


#include <memory>
#include <set>
template<typename T>
class concurrent_stack_v2 {
private:
    struct node {
        std::shared_ptr<T> m_data;
        std::shared_ptr<node> m_next;
        explicit node(const T& data) : m_data(std::make_shared<T>(data)), m_next(nullptr) {}
    };

    concurrent_stack_v2(const concurrent_stack_v2&) = delete;
    concurrent_stack_v2& operator=(const concurrent_stack_v2&) = delete;

    std::atomic<std::shared_ptr<node>> m_head;
public:

    concurrent_stack_v2() = default;
    void push(const T& data) {
        std::shared_ptr<node> new_node = std::make_shared<node>(node(data));
        // 更新m_head的值
        // do {
        //     new_node->m_next = m_head.load();
        // } while (!m_head.compare_exchange_weak(new_node->m_next, new_node));
        // 这两个代码片段是等价的
        // compare_exchange_weak的作用为 1.比较并更新值 2.如果比较失败，则更新expected的值为m_head
        new_node->m_next = m_head.load();
        while (!m_head.compare_exchange_weak(new_node->m_next, new_node));
    }

    std::shared_ptr<T> pop() {
        std::shared_ptr<node> old_head = m_head.load();

        // compare_exchange_weak会自动的更新old_head的值
        while (old_head && !m_head.compare_exchange_weak(old_head, old_head->m_next));

        if (!old_head) {
            // old_head等于nullptr时，也会结束循环
            return nullptr;
        }
        return std::move(old_head->m_data);
    }
};
