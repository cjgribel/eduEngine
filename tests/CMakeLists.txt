include(FetchContent)

# Automatically fetch gtest
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/release-1.12.1.zip
)

FetchContent_MakeAvailable(googletest)

# Single executable for all tests
add_executable(tests 
    VecTree_tests.cpp 
    ResourceRegistry_tests.cpp
    FreelistPool_tests.cpp)

target_link_libraries(tests PRIVATE gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)