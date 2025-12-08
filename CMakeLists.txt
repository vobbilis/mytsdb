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
add_compile_definitions(TSDB_ENABLE_METRICS)

# Dependencies
include(GNUInstallDirs)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

## Required dependencies
find_package(Threads REQUIRED)

if(APPLE AND EXISTS "/opt/homebrew/opt/openssl@3")
    set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
endif()

find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

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

# Check for gRPC and Protobuf
find_package(gRPC QUIET)
if(NOT gRPC_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(GRPC grpc++)
        if(GRPC_FOUND)
            set(gRPC_FOUND TRUE)
            # Map PkgConfig variables to CMake variables expected by the project
            set(gRPC_INCLUDE_DIRS ${GRPC_INCLUDE_DIRS})
            set(gRPC_LIBRARIES ${GRPC_LIBRARIES})
            # Create imported target if it doesn't exist
            if(NOT TARGET gRPC::grpc++)
                add_library(gRPC::grpc++ INTERFACE IMPORTED)
                set_target_properties(gRPC::grpc++ PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${GRPC_INCLUDE_DIRS}"
                    INTERFACE_LINK_LIBRARIES "${GRPC_LIBRARIES}"
                )
            endif()
        endif()
    endif()
endif()

find_package(Protobuf QUIET)

if(DEFINED HAVE_GRPC)
    if(HAVE_GRPC)
        if(NOT gRPC_FOUND)
            message(FATAL_ERROR "HAVE_GRPC=ON but gRPC not found (tried find_package and PkgConfig)")
        endif()
        if(NOT Protobuf_FOUND)
            message(FATAL_ERROR "HAVE_GRPC=ON but Protobuf not found")
        endif()
        message(STATUS "gRPC support enabled explicitly")
    else()
        message(STATUS "gRPC support disabled explicitly")
    endif()
else()
    if(gRPC_FOUND AND Protobuf_FOUND)
        message(STATUS "Found gRPC and Protobuf, enabling RPC support")
        set(HAVE_GRPC TRUE)
    else()
        message(STATUS "gRPC or Protobuf not found, building without RPC support")
        set(HAVE_GRPC FALSE)
    endif()
endif()

if(Protobuf_FOUND)
    set(HAVE_PROTOBUF TRUE)
else()
    set(HAVE_PROTOBUF FALSE)
endif()

find_package(httplib QUIET)
if(NOT httplib_FOUND)
    if(EXISTS "/usr/include/httplib.h")
        set(HTTPLIB_INCLUDE_DIR "/usr/include")
    else()
        find_path(HTTPLIB_INCLUDE_DIR httplib.h)
    endif()

    if(HTTPLIB_INCLUDE_DIR)
        set(httplib_FOUND TRUE)
        add_library(httplib::httplib INTERFACE IMPORTED)
        set_target_properties(httplib::httplib PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${HTTPLIB_INCLUDE_DIR}"
        )
        message(STATUS "Found httplib header in ${HTTPLIB_INCLUDE_DIR}")
    endif()
endif()

if(httplib_FOUND)
    message(STATUS "Found httplib")
    set(HAVE_HTTPLIB TRUE)
else()
    message(STATUS "httplib NOT found")
    set(HAVE_HTTPLIB FALSE)
endif()

find_package(RapidJSON QUIET)
if(RapidJSON_FOUND)
    message(STATUS "Found RapidJSON")
    set(HAVE_RAPIDJSON TRUE)
else()
    message(STATUS "RapidJSON NOT found")
    set(HAVE_RAPIDJSON FALSE)
endif()

find_package(Snappy QUIET)
if(Snappy_FOUND)
    message(STATUS "Found Snappy")
    set(HAVE_SNAPPY TRUE)
else()
    message(STATUS "Snappy not found, remote storage will use uncompressed protobuf")
    set(HAVE_SNAPPY FALSE)
endif()

find_package(Arrow QUIET)
if(Arrow_FOUND)
    message(STATUS "Found Apache Arrow: ${Arrow_VERSION}")
    set(HAVE_ARROW TRUE)
    
    # Also check for Arrow Flight
    find_package(ArrowFlight QUIET)
    if(ArrowFlight_FOUND)
        message(STATUS "Found Apache Arrow Flight")
        set(HAVE_ARROW_FLIGHT TRUE)
    else()
        message(STATUS "Apache Arrow Flight not found")
        set(HAVE_ARROW_FLIGHT FALSE)
    endif()
else()
    message(STATUS "Apache Arrow not found")
    set(HAVE_ARROW FALSE)
    set(HAVE_ARROW_FLIGHT FALSE)
endif()

find_package(Parquet QUIET)
if(Parquet_FOUND)
    message(STATUS "Found Apache Parquet: ${Parquet_VERSION}")
    set(HAVE_PARQUET TRUE)
else()
    message(STATUS "Apache Parquet not found")
    set(HAVE_PARQUET FALSE)
endif()

## Test dependencies
if(BUILD_TESTS)
    find_package(GTest 1.11.0 QUIET)
    if(GTest_FOUND)
        include(GoogleTest)
        enable_testing()
        set(HAVE_GTEST TRUE)
        
        # Configure CTest custom settings
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CTestCustom.cmake.in
            ${CMAKE_BINARY_DIR}/CTestCustom.cmake
            @ONLY
        )
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
add_subdirectory(common)
add_subdirectory(src)
add_subdirectory(tools)

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