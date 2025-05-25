//
// Created by ghost-him on 25-5-15.
//

#ifndef ENABLE_SINGLETON_H
#define ENABLE_SINGLETON_H

namespace thread_pool_v1 {

    template <typename T>
    class enable_singleton {
    public:
        static T& get_instance() {
            static T instance;
            return instance;
        }
    protected:
        enable_singleton() = default;
        ~enable_singleton() = default;
        enable_singleton(const enable_singleton &) = delete;
        enable_singleton& operator=(const enable_singleton &) = delete;
        enable_singleton(const enable_singleton &&) = delete;
        enable_singleton& operator=(const enable_singleton &&) = delete;

    };

} // thread_pool_v1

#endif //ENABLE_SINGLETON_H
