#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include "tsdb/storage/storage_impl.h"
#include "tsdb/core/config.h"
#include "tsdb/core/types.h"

const int NUM_THREADS = 4;
const int WRITES_PER_THREAD = 100;

void writer_thread(tsdb::storage::StorageImpl* storage, int thread_id) {
    for (int i = 0; i < WRITES_PER_THREAD; ++i) {
        tsdb::core::Labels labels;
        labels.add("__name__", "hang_test_metric");
        labels.add("thread_id", std::to_string(thread_id));
        labels.add("write_id", std::to_string(i));

        tsdb::core::TimeSeries series(labels);
        series.add_sample(std::chrono::system_clock::now().time_since_epoch().count(), 1.0);

        auto result = storage->write(series);
        if (!result.ok()) {
            std::cerr << "Thread " << thread_id << " write failed: " << result.error().what() << std::endl;
        }
    }
}

int main() {
    std::cout << "=== DEBUG: Starting hang test for StorageImpl ===" << std::endl;
    
    try {
        tsdb::core::StorageConfig config;
        config.data_dir = "./debug_hang_test_data";
        
        auto storage = std::make_unique<tsdb::storage::StorageImpl>(config);
        
        auto init_result = storage->init(config);
        if (!init_result.ok()) {
            std::cerr << "FAILED: Storage initialization failed: " << init_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "SUCCESS: StorageImpl initialized" << std::endl;
        
        std::vector<std::thread> threads;
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(writer_thread, storage.get(), i);
        }

        for (auto& t : threads) {
            t.join();
        }
        std::cout << "SUCCESS: All writer threads completed." << std::endl;

        std::cout << "Step: Closing storage..." << std::endl;
        auto close_result = storage->close();
        if (!close_result.ok()) {
            std::cerr << "FAILED: Storage close failed: " << close_result.error().what() << std::endl;
            return 1;
        }
        std::cout << "SUCCESS: Storage closed" << std::endl;
        
        std::cout << "=== DEBUG: All steps completed successfully ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
