# OTEL/gRPC Test Directory
# This directory contains all tests related to OpenTelemetry and gRPC functionality

# Only build OTEL/gRPC tests if gRPC is available
if(HAVE_GRPC)
    # E2E Tests - Tests that start actual gRPC server
    add_subdirectory(e2e)
    
    # Integration Tests - Test OTEL bridge and gRPC service integration
    add_subdirectory(integration)
    
    # Unit Tests - Unit tests for OTEL/gRPC components
    add_subdirectory(unit)
    
    # Performance Tests - Performance benchmarks for OTEL/gRPC
    add_subdirectory(performance)
else()
    message(STATUS "gRPC not available - skipping OTEL/gRPC tests")
endif()

