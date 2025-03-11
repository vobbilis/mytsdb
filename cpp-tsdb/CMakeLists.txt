cmake_minimum_required(VERSION 3.20)
project(cpp-tsdb VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)

# Find required packages
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(TBB REQUIRED)

# Add subdirectories
add_subdirectory(src)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(test/benchmark)
endif()

# Installation rules
install(DIRECTORY include/tsdb
        DESTINATION include
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp") 