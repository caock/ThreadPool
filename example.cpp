#include <iostream>
#include <vector>
#include <chrono>

#include "ThreadPool.h"

void CommonThreadPool(size_t n)
{
    ThreadPool pool(n);
    std::vector< std::future<int> > results;

    for(int i = 0; i < 8; ++i) {
        results.emplace_back(
            pool.enqueue([i] {
                std::cout << "hello " << i << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "world " << i << std::endl;
                
                return i*i;
            })
        );
    }

    // wait for results
    for(auto && result: results)
        if( result.valid() )
                result.wait();
    std::cout << "results:" << std::endl;
    for(auto && result: results)
        std::cout << result.get() << ' ';
    std::cout << std::endl;
}

void BindThreadPool(size_t n, std::vector<int> thread_ids)
{
    ThreadPool bind_thread(n);
    bind_thread.pool_bind_core(thread_ids);
    std::vector<std::future<int>> results(n);
    for(int i=0;i<8;i++)
    {
        int k = i % n;
        if( results[k].valid() )
                results[k].wait();
        
        results[k] = bind_thread.strict_enqueue(thread_ids[k], [i, k]{
            std::cout << k << " hello " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << k << " world " << i << std::endl;
            
            return i*i;
        });
    }
    
    // wait for results
    for(auto && result: results)
        if( result.valid() )
                result.wait();
    std::cout << "results:" << std::endl;
    for(auto && result: results)
        std::cout << result.get() << ' ';
    std::cout << std::endl;
}

int main()
{
    std::cout << "Example for CommonThreadPool:" << std::endl;
    CommonThreadPool(4);
    std::cout << std::endl;

    std::cout << "Example for BindThreadPool:" << std::endl;
    BindThreadPool(4, {0, 1, 2, 3});
    
    return 0;
}
