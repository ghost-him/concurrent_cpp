//
// Created by ghost-him on 25-6-21.
//

#ifndef ACTOR_H
#define ACTOR_H
#include <atomic>
#include <thread>

#include "concurrent_queue.h"

template<typename ClassType, typename MessageType>
class actor {
public:
    static ClassType& instance()
    {
        static ClassType instance;
        return instance;
    }

    ~actor()
    {

    }

    void post_message(const MessageType& data)
    {
        m_queue.push(data);
    }

protected:
    actor(): m_stop(false)
    {

    }

    actor(const actor&) = delete;
    actor(actor&&) = delete;
    actor& operator=(const actor&) = delete;

    std::atomic<bool> m_stop;
    std::thread m_thread;
    concurrent_queue_v3<MessageType> m_queue;
};




#endif //ACTOR_H
