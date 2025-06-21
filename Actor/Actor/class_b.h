//
// Created by ghost-him on 25-6-21.
//

#ifndef CLASS_B_H
#define CLASS_B_H
#include <iostream>
#include <string>

#include "actor.h"

struct msg_class_b
{
    std::string name;
    friend std::ostream& operator<<(std::ostream& os, const msg_class_b& msg)
    {
        os << msg.name;
        return os;
    }
};

class class_b : public actor<class_b, msg_class_b>
{
    friend class actor<class_b, msg_class_b>;
public:
    ~class_b()
    {
        m_stop = true;
        m_queue.notify_stop();
        m_thread.join();
        std::cout << "class b destruct" << std::endl;
    }

    void deal_message(std::shared_ptr<msg_class_b> data)
    {
        // 同理，这里也是对消息的处理，这里也还是以显示消息的内容为主
        std::cout << *data << std::endl;

        // 如果还需要再使用class c来处理，也可以像class a传递给class b一样将这个消息传递给class c
        // 这里就不写重复的代码了
    }

private:
    class_b()
    {
        m_thread = std::thread([this]()
        {
            // 只要还没有停，就一直循环
            for (;(m_stop.load() == false);)
            {
                // 阻塞等待消息的弹出
                std::shared_ptr<msg_class_b> data = m_queue.wait_and_pop();
                if (data == nullptr)
                {
                    continue;
                }

                deal_message(data);
            }
            // 退出了循环说明该退出了
            std::cout << "class a thread exit" << std::endl;
        });
    }
};


#endif //CLASS_B_H
