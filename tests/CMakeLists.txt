include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(aether_tests
    test_order_book.cpp
    test_slab_allocator.cpp
)

target_link_libraries(aether_tests PRIVATE GTest::gtest_main)
target_include_directories(aether_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(aether_tests)
