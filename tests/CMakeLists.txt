include(FetchContent)

# Fetch gtest
# Automatically match gtest to project’s runtime (MSVC only)
if(MSVC)
    if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "Static")
        set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
    else()
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif()
endif()
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)    # Don't build GMock if not needed
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)   # Don't install
set(gtest_build_tests OFF CACHE BOOL "" FORCE) # Don't build GTest's own tests
set(gtest_build_samples OFF CACHE BOOL "" FORCE) # Don't build samples
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # <-- STATIC!
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/release-1.12.1.zip
)
FetchContent_MakeAvailable(googletest)

# Single executable for all tests
add_executable(tests 
    VecTree_tests.cpp 
    #ResourceRegistry_tests.cpp
    PoolAllocatorFH_tests.cpp
    PoolAllocatorTFH_tests.cpp
    Meta_tests.cpp)

target_link_libraries(tests PRIVATE gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)