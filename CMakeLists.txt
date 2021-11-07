cmake_minimum_required(VERSION 3.2)
project(TheadPoolExample)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(SOURCE_FILES
    example.cpp)

add_executable(thread_pool_example ${SOURCE_FILES})
target_link_libraries(thread_pool_example Threads::Threads)
include_directories(${CMAKE_SOURCE_DIR}/include)
