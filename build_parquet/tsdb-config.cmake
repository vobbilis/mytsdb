
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was tsdb-config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

include(CMakeFindDependencyMacro)

# Find dependencies
find_dependency(Threads REQUIRED)
find_dependency(spdlog 1.9.0 REQUIRED)

# Optional dependencies
if(OFF)
    # OpenTelemetry is included as a submodule, so we need to ensure it's available
    if(NOT TARGET opentelemetry-cpp)
        message(FATAL_ERROR "OpenTelemetry targets not found. Please ensure the submodule is initialized.")
    endif()
endif()

# Include targets file
include("${CMAKE_CURRENT_LIST_DIR}/tsdb-targets.cmake")

# Component configuration
set(TSDB_VERSION 1.0.0)
set(TSDB_ENABLE_SIMD ON)
set(TSDB_ENABLE_OTEL OFF)

# Compiler features
set(TSDB_CXX_STANDARD 17)
set(TSDB_SUPPORTS_AVX512 1)

# Check if the implementation matches the client's requirements
check_required_components(tsdb)

# Convenience variables for include directories
set_and_check(TSDB_INCLUDE_DIR "${PACKAGE_PREFIX_DIR}/include")
set_and_check(TSDB_LIB_DIR "${PACKAGE_PREFIX_DIR}/lib") 
