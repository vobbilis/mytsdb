#!/bin/bash
set -e

# Run tests with coverage
go test -race -coverprofile=coverage.out -covermode=atomic ./...

# Generate coverage report
go tool cover -html=coverage.out -o coverage.html

# Display coverage statistics
go tool cover -func=coverage.out

# Clean up
rm coverage.out 