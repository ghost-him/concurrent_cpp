#pragma once
#include <memory>
#include <atomic>

template<typename T>
class concurrent_stack_v4 {
private:
    struct count_node;
    struct counted_node_ptr
    {
        // 外部引用的计数
        int m_external_count;
        // 结点的地址
        count_node* m_ptr;
    };

    struct count_node
    {
        // 数据域的智能指针
        std::shared_ptr<T> m_data;
        // 节点内部引用计数
        std::atomic<int> m_internal_count;
        // 下一个节点
        counted_node_ptr m_next;
        count_node(const T& data) : m_data(std::make_shared<T>(data)), m_internal_count(0){}
    };

    // 头部的结点
    std::atomic<counted_node_ptr> m_head;

public:
    concurrent_stack_v4()
    {
        // 这个只是用于做标识的
        counted_node_ptr head_node_ptr;
        head_node_ptr.m_external_count = 0;
        head_node_ptr.m_ptr = nullptr;
        m_head.store(head_node_ptr);
    }
    ~concurrent_stack_v4()
    {
        while (pop());
    }
    // 入栈
    void push(const T& data)
    {
        counted_node_ptr new_node;
        new_node.m_ptr = new count_node(data);
        new_node.m_external_count = 1;
        new_node.m_ptr->m_next = m_head.load();
        while (!m_head.compare_exchange_weak(new_node.m_ptr->m_next, new_node));
    }
    // 增加头结点引用的数量
    void increase_head_count(counted_node_ptr& old_counter)
    {
        counted_node_ptr new_counter;
        do
        {
            new_counter = old_counter;
            ++new_counter.m_external_count;
        } while (!m_head.compare_exchange_strong(old_counter, new_counter, std::memory_order_acquire, std::memory_order_relaxed));
        // 确保调用者得到的old_counter的external_count是递增后的值
        old_counter.m_external_count = new_counter.m_external_count;
    }

    std::shared_ptr<T> pop()
    {
        // 读取一下当前的头结点
        counted_node_ptr old_head = m_head.load();
        while (true)
        {
            // 先给头结点的引用数+1
            increase_head_count(old_head);
            // 获得它的数据
            count_node* const ptr = old_head.m_ptr;
            // 如果没有数据，则说明栈中已经没有数据了，而当前的这个是一开始被加入的标志位
            if (!ptr)
            {
                // 直接返回空的数据
                return std::shared_ptr<T>();
            }

            // 尝试更新head的值，即弹出一个数据
            if (m_head.compare_exchange_strong(old_head, ptr->m_next, std::memory_order_relaxed))
            {
                // 本线程如果抢先完成head的更新
                // 则取数据，弹数据
                std::shared_ptr<T> res;
                res.swap(ptr->m_data);
                // 为什么要减2,这个数字代表的是，当前有几个结点在引用这个结点
                // 所以减2代表着有两个结点不再引用这个结点了，分别是
                // 1.头结点：已经指向该结点的下一个结点了
                // 2.本线程：本线程的上一行代码要将该结点弹出来，所以引用了这个结点，所以该结点的引用数量+1,而现在已经不引用了，所以将其减1
                const int count_increase = old_head.m_external_count - 2;
                // 这个count_increase表示，除了头结点与该线程，还有几个线程访问了这个结点
                if (ptr->m_internal_count.fetch_add(count_increase, std::memory_order_release) == -count_increase)
                {
                    // 如果内部的计数加上了count_increase等于0,即返回的结果为-count_increase，则说明目前只有该线程在访问这个结点
                    // 所以由该线程负责删除
                    delete ptr;
                }
                return res;
            } else
            {
                // 如果当前的线程操作已经被其他的线程更新了，则减少内部的引用计数
                if (ptr->m_internal_count.fetch_add(-1, std::memory_order_acquire) == 1)
                {
                    // 如果当前的线程减少内部引用计数，返回的值为1，则说明该指针只被当前的线程引用了，则由该线程来负责释放
                    ptr->m_internal_count.load(std::memory_order_acquire);
                    delete ptr;
                }
            }


        }
    }
};
