cmake_minimum_required(VERSION 3.22)
project(khcomp VERSION 1.0.0 LANGUAGES CXX)

# Standard C++23 Enforcement
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option for enabling Sanitizers (ASAN / UBSAN) during debug builds
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Core Static Library Target
file(GLOB CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp")

add_library(khcomp_core STATIC ${CORE_SOURCES})

target_include_directories(khcomp_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(khcomp_core
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(khcomp_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(khcomp_core PUBLIC -fsanitize=address,undefined)
    endif()
endif()

# Enable Testing Infrastructure
enable_testing()

# Unit Test Executable Target
add_executable(test_compression tests/cpp/test_compression.cpp)

target_link_libraries(test_compression
    PRIVATE
        khcomp_core
)

# Output test executable to tests/cpp/ directory within the binary tree
set_target_properties(test_compression PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/cpp"
)

add_test(NAME test_compression COMMAND test_compression)