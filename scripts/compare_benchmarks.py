#!/usr/bin/env python3
"""
compare_benchmarks.py

Compares benchmark results before and after optimization phases.
Generates a report showing improvements in key metrics.

Usage:
    python scripts/compare_benchmarks.py baseline.log optimized.log
    python scripts/compare_benchmarks.py --dir benchmark_results/
"""

import argparse
import re
import sys
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple
from pathlib import Path


@dataclass
class BenchmarkMetrics:
    """Parsed benchmark metrics"""
    name: str
    p50_ms: Optional[float] = None
    p90_ms: Optional[float] = None
    p99_ms: Optional[float] = None
    max_ms: Optional[float] = None
    throughput_qps: Optional[float] = None
    count: Optional[int] = None


def parse_log_file(filepath: Path) -> Dict[str, BenchmarkMetrics]:
    """Parse a benchmark log file and extract metrics."""
    metrics = {}
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Pattern for latency lines: "P50: 60.40 ms"
    p50_pattern = re.compile(r'[Pp]50[:\s]+(\d+\.?\d*)\s*(?:ms|us|μs)')
    p90_pattern = re.compile(r'[Pp]90[:\s]+(\d+\.?\d*)\s*(?:ms|us|μs)')
    p99_pattern = re.compile(r'[Pp]99[:\s]+(\d+\.?\d*)\s*(?:ms|us|μs)')
    max_pattern = re.compile(r'[Mm]ax[:\s]+(\d+\.?\d*)\s*(?:ms|us|μs)')
    
    # Pattern for throughput
    qps_pattern = re.compile(r'[Tt]hroughput[:\s]+(\d+\.?\d*)\s*(?:qps|queries/sec|samples/sec)')
    
    # Pattern for test names
    test_pattern = re.compile(r'(?:TEST_F|BENCHMARK)\([^,]+,\s*(\w+)\)|(\w+):\s+Count:')
    
    current_test = "overall"
    
    for line in content.split('\n'):
        # Check for test name
        test_match = test_pattern.search(line)
        if test_match:
            current_test = test_match.group(1) or test_match.group(2) or "overall"
            if current_test not in metrics:
                metrics[current_test] = BenchmarkMetrics(name=current_test)
        
        # Extract metrics
        m = metrics.get(current_test, BenchmarkMetrics(name=current_test))
        
        p50_match = p50_pattern.search(line)
        if p50_match:
            m.p50_ms = float(p50_match.group(1))
        
        p90_match = p90_pattern.search(line)
        if p90_match:
            m.p90_ms = float(p90_match.group(1))
            
        p99_match = p99_pattern.search(line)
        if p99_match:
            m.p99_ms = float(p99_match.group(1))
            
        max_match = max_pattern.search(line)
        if max_match:
            m.max_ms = float(max_match.group(1))
            
        qps_match = qps_pattern.search(line)
        if qps_match:
            m.throughput_qps = float(qps_match.group(1))
        
        metrics[current_test] = m
    
    # Also try to extract from summary lines
    summary_patterns = [
        # "Latency p50: 60.40 ms"
        re.compile(r'Latency p50[:\s]+(\d+\.?\d*)\s*ms'),
        # "Read Latency p99: 3919.22 ms"
        re.compile(r'Read.*p99[:\s]+(\d+\.?\d*)\s*ms'),
    ]
    
    for line in content.split('\n'):
        for pattern in summary_patterns:
            match = pattern.search(line)
            if match and 'overall' in metrics:
                if 'p50' in line.lower():
                    metrics['overall'].p50_ms = float(match.group(1))
                elif 'p99' in line.lower():
                    metrics['overall'].p99_ms = float(match.group(1))
    
    return metrics


def calculate_improvement(baseline: Optional[float], optimized: Optional[float], 
                          lower_is_better: bool = True) -> Tuple[Optional[float], str]:
    """Calculate improvement percentage and format string."""
    if baseline is None or optimized is None:
        return None, "N/A"
    
    if baseline == 0:
        return None, "N/A (baseline=0)"
    
    if lower_is_better:
        improvement = ((baseline - optimized) / baseline) * 100
        speedup = baseline / optimized if optimized > 0 else float('inf')
    else:
        improvement = ((optimized - baseline) / baseline) * 100
        speedup = optimized / baseline if baseline > 0 else float('inf')
    
    if improvement > 0:
        color = '\033[92m'  # Green
        sign = '+'
    else:
        color = '\033[91m'  # Red
        sign = ''
    
    reset = '\033[0m'
    
    if abs(speedup) > 1.1:
        return improvement, f"{color}{sign}{improvement:.1f}% ({speedup:.1f}x){reset}"
    else:
        return improvement, f"{sign}{improvement:.1f}%"


def compare_metrics(baseline: Dict[str, BenchmarkMetrics], 
                   optimized: Dict[str, BenchmarkMetrics]) -> str:
    """Generate comparison report."""
    lines = []
    lines.append("=" * 80)
    lines.append("BENCHMARK COMPARISON REPORT")
    lines.append("=" * 80)
    lines.append("")
    
    # Overall summary table
    lines.append("LATENCY COMPARISON (lower is better)")
    lines.append("-" * 80)
    lines.append(f"{'Metric':<25} {'Baseline':<15} {'Optimized':<15} {'Improvement':<25}")
    lines.append("-" * 80)
    
    # Aggregate key metrics
    key_metrics = ['overall', 'SLA_Compliance', 'RangeQuery_1Hour_1MinStep', 
                   'InstantQuery_SimpleSelector', 'InstantQuery_RateFunction']
    
    for test_name in key_metrics:
        if test_name in baseline or test_name in optimized:
            b = baseline.get(test_name, BenchmarkMetrics(name=test_name))
            o = optimized.get(test_name, BenchmarkMetrics(name=test_name))
            
            # p50
            _, p50_imp = calculate_improvement(b.p50_ms, o.p50_ms, lower_is_better=True)
            lines.append(f"  {test_name[:23]:<23} P50: {b.p50_ms or 'N/A':>8} → {o.p50_ms or 'N/A':>8} ms  {p50_imp}")
            
            # p99
            _, p99_imp = calculate_improvement(b.p99_ms, o.p99_ms, lower_is_better=True)
            lines.append(f"  {'':<23} P99: {b.p99_ms or 'N/A':>8} → {o.p99_ms or 'N/A':>8} ms  {p99_imp}")
    
    lines.append("")
    lines.append("THROUGHPUT COMPARISON (higher is better)")
    lines.append("-" * 80)
    
    for test_name in key_metrics:
        if test_name in baseline or test_name in optimized:
            b = baseline.get(test_name, BenchmarkMetrics(name=test_name))
            o = optimized.get(test_name, BenchmarkMetrics(name=test_name))
            
            if b.throughput_qps is not None or o.throughput_qps is not None:
                _, qps_imp = calculate_improvement(b.throughput_qps, o.throughput_qps, lower_is_better=False)
                lines.append(f"  {test_name[:23]:<23} {b.throughput_qps or 'N/A':>8} → {o.throughput_qps or 'N/A':>8} qps  {qps_imp}")
    
    lines.append("")
    lines.append("=" * 80)
    lines.append("SLA TARGET STATUS")
    lines.append("=" * 80)
    
    # Check SLA compliance
    sla_metrics = optimized.get('SLA_Compliance') or optimized.get('overall')
    if sla_metrics:
        targets = [
            ("p50 ≤ 50ms", sla_metrics.p50_ms, 50, True),
            ("p99 ≤ 500ms", sla_metrics.p99_ms, 500, True),
            ("Throughput ≥ 100 qps", sla_metrics.throughput_qps, 100, False),
        ]
        
        for name, value, target, lower_is_better in targets:
            if value is None:
                status = "N/A"
            elif lower_is_better:
                status = "\033[92mPASS\033[0m" if value <= target else "\033[91mFAIL\033[0m"
            else:
                status = "\033[92mPASS\033[0m" if value >= target else "\033[91mFAIL\033[0m"
            
            lines.append(f"  {name:<30}: {value or 'N/A':>10} [{status}]")
    
    lines.append("")
    lines.append("=" * 80)
    
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description='Compare benchmark results')
    parser.add_argument('baseline', nargs='?', help='Baseline log file')
    parser.add_argument('optimized', nargs='?', help='Optimized log file')
    parser.add_argument('--dir', help='Directory containing benchmark results')
    
    args = parser.parse_args()
    
    if args.dir:
        # Find latest baseline and optimized files
        results_dir = Path(args.dir)
        baseline_files = sorted(results_dir.glob('*baseline*.log'))
        optimized_files = sorted(results_dir.glob('*after*.log') or results_dir.glob('*optimized*.log'))
        
        if not baseline_files:
            print("No baseline files found in", args.dir)
            sys.exit(1)
        
        baseline_path = baseline_files[-1]  # Latest
        optimized_path = optimized_files[-1] if optimized_files else baseline_files[-1]
        
        print(f"Comparing: {baseline_path.name} vs {optimized_path.name}")
        print()
    else:
        if not args.baseline or not args.optimized:
            parser.print_help()
            sys.exit(1)
        baseline_path = Path(args.baseline)
        optimized_path = Path(args.optimized)
    
    baseline_metrics = parse_log_file(baseline_path)
    optimized_metrics = parse_log_file(optimized_path)
    
    report = compare_metrics(baseline_metrics, optimized_metrics)
    print(report)


if __name__ == '__main__':
    main()
