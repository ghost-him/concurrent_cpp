//
// Created by ghost-him on 25-5-25.
//

#ifndef concurrent_queue_v2_H
#define concurrent_queue_v2_H
#include <atomic>
#include <memory>
#include <bitset>



template<typename T, size_t Cap>
class concurrent_queue_v2 :private std::allocator<T> {
public:
    concurrent_queue_v2() : m_max_size(Cap + 1), m_head(0), m_tail(0),m_data(std::allocator<T>::allocate(m_max_size)), m_flag(new std::atomic<bool>[m_max_size]) {
        for (size_t i = 0; i < m_max_size; i ++) {
            m_flag[i].store(false, std::memory_order_relaxed);
        }
    }
    concurrent_queue_v2(const concurrent_queue_v2&) = delete;
    concurrent_queue_v2& operator=(const concurrent_queue_v2&) volatile = delete;
    concurrent_queue_v2& operator=(const concurrent_queue_v2&) = delete;

    ~concurrent_queue_v2() {
        while (m_head != m_tail) {
            std::destroy_at(m_data + m_head);
            m_head = (m_head + 1) % m_max_size;
        }
        std::allocator<T>::deallocate(m_data, m_max_size);
        delete[] m_flag;
    }

    bool is_empty() const {
        const auto head = m_head.load(std::memory_order_relaxed);
        const auto tail = m_tail.load(std::memory_order_acquire);
        return head == tail;
    }

    bool is_full() const {
        const auto head = m_head.load(std::memory_order_relaxed);
        const auto tail = m_tail.load(std::memory_order_acquire);
        return ((tail + 1) % m_max_size) == head;
    }

    bool push(const T& data) {
        return emplace(data);
    }

    bool push(const T&& data) {
        return emplace(std::move(data));
    }

    bool pop(T & ret_data) {
        size_t current_head;
        while (true) {
            current_head = m_head.load(std::memory_order_relaxed);
            size_t tail = m_tail.load(std::memory_order_acquire);

            if (current_head == tail) {
                return false;
            }

            if (!m_flag[current_head].load(std::memory_order_acquire)) {
                return false;
            }

            if (m_head.compare_exchange_strong(current_head, (current_head + 1) % m_max_size, std::memory_order_release, std::memory_order_relaxed)) {
                ret_data = std::move(m_data[current_head]);
                std::destroy_at(m_data + current_head);
                m_flag[current_head].store(false, std::memory_order_release);
                return true;
            }
        }
    }

private:
    template<typename ... Args>
    bool emplace(Args &&... args)
    requires (noexcept(std::construct_at(std::declval<T*>(), std::declval<Args>()...))) {
        size_t current_tail;
        while (true) {
            current_tail = m_tail.load(std::memory_order_relaxed);
            size_t head = m_head.load(std::memory_order_acquire);
            // 说明已经满了
            if ((current_tail + 1) % m_max_size == head) {
                return false;
            }

            if (m_tail.compare_exchange_strong(current_tail, (current_tail + 1) % m_max_size, std::memory_order_release, std::memory_order_relaxed)) {
                std::construct_at(m_data + current_tail, std::forward<Args>(args)...);
                m_flag[current_tail].store(true, std::memory_order_release);
                return true;
            }
        }
    }

    const size_t m_max_size;
    std::atomic<size_t> m_head, m_tail;
    T* m_data;
    std::atomic<bool>* m_flag;
};



#endif //concurrent_queue_v2_H
