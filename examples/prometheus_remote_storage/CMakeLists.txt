cmake_minimum_required(VERSION 3.15)

# Prometheus Remote Storage example server
message(STATUS "Checking dependencies for Prometheus Remote Storage example:")
message(STATUS "  HAVE_HTTPLIB: ${HAVE_HTTPLIB}")
message(STATUS "  HAVE_RAPIDJSON: ${HAVE_RAPIDJSON}")
message(STATUS "  HAVE_PROTOBUF: ${HAVE_PROTOBUF}")

if(HAVE_HTTPLIB AND HAVE_RAPIDJSON AND HAVE_PROTOBUF)
    add_executable(prometheus_remote_storage_server
        prometheus_remote_storage_server.cpp
    )
    
    target_link_libraries(prometheus_remote_storage_server
        PRIVATE
            tsdb::lib
            tsdb::prometheus
    )
    
    target_compile_features(prometheus_remote_storage_server PRIVATE cxx_std_17)
    
    message(STATUS "Prometheus Remote Storage example enabled")
else()
    message(STATUS "Prometheus Remote Storage example disabled (missing httplib, RapidJSON, or Protobuf)")
endif()
