# TSDB Test Suite

This directory contains the comprehensive test suite for the Time Series Database (TSDB) project.

## Test Organization

The test suite is organized into several components:

1. **Storage Engine Tests** (`storage/`)
   - Basic operations (CRUD)
   - Block management
   - Concurrency
   - Error handling
   - Resource management

2. **Histogram Tests** (`histogram/`)
   - Fixed-bucket histograms
   - Exponential histograms
   - Distribution accuracy
   - Concurrency
   - Memory efficiency

3. **OpenTelemetry Bridge Tests** (`otel/`)
   - Metric conversion
   - Attribute handling
   - Resource limits
   - Error handling
   - Concurrency

4. **Performance Tests** (`benchmark/`)
   - Write performance
   - Read performance
   - Compaction performance
   - Resource utilization

5. **Integration Tests** (`integration/`)
   - System integration
   - Operational tests

## Running Tests

### Running All Tests
```bash
cd build
ctest --output-on-failure
```

### Running Specific Test Categories
```bash
# Run storage tests
ctest -R "storage_.*"

# Run histogram tests
ctest -R "histogram_.*"

# Run OpenTelemetry tests
ctest -R "otel_.*"

# Run performance tests
ctest -R "benchmark_.*"

# Run integration tests
ctest -R "integration_.*"
```

### Running Tests with Coverage
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
ctest
gcovr -r .. .
```

## Test Plan Management

The test plan is maintained in `TESTPLAN.md` at the root of the project. To update test status:

```bash
# Interactive mode
./scripts/update_test_status.py

# Automatic mode (parses test output)
ctest --output-on-failure | tee test_output.txt
./scripts/update_test_status.py --auto
```

## Test Categories

### Unit Tests
- Test individual components in isolation
- Mock dependencies where necessary
- Focus on edge cases and error conditions
- Fast execution time

### Integration Tests
- Test component interactions
- Use real dependencies
- Focus on common use cases
- May be slower to execute

### Performance Tests
- Measure throughput and latency
- Test under various loads
- Monitor resource usage
- Run in isolation

### Stress Tests
- Test system limits
- Run under resource constraints
- Test recovery mechanisms
- Long-running tests

## Writing Tests

### Guidelines
1. Each test should have a clear purpose
2. Test both success and failure cases
3. Include edge cases and boundary conditions
4. Mock external dependencies appropriately
5. Clean up resources after tests
6. Document test assumptions and requirements

### Test Structure
```cpp
TEST_F(TestFixture, TestName) {
    // ARRANGE - Set up test conditions
    
    // ACT - Perform the operation being tested
    
    // ASSERT - Verify the results
}
```

### Adding New Tests
1. Create test file in appropriate directory
2. Add test to CMakeLists.txt
3. Update TESTPLAN.md
4. Add any necessary test data or fixtures
5. Document test purpose and requirements

## Test Data

Test data is stored in `test/data/` and includes:
- Sample metrics
- Histogram distributions
- OpenTelemetry protobuf messages
- Configuration files

## Debugging Tests

### Common Issues
1. Resource cleanup failures
2. Race conditions in concurrent tests
3. System-dependent behavior
4. Performance test variability

### Debugging Tools
1. Use `--gtest_break_on_failure`
2. Enable verbose logging
3. Use sanitizers (ASAN, TSAN)
4. Profile with perf or gprof

## Test Coverage

Coverage reports are generated for:
- Line coverage
- Branch coverage
- Function coverage

Target coverage metrics:
- Line coverage: >90%
- Branch coverage: >85%
- Function coverage: >95%

## Contributing

When contributing new tests:
1. Follow existing test patterns
2. Include both positive and negative tests
3. Document test purpose and requirements
4. Update TESTPLAN.md
5. Verify coverage impact

## Notes

- Tests should be deterministic
- Clean up resources even if test fails
- Use appropriate timeouts for async tests
- Document any system requirements
- Keep performance tests isolated 