//
// Created by ghost-him on 25-6-21.
//

#ifndef CLASSA_H
#define CLASSA_H
#include <iostream>
#include <string>

#include "actor.h"
#include "class_b.h"

// 如果要使用这个Actor时，要先定义自己所接受的消息类型

struct msg_class_a
{
    std::string name;
    friend std::ostream& operator<<(std::ostream& os, const msg_class_a& msg)
    {
        os << msg.name;
        return os;
    }
};

class class_a: public actor<class_a, msg_class_a>
{
    // 要将这个声明为友元，这个是CRTP技术的常见的写法
    // CRTP的介绍：https://ghost-him.com/posts/d8fa9844/
    friend class actor<class_a, msg_class_a>;
public:
    ~class_a()
    {
        m_stop = true;
        m_queue.notify_stop();
        m_thread.join();
        std::cout << "class_a destruct" << std::endl;
    }

    void deal_message(std::shared_ptr<msg_class_a> data)
    {
        // 这里就可以处理收到消息时的逻辑

        // 比如这里我就可以只对消息做一个输出的处理
        std::cout << "class a deal message is : " << *data << std::endl;

        // 如果在处理完当前的消息以后，还需要再传输给下一个类来进行处理，则这里也可以继续传递消息
        msg_class_b message_to_b;
        message_to_b.name = "我已经处理完消息a了，该给你处理消息b了";
        // 使用这个公共的接口将消息传递给消息b
        class_b::instance().post_message(message_to_b);
    }
private:
    class_a()
    {
        // 初始化actor中的并发线程
        // 在这里定义时，才能使用class a的deal message函数
        // 也可以在actor中定义一个虚函数，然后这个子类实现虚函数，这样也可以
        // 但是把这个处理消息的代码放到子类中可以有更多的灵活性
        m_thread = std::thread([this]()
        {
            // 只要还没有停，就一直循环
            for (;(m_stop.load() == false);)
            {
                // 阻塞等待消息的弹出
                std::shared_ptr<msg_class_a> data = m_queue.wait_and_pop();
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



#endif //CLASSA_H
