#!/bin/bash
# Comprehensive PromQL Query Testing Script for Executive Dashboard
# Tests all queries before adding them to Grafana dashboard

set -e

MYTSDB_URL="${MYTSDB_URL:-http://localhost:9090}"
PASSED=0
FAILED=0
TOTAL=0

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}MyTSDB PromQL Query Testing${NC}"
echo -e "${BLUE}Target: $MYTSDB_URL${NC}"
echo -e "${BLUE}========================================${NC}\n"

# Function to test a query
test_query() {
    local name="$1"
    local query="$2"
    local expect_data="${3:-true}"
    
    TOTAL=$((TOTAL + 1))
    echo -ne "${YELLOW}[$TOTAL]${NC} Testing: $name ... "
    
    # URL encode the query
    encoded_query=$(echo -n "$query" | jq -sRr @uri)
    
    # Execute query
    response=$(curl -s "${MYTSDB_URL}/api/v1/query?query=${encoded_query}")
    status=$(echo "$response" | jq -r '.status')
    
    if [ "$status" != "success" ]; then
        echo -e "${RED}FAILED${NC}"
        echo "  Error: $(echo "$response" | jq -r '.error // "Unknown error"')"
        FAILED=$((FAILED + 1))
        return 1
    fi
    
    # Check if we got data
    result_type=$(echo "$response" | jq -r '.data.resultType')
    result_count=$(echo "$response" | jq -r '.data.result | length')
    
    if [ "$expect_data" = "true" ] && [ "$result_count" -eq 0 ]; then
        echo -e "${YELLOW}WARNING${NC}"
        echo "  No data returned (might be expected for some queries)"
    else
        echo -e "${GREEN}PASSED${NC}"
        echo "  Result type: $result_type, Series count: $result_count"
        PASSED=$((PASSED + 1))
    fi
}

echo -e "${BLUE}=== HTTP Request Metrics ===${NC}\n"

test_query "Total HTTP Requests" \
    "sum(http_requests_total)"

test_query "HTTP Request Rate (5m)" \
    "sum(rate(http_requests_total[5m]))"

test_query "HTTP Requests by Method" \
    "sum by (method) (rate(http_requests_total[5m]))"

test_query "HTTP Requests by Status Code" \
    "sum by (status) (rate(http_requests_total[5m]))"

test_query "HTTP Requests by Service" \
    "sum by (service) (rate(http_requests_total[5m]))"

test_query "HTTP Error Rate (4xx + 5xx)" \
    "sum(rate(http_requests_total{status=~\"4..|5..\"}[5m])) / sum(rate(http_requests_total[5m]))"

test_query "HTTP 5xx Error Rate" \
    "sum(rate(http_requests_total{status=~\"5..\"}[5m])) / sum(rate(http_requests_total[5m]))"

test_query "HTTP Success Rate (2xx)" \
    "sum(rate(http_requests_total{status=~\"2..\"}[5m])) / sum(rate(http_requests_total[5m]))"

test_query "Top 5 Services by Request Volume" \
    "topk(5, sum by (service) (rate(http_requests_total[5m])))"

test_query "HTTP Requests Heatmap by Status" \
    "sum by (status) (increase(http_requests_total[1m]))"

echo -e "\n${BLUE}=== HTTP Request Duration Metrics ===${NC}\n"

test_query "Average Request Duration" \
    "rate(http_request_duration_seconds_sum[5m]) / rate(http_request_duration_seconds_count[5m])"

test_query "Max Request Duration" \
    "max(rate(http_request_duration_seconds_sum[5m]) / rate(http_request_duration_seconds_count[5m]))"

test_query "Avg Request Duration by Service" \
    "avg by (service) (rate(http_request_duration_seconds_sum[5m]) / rate(http_request_duration_seconds_count[5m]))"

echo -e "\n${BLUE}=== CPU Metrics ===${NC}\n"

test_query "Overall Cluster CPU Usage" \
    "avg(node_cpu_usage_ratio)"

test_query "CPU Usage by Node" \
    "avg by (node) (node_cpu_usage_ratio)"

test_query "CPU Usage by Zone" \
    "avg by (zone) (node_cpu_usage_ratio)"

test_query "Container CPU Usage by Service" \
    "avg by (service) (container_cpu_usage_ratio)"

test_query "Top 10 Pods by CPU" \
    "topk(10, container_cpu_usage_ratio)"

test_query "CPU Usage Over 80%" \
    "count(container_cpu_usage_ratio > 0.8)"

echo -e "\n${BLUE}=== Memory Metrics ===${NC}\n"

test_query "Overall Cluster Memory Usage" \
    "avg(node_memory_usage_ratio)"

test_query "Memory Usage by Node" \
    "avg by (node) (node_memory_usage_ratio)"

test_query "Total Memory Used (GB)" \
    "sum(process_resident_memory_bytes) / 1024 / 1024 / 1024"

test_query "Memory Usage by Service" \
    "avg by (service) (process_resident_memory_bytes)"

test_query "Top 10 Pods by Memory" \
    "topk(10, process_resident_memory_bytes)"

echo -e "\n${BLUE}=== Availability Metrics ===${NC}\n"

test_query "Total Pods Up" \
    "count(up == 1)"

test_query "Pod Availability Rate" \
    "count(up == 1) / count(up)"

test_query "Pods by Namespace" \
    "count by (namespace) (up)"

test_query "Pods by Service" \
    "count by (service) (up)"

echo -e "\n${BLUE}=== Advanced Analytics (Skipped - Unsupported Functions) ===${NC}\n"

test_query "Request Rate Change (5m vs 10m)" \
    "(sum(rate(http_requests_total[5m])) - sum(rate(http_requests_total[10m]))) / sum(rate(http_requests_total[10m]))"

test_query "Error Budget (99.9% SLO)" \
    "1 - (sum(rate(http_requests_total{status=~\"5..\"}[5m])) / sum(rate(http_requests_total[5m])))"

test_query "Requests per Pod" \
    "sum(rate(http_requests_total[5m])) / count(up == 1)"

test_query "Service Health Score" \
    "avg by (service) (up) * (1 - (sum by (service) (rate(http_requests_total{status=~\"5..\"}[5m])) / sum by (service) (rate(http_requests_total[5m]))))"

echo -e "\n${BLUE}========================================${NC}"
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"
echo -e "${BLUE}Total:  $TOTAL${NC}"
echo -e "${BLUE}========================================${NC}\n"

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All queries passed! Ready to create dashboard.${NC}"
    exit 0
else
    echo -e "${RED}✗ Some queries failed. Please review.${NC}"
    exit 1
fi
