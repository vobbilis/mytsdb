#include <benchmark/benchmark.h>
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <memory>

// Simple benchmark to verify Arrow/Parquet linkage and basic object creation
static void BM_ArrowMemoryPool(benchmark::State& state) {
    for (auto _ : state) {
        arrow::MemoryPool* pool = arrow::default_memory_pool();
        benchmark::DoNotOptimize(pool);
    }
}
BENCHMARK(BM_ArrowMemoryPool);

static void BM_ArrowSchemaCreation(benchmark::State& state) {
    for (auto _ : state) {
        auto field = arrow::field("value", arrow::float64());
        auto schema = arrow::schema({field});
        benchmark::DoNotOptimize(schema);
    }
}
BENCHMARK(BM_ArrowSchemaCreation);

BENCHMARK_MAIN();
