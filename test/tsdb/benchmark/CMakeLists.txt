find_package(benchmark REQUIRED)

add_executable(tsdb_benchmark benchmark.cpp)

target_link_libraries(tsdb_benchmark
    PRIVATE
        tsdb
        benchmark::benchmark
)

add_test(NAME tsdb_benchmark COMMAND tsdb_benchmark --benchmark_min_time=1) 