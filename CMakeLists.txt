cmake_minimum_required(VERSION 3.14)
project(EAE_Firmware CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

# Statically link the C++/GCC runtime into our own binaries so the
# built executables don't depend on matching shared libstdc++/libgcc
# being present on the target machine. This does not apply to
# googletest itself (fetched below), only to our own targets.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_link_options(-static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------
# Core embedded-style logic, built once and shared by the demo
# executable and the test binary.
# ---------------------------------------------------------------
add_library(cooling_core STATIC
    src/core/pid.cpp
    src/core/state_machine.cpp
    src/core/can_bus.cpp
    src/core/can_protocol.cpp
    src/core/cooling_controller.cpp
)
target_include_directories(cooling_core PUBLIC src)

# ---------------------------------------------------------------
# Demo executable
# ---------------------------------------------------------------
add_executable(firmware_demo src/main.cpp)
target_link_libraries(firmware_demo PRIVATE cooling_core)

# ---------------------------------------------------------------
# Tests (GTest fetched at configure time, not vendored in this
# repo, and built/linked statically -- nothing shipped).
# ---------------------------------------------------------------
option(EAE_FIRMWARE_BUILD_TESTS "Build the GTest unit tests" ON)

if(EAE_FIRMWARE_BUILD_TESTS)
    include(FetchContent)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    )
    # Match our runtime's CRT settings; harmless on non-MSVC toolchains.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(cooling_tests
        tests/test_pid.cpp
        tests/test_state_machine.cpp
        tests/test_can_bus.cpp
        tests/test_cooling_controller.cpp
    )
    target_link_libraries(cooling_tests PRIVATE cooling_core gtest_main)

    include(GoogleTest)
    gtest_discover_tests(cooling_tests)
endif()
