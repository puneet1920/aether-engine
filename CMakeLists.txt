cmake_minimum_required(VERSION 3.20)
project(aether-engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Low-latency compiler flags
if (MSVC)
    add_compile_options(/O2 /W4 /permissive-)
else ()
    add_compile_options(-O3 -march=native -Wall -Wextra -Wpedantic)
endif ()

include_directories(include)

add_executable(aether_engine src/main.cpp)
add_executable(aether_benchmark src/benchmark.cpp)

# Testing
option(BUILD_TESTING "Build the testing tree." ON)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
