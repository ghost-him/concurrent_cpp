//
// Created by ghost-him on 25-5-15.
//

#include "class_a.h"

int main ()
{
    msg_class_a msg;
    msg.name = "ghost-him";
    class_a::instance().post_message(msg);

    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "主线程退出!" << std::endl;
}


