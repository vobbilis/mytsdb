if(HAVE_GRPC AND ENABLE_OTEL)
    add_subdirectory(data_gen)
    
    add_executable(benchmark_tool benchmark_tool.cpp)
    target_include_directories(benchmark_tool PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
    )
    target_link_libraries(benchmark_tool PRIVATE
        tsdb_otel_impl
        gRPC::grpc++
        protobuf::libprotobuf
        Threads::Threads
    )
    if(HAVE_HTTPLIB)
        target_link_libraries(benchmark_tool PRIVATE httplib::httplib)
    endif()
else()
    message(STATUS "gRPC or OTEL not found/enabled, skipping data_gen and benchmark_tool")
endif()

# Arrow Flight benchmark tools (no gRPC/OTEL dependency)
if(HAVE_ARROW_FLIGHT)
    add_executable(arrow_benchmark arrow_benchmark.cpp)
    target_include_directories(arrow_benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${Arrow_INCLUDE_DIRS}
    )
    target_link_libraries(arrow_benchmark PRIVATE
        Arrow::arrow_shared
        ArrowFlight::arrow_flight_shared
        Threads::Threads
    )
    
    add_executable(k8s_data_generator k8s_data_generator.cpp)
    target_include_directories(k8s_data_generator PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${Arrow_INCLUDE_DIRS}
    )
    target_link_libraries(k8s_data_generator PRIVATE
        Arrow::arrow_shared
        ArrowFlight::arrow_flight_shared
        Threads::Threads
    )
    
    # K8s Combined Write+Read Benchmark (uses Arrow Flight only)
    add_executable(k8s_combined_benchmark k8s_combined_benchmark.cpp)
    target_include_directories(k8s_combined_benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${Arrow_INCLUDE_DIRS}
    )
    target_link_libraries(k8s_combined_benchmark PRIVATE
        Arrow::arrow_shared
        ArrowFlight::arrow_flight_shared
        Threads::Threads
    )
    if(HAVE_HTTPLIB)
        target_link_libraries(k8s_combined_benchmark PRIVATE httplib::httplib)
    endif()
    
    message(STATUS "Arrow Flight benchmark tools enabled")
endif()
