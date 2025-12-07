#!/bin/bash
#
# K8s Benchmark Suite
# 
# Uses k8s_data_generator for realistic K8s data writes, then runs read benchmarks.
# Collects all results in CSV format for charting.
#
# Benchmark Flow:
#   1. Write baseline: Generate K8s data using k8s_data_generator (Arrow Flight)
#   2. Read baseline: Run Grafana-style dashboard queries
#   3. Output: CSV file with throughput and latency metrics

set -e

# Configuration
TSDB_DIR="${TSDB_DIR:-/Users/vobbilis/go/src/github.com/vobbilis/codegen/mytsdb}"
K8S_GEN="${TSDB_DIR}/build/tools/k8s_data_generator"
OUTPUT_DIR="${TSDB_DIR}/benchmark_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="${OUTPUT_DIR}/k8s_benchmark_${TIMESTAMP}.csv"
LOG_FILE="${OUTPUT_DIR}/k8s_benchmark_${TIMESTAMP}.log"

# Server settings
FLIGHT_HOST="${FLIGHT_HOST:-localhost}"
FLIGHT_PORT="${FLIGHT_PORT:-8815}"
HTTP_ADDR="${HTTP_ADDR:-localhost:9090}"

# Create output directory
mkdir -p "$OUTPUT_DIR"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

# CSV header
echo "timestamp,preset,operation,duration_sec,total_samples,throughput_samples_sec,p50_ms,p99_ms,max_ms,status" > "$RESULTS_FILE"

# Write result to CSV
write_csv() {
    local preset="$1"
    local operation="$2"
    local duration="$3"
    local samples="$4"
    local throughput="$5"
    local p50="$6"
    local p99="$7"
    local max="$8"
    local status="$9"
    
    echo "$(date +%Y-%m-%d_%H:%M:%S),$preset,$operation,$duration,$samples,$throughput,$p50,$p99,$max,$status" >> "$RESULTS_FILE"
}

# Run write benchmark using k8s_data_generator
run_write_benchmark() {
    local preset="$1"
    
    log "=========================================="
    log "WRITE BENCHMARK: $preset"
    log "=========================================="
    
    local start_time=$(date +%s.%N)
    
    # Run k8s_data_generator
    log "Starting k8s_data_generator with --$preset..."
    local output
    output=$("$K8S_GEN" --"$preset" --host "$FLIGHT_HOST" --port "$FLIGHT_PORT" 2>&1)
    
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    
    echo "$output" >> "$LOG_FILE"
    
    # Extract samples count from output
    local total_samples=$(echo "$output" | grep -o "[0-9,]* samples" | head -1 | tr -d ',' | awk '{print $1}')
    total_samples=${total_samples:-0}
    
    # Calculate throughput
    local throughput=0
    if [ "$duration" != "0" ] && [ -n "$duration" ]; then
        throughput=$(echo "scale=0; $total_samples / $duration" | bc 2>/dev/null || echo "0")
    fi
    
    # Check if success
    local status="success"
    if echo "$output" | grep -qi "error\|failed\|exception"; then
        status="error"
    fi
    
    log "Write completed: ${total_samples} samples in ${duration}s = ${throughput} samples/sec"
    
    write_csv "$preset" "write" "$duration" "$total_samples" "$throughput" "N/A" "N/A" "N/A" "$status"
    
    echo "$total_samples"
}

# Run read benchmark with various queries
run_read_benchmark() {
    local preset="$1"
    local num_queries="${2:-100}"
    
    log "=========================================="
    log "READ BENCHMARK: $preset (${num_queries} queries)"
    log "=========================================="
    
    # Sample queries representing Grafana dashboards
    local queries=(
        # Hot queries (1h range)
        "sum(rate(container_cpu_usage_seconds_total[5m])) by (namespace)"
        "sum(container_memory_working_set_bytes) by (namespace)"
        "count(kube_pod_status_phase) by (namespace, phase)"
        
        # Mixed queries (6h range)
        "avg(rate(container_cpu_usage_seconds_total[5m])) by (region)"
        "sum(rate(container_network_receive_bytes_total[5m])) by (cluster)"
        
        # Cold queries (24h range)
        "avg_over_time(container_memory_working_set_bytes[24h])"
    )
    
    local total_queries=0
    local total_latency=0
    local latencies=()
    local start_time=$(date +%s.%N)
    
    for i in $(seq 1 $num_queries); do
        local query="${queries[$((i % ${#queries[@]}))]}"
        local query_start=$(date +%s.%N)
        
        # Run query
        local result=$(curl -s -o /dev/null -w "%{http_code},%{time_total}" \
            "http://${HTTP_ADDR}/api/v1/query?query=$(echo "$query" | sed 's/ /%20/g')" 2>/dev/null)
        
        local query_end=$(date +%s.%N)
        local http_code=$(echo "$result" | cut -d',' -f1)
        local latency_sec=$(echo "$result" | cut -d',' -f2)
        local latency_ms=$(echo "$latency_sec * 1000" | bc)
        
        if [ "$http_code" = "200" ]; then
            total_queries=$((total_queries + 1))
            latencies+=("$latency_ms")
        fi
        
        # Progress every 10 queries
        if [ $((i % 20)) -eq 0 ]; then
            log "Completed $i/$num_queries queries..."
        fi
    done
    
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    
    # Calculate statistics
    local throughput=0
    if [ "$duration" != "0" ] && [ -n "$duration" ]; then
        throughput=$(echo "scale=2; $total_queries / $duration" | bc 2>/dev/null || echo "0")
    fi
    
    # Calculate percentiles (sort latencies)
    local p50="N/A"
    local p99="N/A"
    local max="N/A"
    
    if [ ${#latencies[@]} -gt 0 ]; then
        # Sort latencies
        IFS=$'\n' sorted=($(sort -n <<<"${latencies[*]}")); unset IFS
        
        local count=${#sorted[@]}
        local p50_idx=$((count * 50 / 100))
        local p99_idx=$((count * 99 / 100))
        
        p50="${sorted[$p50_idx]}"
        p99="${sorted[$p99_idx]}"
        max="${sorted[$((count-1))]}"
    fi
    
    log "Read completed: ${total_queries} queries in ${duration}s = ${throughput} queries/sec"
    log "Latency p50: ${p50}ms, p99: ${p99}ms, max: ${max}ms"
    
    write_csv "$preset" "read" "$duration" "$total_queries" "$throughput" "$p50" "$p99" "$max" "success"
}

# Check server connectivity
check_servers() {
    log "Checking server connectivity..."
    
    # Check Arrow Flight
    if ! nc -z $FLIGHT_HOST $FLIGHT_PORT 2>/dev/null; then
        log "ERROR: Cannot connect to Arrow Flight at ${FLIGHT_HOST}:${FLIGHT_PORT}"
        log "Please start the TSDB server with: ./build/src/tsdb_server"
        return 1
    fi
    log "Arrow Flight: OK (${FLIGHT_HOST}:${FLIGHT_PORT})"
    
    # Check HTTP/PromQL
    if curl -s "http://${HTTP_ADDR}/api/v1/status/buildinfo" > /dev/null 2>&1; then
        log "HTTP/PromQL: OK (${HTTP_ADDR})"
    else
        log "WARNING: HTTP/PromQL at ${HTTP_ADDR} may not be fully ready"
    fi
    
    return 0
}

# Print summary
print_summary() {
    log ""
    log "=========================================="
    log "BENCHMARK SUMMARY"
    log "=========================================="
    log ""
    
    echo ""
    echo "| Preset | Operation | Duration | Samples/Queries | Throughput | p99 |"
    echo "|--------|-----------|----------|-----------------|------------|-----|"
    
    tail -n +2 "$RESULTS_FILE" | while IFS=, read -r ts preset op dur samples tp p50 p99 max status; do
        printf "| %-6s | %-9s | %8s | %15s | %10s | %s |\n" "$preset" "$op" "${dur}s" "$samples" "$tp" "$p99"
    done
    
    echo ""
    log "Results saved to: $RESULTS_FILE"
    log "Log saved to: $LOG_FILE"
}

# Usage
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Run K8s realistic benchmark suite"
    echo ""
    echo "Options:"
    echo "  --preset PRESET    Scale preset: quick, small, medium, large (default: quick)"
    echo "  --queries N        Number of read queries to run (default: 100)"
    echo "  --write-only       Only run write benchmark"
    echo "  --read-only        Only run read benchmark"
    echo "  --help             Show this help"
    echo ""
    echo "Example:"
    echo "  $0 --preset quick --queries 50"
}

# Main
main() {
    local preset="quick"
    local num_queries=100
    local write_only=false
    local read_only=false
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --preset)
                preset="$2"
                shift 2
                ;;
            --queries)
                num_queries="$2"
                shift 2
                ;;
            --write-only)
                write_only=true
                shift
                ;;
            --read-only)
                read_only=true
                shift
                ;;
            --help)
                usage
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done
    
    log "=========================================="
    log "K8s Realistic Benchmark Suite"
    log "=========================================="
    log "Preset: $preset"
    log "Queries: $num_queries"
    log "Output: $RESULTS_FILE"
    log ""
    
    if ! check_servers; then
        exit 1
    fi
    
    log ""
    
    # Run benchmarks
    if [ "$read_only" = false ]; then
        run_write_benchmark "$preset"
        log ""
        sleep 2  # Allow data to settle
    fi
    
    if [ "$write_only" = false ]; then
        run_read_benchmark "$preset" "$num_queries"
    fi
    
    print_summary
}

main "$@"
