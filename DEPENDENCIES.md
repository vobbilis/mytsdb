# External Dependencies

## Required Dependencies

### spdlog
- **Version**: >= 1.9.0
- **Purpose**: Fast C++ logging library
- **Installation**:
  ```bash
  # Using vcpkg
  vcpkg install spdlog
  
  # Using apt (Ubuntu/Debian)
  apt-get install libspdlog-dev
  
  # Using brew (macOS)
  brew install spdlog
  ```
- **CMake Integration**:
  ```cmake
  find_package(spdlog REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
  ```

### Google Test
- **Version**: >= 1.11.0
- **Purpose**: Testing framework
- **Installation**:
  ```bash
  # Using vcpkg
  vcpkg install gtest
  
  # Using apt (Ubuntu/Debian)
  apt-get install libgtest-dev
  
  # Using brew (macOS)
  brew install googletest
  ```
- **CMake Integration**:
  ```cmake
  find_package(GTest REQUIRED)
  target_link_libraries(${PROJECT_NAME}_test PRIVATE GTest::GTest GTest::Main)
  ```

### Threads
- **Version**: System provided
- **Purpose**: Threading support
- **CMake Integration**:
  ```cmake
  find_package(Threads REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
  ```

## Optional Dependencies

### OpenTelemetry C++ SDK
- **Version**: >= 1.0.0
- **Purpose**: OpenTelemetry integration
- **Installation**: Vendored as git submodule
- **CMake Integration**:
  ```cmake
  if(ENABLE_OTEL)
    add_subdirectory(third_party/opentelemetry-cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE opentelemetry-cpp)
  endif()
  ```

## Version Requirements
All version requirements are enforced through CMake's find_package command with appropriate version constraints.

## Dependency Management
We use the following methods for dependency management:
1. System packages for widely available libraries (spdlog, GTest)
2. Vendored dependencies for specific versions or modifications
3. CMake's find_package for version enforcement
4. Optional features can be disabled if dependencies are not available

## Build Requirements
- CMake >= 3.15
- C++17 compliant compiler
- AVX-512 support (optional, enables SIMD optimizations) 