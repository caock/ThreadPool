ThreadPool
==========

A simple C++11 Thread Pool implementation.

Basic usage:
```c++
// create thread pool with 4 worker threads
ThreadPool pool(4);

// enqueue and store future
auto result = pool.enqueue([](int answer) { return answer; }, 42);

// get result from future
std::cout << result.get() << std::endl;

```

advanced usage (bind thread to specific cpu core):
```c++
// create thread pool with 4 worker threads
ThreadPool pool(4);
int bind_core_id = 0;

// enqueue and store future
auto result = pool.strict_enqueue(bind_core_id, [](int answer) { return answer; }, 42);

// get result from future
std::cout << result.get() << std::endl;

```
more powerful examples in example.cpp file.
