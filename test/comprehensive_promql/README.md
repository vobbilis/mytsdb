# Comprehensive PromQL Test Harness

This directory contains the comprehensive test harness for the PromQL engine.
It is designed to run against a large-scale synthetic dataset to verify correctness and performance.

## Prerequisites
1.  **Data Generation**: You must generate the test dataset first.
    ```bash
    # Build the generator
    make synthetic_cluster_generator
    
    # Start TSDB server (to receive data)
    ./build/src/tsdb/tsdb_server --address 127.0.0.1:9090 --data-dir data/comprehensive_test_data &
    
    # Run generator (e.g., for 10M samples)
    ./build/tools/data_gen/synthetic_cluster_generator --address 127.0.0.1:9090 --nodes 50 --pods 20 --hours 24
    
    # Stop server
    pkill tsdb_server
    ```

2.  **Build Harness**:
    ```bash
    make comprehensive_promql_test_harness
    ```

## Running Tests
The harness loads the data from `data/comprehensive_test_data` directly (embedded storage).
Ensure `tsdb_server` is NOT running on the same data directory to avoid lock contention.

```bash
./build/test/comprehensive_promql/comprehensive_promql_test_harness
```

## Test Structure
- `test_harness.cpp`: Main entry point and test definitions.
- `basic_tests.json`: (Future) JSON-based test cases.
