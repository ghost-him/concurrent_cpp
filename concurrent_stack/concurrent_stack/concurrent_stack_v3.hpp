//
// Created by ghost-him on 25-6-7.
//
#pragma once

#include <atomic>
#include <memory>
#include <thread>

// 最大的风险指针的个数，这也表示并发数最大为100
const unsigned max_hazard_pointer = 100;

// 定义一个风险指针
// 表明 哪一个线程 在访问 哪一个地址。
struct hazard_pointer {
    std::atomic<std::thread::id> m_id;
    std::atomic<void*> m_pointer;
};

// 一个全局的，风险指针数组
hazard_pointer hazard_pointers[max_hazard_pointer];

// 用于对全局风险指针数组做管理
class hp_owner {
public:
    hp_owner(const hp_owner&) = delete;
    hp_owner& operator=(const hp_owner&) = delete;

    // 初始化时，会自动向全局的风险指针数据中申请一个风险指针
    hp_owner() : m_hp(nullptr) {
        bind_hazard_pointer();
    }

    // 析构时，会自己归还申请的风险指针，从而可以让其他线程使用
    ~hp_owner() {
        m_hp->m_pointer.store(nullptr);
        // id变成默认的
        m_hp->m_id.store(std::thread::id());
    }

    // 获取自己的风险指针
    std::atomic<void*>& get_pointer() {
        return m_hp->m_pointer;
    }

private:
    // 绑定一个风险指针
    void bind_hazard_pointer() {
        for (unsigned i = 0; i < max_hazard_pointer; i ++) {
            // 默认是空的id
            std::thread::id old_id;
            // 循环遍历所有的风险指针的槽，如果是空的，则可以与old_id匹配上，那么就将这个槽的线程id设置成当前线程的id
            if (hazard_pointers[i].m_id.compare_exchange_strong(old_id, std::this_thread::get_id())) {
                // 然后再绑定一下风险指针的地址
                m_hp = &hazard_pointers[i];
                // 这样就相当于在风险指针数组中注册了当前的线程
                break;
            }
        }

        // 如果没有找到可用的槽，则直接报错，说明超过了并发量
        // 给程序员提示，说明应该1.扩大风险指针数组以及提高可承担的并发量2.限制业务的并发量
        if (!m_hp) {
            throw std::runtime_error("没有找到空闲的风险指针");
        }
    }

    hazard_pointer* m_hp;
};

std::atomic<void*>& get_hazard_pointer_for_current_thread() {
    // 每个线程都有自己的风险指针
    // 当一个线程第一次调用此函数时，hp_owner 的构造函数会自动为其从全局池中申请一个风险指针槽。
    // 当线程退出时，hp_owner 的析构函数会自动释放该槽。
    thread_local static hp_owner hazard;
    return hazard.get_pointer();
}

template<typename T>
class concurrent_stack_v3 {
private:
    struct node {
        std::shared_ptr<T> m_data;
        node* m_next;
        node(const T& data) : m_data(std::make_shared<T>(data)) {}
    };

    // 待删结点
    struct data_to_reclaim {
        node* m_data;
        data_to_reclaim* m_next;
        data_to_reclaim(node* p): m_data(p), m_next(nullptr){}
        ~data_to_reclaim() {
            delete m_data;
        }
    };

    concurrent_stack_v3(const concurrent_stack_v3&) = delete;
    concurrent_stack_v3& operator=(const concurrent_stack_v3&) = delete;

    // 维护当前栈的数据
    std::atomic<node*> m_head;
    // 维护待删列表的数据
    std::atomic<data_to_reclaim*> m_nodes_to_reclaim;
public:
    concurrent_stack_v3() = default;

    // 这个部分的与v2版本一样，所以就不写重复的注释了
    void push(const T& data) {
        node* const new_node = new node(data);
        new_node->m_next = m_head.load();
        while (!m_head.compare_exchange_weak(new_node->m_next, new_node));
    }

    std::shared_ptr<T> pop() {
        // 获取当前线程绑定的风险指针
        std::atomic<void*>& hp = get_hazard_pointer_for_current_thread();
        node* old_head = m_head.load();
        do {
            node* temp;
            // 通过风险指针声明这个线程正在使用old_head这个指针
            do {
                temp = old_head;            // 声明前读一下当前的栈的第一个元素
                hp.store(old_head);
                old_head = m_head.load();   // 声明后再读一下第一个元素
            } while (old_head != temp);     // 判断声明前与声明后还是不是同一个元素
            // 循环来声明，因为可能在声明（即：向风险指针存储）的过程中，当前的结点已经被其他的线程声明了
            // 如果被声明了，则取一个新的结点，然后再尝试声明这个新的结点

            // 原子的取出当前的结点，如果成功，则逻辑上移除了结点
            // 如果失败，则说明在操作期间，另一个线程成功的 pop 或 push，此时循环的再弹出新的结点（与v2版本一样）
        } while (old_head && !m_head.compare_exchange_weak(old_head, old_head->m_next));

        // 如果已经成功的弹出来了，则可以撤销风险的声明
        // 风险指针的主要的作用是防止在读head和修改head之间，节点被其他的线程弹出并删除（因为这会导致old_head被置空。从而使得old_head->next的异常）。
        hp.store(nullptr);
        std::shared_ptr<T> result;
        // 也有可能是因为栈里无元素了，才返回的
        if (old_head) {
            result.swap(old_head->m_data);
            // 判断一下当前的这个结点是不是被其他的线程声明了
            if (outstanding_hazard_pointers_for(old_head)) {
                // 如果还有线程在用这个结点，则延迟删除
                reclaim_later(old_head);
            } else {
                // 如果已经没有线程在声明这个了，则说明当前线程是唯一一个执有这个结点的，则负责释放该结点
                delete old_head;
                // 会不会在执行完这个扫描后又有线程声明了？
                // 不会，因为在上文的代码中，已经将这个结点弹出原链表了，所以其他的结点不会再访问到这个结点了
            }
            // 执行完以后，再看看那些延迟删除的结点能不能删了，如果可以删，则删去
            delete_nodes_with_no_hazards();
        }
        return result;
    }

    //  遍历全局风险指针数组，检查是否有任何线程正在“声明”对指针 p 的风险。
    //  这是实现安全删除的核心检查。
    //  如果返回true，说明还有线程在使用这个结点
    //  如果返回false，说明已经没有线程在使用这个结点了
    bool outstanding_hazard_pointers_for(void *p) {
        for (unsigned i = 0; i < max_hazard_pointer; i ++) {
            if (hazard_pointers[i].m_pointer.load() == p) {
                return true;
            }
        }
        return false;
    }

    // 延迟删除
    void reclaim_later(node* old_node) {
        add_to_reclaim_list(new data_to_reclaim(old_node));
    }

    void add_to_reclaim_list(data_to_reclaim* reclaim_node) {
        reclaim_node->m_next = m_nodes_to_reclaim.load();
        while (!m_nodes_to_reclaim.compare_exchange_weak(reclaim_node->m_next, reclaim_node));
    }

    // 删除已经不再使用的结点
    void delete_nodes_with_no_hazards() {
        data_to_reclaim* current = m_nodes_to_reclaim.exchange(nullptr);
        while (current) {
            data_to_reclaim* const next = current->m_next;
            if (!outstanding_hazard_pointers_for(current->m_data)) {
                delete current;
            }
            else {
                add_to_reclaim_list(current);
            }
            current = next;
        }
    }
};