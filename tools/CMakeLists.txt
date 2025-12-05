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
