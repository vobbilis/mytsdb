cmake_minimum_required(VERSION 3.15)
project(tsdb VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Options
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)
option(ENABLE_SIMD "Enable SIMD optimizations" ON)
option(ENABLE_OTEL "Enable OpenTelemetry support" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
option(TSDB_SEMVEC "Enable semantic vector components (compile-time only)" OFF)

# Dependencies
include(GNUInstallDirs)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

## Required dependencies
find_package(Threads REQUIRED)

# Try to find optional dependencies
find_package(spdlog 1.9.0 QUIET)
if(spdlog_FOUND)
    message(STATUS "Found spdlog, enabling logging support")
    set(HAVE_SPDLOG TRUE)
else()
    message(STATUS "spdlog not found, building without logging support")
    set(HAVE_SPDLOG FALSE)
endif()

find_package(TBB 2021.0 QUIET)
if(TBB_FOUND)
    message(STATUS "Found TBB, enabling concurrent data structures")
    set(HAVE_TBB TRUE)
else()
    message(STATUS "TBB not found, building without concurrent data structures")
    set(HAVE_TBB FALSE)
endif()

find_package(gRPC 1.45.0 CONFIG QUIET)
find_package(Protobuf 3.19.0 QUIET)
if(gRPC_FOUND AND Protobuf_FOUND)
    message(STATUS "Found gRPC and Protobuf, enabling RPC support")
    set(HAVE_GRPC TRUE)
else()
    message(STATUS "gRPC or Protobuf not found, building without RPC support")
    set(HAVE_GRPC FALSE)
endif()

## Test dependencies
if(BUILD_TESTS)
    find_package(GTest 1.11.0 QUIET)
    if(GTest_FOUND)
        include(GoogleTest)
        enable_testing()
        set(HAVE_GTEST TRUE)
    else()
        message(STATUS "GTest not found, skipping tests")
        set(BUILD_TESTS OFF)
        set(HAVE_GTEST FALSE)
    endif()
endif()

## Optional dependencies
if(ENABLE_OTEL)
    find_package(opentelemetry-cpp 1.19.0 QUIET)
    if(opentelemetry-cpp_FOUND)
        set(HAVE_OTEL TRUE)
    else()
        message(STATUS "OpenTelemetry not found, disabling OTEL support")
        set(ENABLE_OTEL OFF)
        set(HAVE_OTEL FALSE)
    endif()
endif()

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    # Remove -Werror for now to allow build with warnings
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
    
    # Coverage flags
    if(ENABLE_COVERAGE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
    
    # Check for AVX-512 support
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512)
    if(COMPILER_SUPPORTS_AVX512)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f -mavx512dq -mavx512bw -mavx512vl")
    endif()
endif()

# Create config header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/tsdb/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/tsdb/config.h"
)

# Subdirectories
add_subdirectory(src)

# Tests
if(BUILD_TESTS AND HAVE_GTEST)
    add_subdirectory(test)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation (only if we have a target to install)
if(TARGET tsdb_lib)
    install(TARGETS tsdb_lib
        EXPORT tsdb-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY include/tsdb
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    install(EXPORT tsdb-targets
        FILE tsdb-targets.cmake
        NAMESPACE tsdb::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tsdb
    )

    # Generate and install package config
    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        cmake/tsdb-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/tsdb-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tsdb
        PATH_VARS
            CMAKE_INSTALL_LIBDIR
            CMAKE_INSTALL_INCLUDEDIR
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/tsdb-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/tsdb-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/tsdb-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tsdb
    )
endif()

# =============================================================================
# TOP-LEVEL TEST TARGETS
# =============================================================================

# Test targets are defined in test/CMakeLists.txt and available at top level
# when BUILD_TESTS=ON 