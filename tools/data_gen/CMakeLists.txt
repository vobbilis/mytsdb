add_executable(synthetic_cluster_generator main.cpp)

target_include_directories(synthetic_cluster_generator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/tsdb/proto/gen
)

target_link_libraries(synthetic_cluster_generator PRIVATE
    tsdb_lib
    gRPC::grpc++
    protobuf::libprotobuf
)
