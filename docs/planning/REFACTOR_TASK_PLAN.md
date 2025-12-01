# Task Plan: Modern Monolith Refactor (Context-Aware)

**Status:** Draft v3 (With Code Examples)
**Goal:** Transform MyTSDB into a high-performance, self-monitoring, single-node "Modern Monolith" by modifying existing components.

## Phase 1: Foundation & Observability
*Goal: Upgrade existing instrumentation to be production-ready.*

- [ ] **Task 1.1: Prometheus Exporter**
    -   **File**: `src/tsdb/observability/prometheus_exporter.h` (New)
    -   **Action**: Create a registry for metrics.
    -   **Implementation Preview**:
        ```cpp
        // src/tsdb/observability/prometheus_exporter.h
        class PrometheusExporter {
        public:
            static PrometheusExporter& instance();
            void register_counter(const std::string& name, const std::string& help, std::atomic<double>& value);
            std::string scrape(); // Returns Prometheus-formatted metrics
        private:
            std::map<std::string, std::atomic<double>*> counters_;
        };
        ```

- [ ] **Task 1.2: Integrate Instrumentation**
    -   **File**: `include/tsdb/storage/write_performance_instrumentation.h`
    -   **Action**: Register internal counters with the exporter.
    -   **Implementation Preview**:
        ```cpp
        // In WritePerformanceInstrumentation constructor
        WritePerformanceInstrumentation() {
            PrometheusExporter::instance().register_counter(
                "mytsdb_wal_write_latency_us", "WAL write latency", wal_write_total_us_);
        }
        ```

## Phase 2: Hot Tier Optimization (Async WAL)
*Goal: Modify `WriteAheadLog` for non-blocking writes.*

- [ ] **Task 2.1: Background Flush Thread**
    -   **File**: `include/tsdb/storage/wal.h`
    -   **Action**: Add queue and thread.
    -   **Implementation Preview**:
        ```cpp
        class WriteAheadLog {
        private:
            // New members
            moodycamel::ConcurrentQueue<std::vector<uint8_t>> write_queue_;
            std::thread flush_thread_;
            std::atomic<bool> running_{true};

            void flush_loop() {
                std::vector<uint8_t> batch;
                while (running_) {
                    if (write_queue_.try_dequeue(batch)) {
                        current_file_.write(...);
                        // Group commit: flush every 10ms or 100 items
                    }
                }
            }
        };
        ```

## Phase 3: Cold Tier Integration (S3)
*Goal: Implement `S3BlockStorage` for the existing `BlockManager`.*

- [ ] **Task 3.1: S3BlockStorage Implementation**
    -   **File**: `src/tsdb/storage/s3_block_storage.h` (New)
    -   **Action**: Implement `BlockStorage` interface.
    -   **Implementation Preview**:
        ```cpp
        class S3BlockStorage : public BlockStorage {
        public:
            core::Result<void> write(const BlockHeader& header, const std::vector<uint8_t>& data) override {
                // Convert to Parquet (Task 3.2)
                auto parquet_data = parquet_converter_.to_parquet(data);
                
                // Upload to S3
                Aws::S3::Model::PutObjectRequest request;
                request.SetBucket(bucket_);
                request.SetKey(generate_key(header));
                s3_client_->PutObject(request);
                return core::Result<void>::success();
            }
        };
        ```

- [ ] **Task 3.3: Wire Up Cold Storage**
    -   **File**: `src/tsdb/storage/internal/block_manager.cpp`
    -   **Action**: Initialize `cold_storage_` with S3.
    -   **Implementation Preview**:
        ```cpp
        // BlockManager constructor
        if (config.s3_enabled) {
            cold_storage_ = std::make_unique<S3BlockStorage>(config.s3_config);
        } else {
            cold_storage_ = std::make_unique<FileSystemStorage>(data_dir + "/cold");
        }
        ```

## Phase 4: In-Process Multi-Tenancy
*Goal: Refactor `StorageImpl` to manage multiple `TenantContext`s.*

- [ ] **Task 4.1: Extract TenantContext**
    -   **File**: `include/tsdb/storage/storage_impl.h`
    -   **Action**: Define context struct.
    -   **Implementation Preview**:
        ```cpp
        struct TenantContext {
            std::unique_ptr<WriteAheadLog> wal;
            std::unique_ptr<ShardedIndex> index;
            std::unique_ptr<BlockManager> block_manager;
            std::unique_ptr<CacheHierarchy> cache;
        };
        ```

- [ ] **Task 4.2: Update StorageImpl**
    -   **File**: `src/tsdb/storage/storage_impl.cpp`
    -   **Action**: Manage tenants map.
    -   **Implementation Preview**:
        ```cpp
        class StorageImpl : public Storage {
        private:
            tbb::concurrent_hash_map<std::string, std::shared_ptr<TenantContext>> tenants_;

            std::shared_ptr<TenantContext> get_tenant(const std::string& tenant_id) {
                // Find or create logic
                // Init WAL with "data_dir/tenant_id/wal"
            }

        public:
            core::Result<void> write(const std::string& tenant_id, const TimeSeries& series) {
                return get_tenant(tenant_id)->wal->log(series);
            }
        };
        ```
