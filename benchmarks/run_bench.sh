#!/bin/bash
./build/tools/k8s_combined_benchmark --preset medium --write-workers 4 --read-workers 0 --duration 10 > bench_script.log 2>&1
