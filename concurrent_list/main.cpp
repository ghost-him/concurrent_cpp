#include "concurrent_list.hpp"
#include <thread>
#include <iostream>

void MultiThreadPush()
{
    concurrent_list<int> thread_safe_list;

    std::thread t1([&]()
    {
            for (int i = 0; i < 20000; i++)
            {
                int mc(i);
                thread_safe_list.push_front(mc);
                std::cout << "push front " << i << " success" << std::endl;
            }
    });

    std::thread t2([&]()
    {
            for (int i = 20000; i < 40000; i++)
            {
                int mc(i);
                thread_safe_list.push_back(mc);
                std::cout << "push back " << i << " success" << std::endl;
            }
    });

    std::thread t3([&]()
    {
            for(int i = 0; i < 40000; )
            {
                bool rmv_res = thread_safe_list.remove_first([&](const int& mc)
                    {
                        return mc == i;
                    });

                if(!rmv_res)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                i++;
            }
    });

    t1.join();
    t2.join();
    t3.join();

    std::cout << "begin for each print...." << std::endl;
    thread_safe_list.for_each([](const int& mc)
        {
            std::cout << "for each print " << mc << std::endl;
        });
    std::cout << "end for each print...." << std::endl;
}
int main() {
    MultiThreadPush();
}