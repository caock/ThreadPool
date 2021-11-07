// Copyright (c) 2012 Jakob Progsch, Václav Zeman
// Copyright (c) 2021 Chengkun Cao
// 
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
// 
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 
//    1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 
//    2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 
//    3. This notice may not be removed or altered from any source
//    distribution.
// 
#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <iostream>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
   //define something for Windows (32-bit and 64-bit, this part is common)
   #ifdef _WIN64
      //define something for Windows (64-bit only)
   #else
      //define something for Windows (32-bit only)
   #endif
#elif __APPLE__
    #include <TargetConditionals.h>
    #if TARGET_IPHONE_SIMULATOR
         // iOS Simulator
    #elif TARGET_OS_MACCATALYST
         // Mac's Catalyst (ports iOS API into Mac, like UIKit).
         #include "macos_cpu.h"
    #elif TARGET_OS_IPHONE
        // iOS device
    #elif TARGET_OS_MAC
        // Other kinds of Mac OS
        #include "macos_cpu.h"
    #else
    #   error "Unknown Apple platform"
    #endif
#elif __linux__
    // linux
#elif __unix__ // all unices not caught above
    // Unix
#elif defined(_POSIX_VERSION)
    // POSIX
#else
#   error "Unknown compiler"
#endif

class ThreadPool {
public:
    ThreadPool(size_t);
    ~ThreadPool();

    void pool_bind_core(std::vector<int> core_ids);

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    template<class F, class... Args>
    auto strict_enqueue(int core_id, F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    std::unordered_map<int, int> core_ids_to_worker_index_map;

    // the task queue
    std::queue< std::function<void()> > tasks;
    std::unordered_map<int, std::queue<std::function<void()>> > strict_tasks;
    
    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};
 
// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    : stop(false)
{
    for(size_t i = 0;i<threads;++i)
    {
        workers.emplace_back(
            [this, i]
            {
                for(;;)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this, i]{ return this->stop || !this->tasks.empty() || !this->strict_tasks[i].empty(); });
                        if(this->stop && this->tasks.empty() && this->strict_tasks[i].empty())
                            return;
                        if(!this->tasks.empty())
                        {
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                            //std::cout << "tasks" << std::endl;
                        }
                        else if(!this->strict_tasks[i].empty())
                        {
                            task = std::move(this->strict_tasks[i].front());
                            this->strict_tasks[i].pop();
                            //std::cout << "strict_tasks" << std::endl;
                        }
                        else
                        {
                            //std::cout << "none" << i << std::endl;
                            continue;
                        }
                    }

                    task();
                }
            }
        );
    }
}

inline void ThreadPool::pool_bind_core(std::vector<int> core_ids)
{
    if(core_ids.size() != this->workers.size())
    {
        auto msg = std::string("Wrong core_ids list size ") + std::to_string(core_ids.size());
        throw std::runtime_error(msg);
    }
    
    for(int i=0;i<workers.size();i++)
    {
        const int id = core_ids[i];
        core_ids_to_worker_index_map.emplace(id, i);
        strict_tasks.emplace(id, std::queue<std::function<void()>>());

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(id, &cpuset);
        int rc = pthread_setaffinity_np(workers[i].native_handle(), sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            auto msg = std::string("Error calling pthread_setaffinity_np: ") + std::to_string(rc) + "\n";
            throw std::runtime_error(msg);
        }
    }
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task](){ (*task)(); });
    }
    condition.notify_one();
    return res;
}

// add new work item to the pool, with strict bind core id
template<class F, class... Args>
auto ThreadPool::strict_enqueue(int core_id, F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    if(this->core_ids_to_worker_index_map.find(core_id) == this->core_ids_to_worker_index_map.end())
        throw std::runtime_error("enqueue core id not found!");
    int worker_id = core_ids_to_worker_index_map[core_id];

    auto task = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        strict_tasks[worker_id].emplace([task](){ (*task)(); });
    }
    condition.notify_all(); 
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers)
    {
        if(worker.joinable())
            worker.join();
    }
}
