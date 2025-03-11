#!/usr/bin/env python3

"""
Script to update test status in TESTPLAN.md.
Usage: ./update_test_status.py [--auto]

If --auto is specified, the script will parse test results from test output.
Otherwise, it will run in interactive mode.
"""

import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple

# Status indicators
STATUS = {
    'not_started': '[ ]',
    'in_progress': '[🟡]',
    'passed': '[✓]',
    'failed': '[❌]',
    'blocked': '[⚠️]'
}

def parse_test_plan(content: str) -> List[Tuple[str, str, int]]:
    """Parse test plan and return list of (section, test, line_number)."""
    tests = []
    current_section = ''
    for i, line in enumerate(content.splitlines()):
        if line.startswith('###'):
            current_section = line.strip('# ')
        elif line.strip().startswith('- ['):
            test = line.split(']', 1)[1].strip()
            tests.append((current_section, test, i))
    return tests

def update_test_status(content: str, updates: Dict[Tuple[str, str], str]) -> str:
    """Update test status in content based on updates dict."""
    lines = content.splitlines()
    tests = parse_test_plan(content)
    
    for section, test, line_num in tests:
        if (section, test) in updates:
            new_status = updates[(section, test)]
            lines[line_num] = f'- {new_status}{test}'
    
    return '\n'.join(lines)

def calculate_summary(content: str) -> Dict[str, int]:
    """Calculate test execution summary."""
    summary = {
        'total': 0,
        'passed': 0,
        'failed': 0,
        'blocked': 0,
        'in_progress': 0,
        'not_started': 0
    }
    
    for line in content.splitlines():
        if line.strip().startswith('- ['):
            summary['total'] += 1
            if '[✓]' in line:
                summary['passed'] += 1
            elif '[❌]' in line:
                summary['failed'] += 1
            elif '[⚠️]' in line:
                summary['blocked'] += 1
            elif '[🟡]' in line:
                summary['in_progress'] += 1
            else:
                summary['not_started'] += 1
    
    return summary

def update_summary(content: str) -> str:
    """Update summary section in test plan."""
    summary = calculate_summary(content)
    coverage = (summary['passed'] / summary['total']) * 100 if summary['total'] > 0 else 0
    
    summary_text = f"""## Test Execution Summary
- Total Tests: {summary['total']}
- Passed: {summary['passed']}
- Failed: {summary['failed']}
- Blocked: {summary['blocked']}
- Not Started: {summary['not_started']}
- Coverage: {coverage:.1f}%"""
    
    pattern = r'## Test Execution Summary.*?(?=\n\n|$)'
    return re.sub(pattern, summary_text, content, flags=re.DOTALL)

def parse_test_output(output_file: str) -> Dict[Tuple[str, str], str]:
    """Parse test output and return status updates."""
    updates = {}
    # Add logic to parse test output format
    return updates

def interactive_update() -> Dict[Tuple[str, str], str]:
    """Interactively update test status."""
    updates = {}
    test_plan_path = Path('TESTPLAN.md')
    content = test_plan_path.read_text()
    tests = parse_test_plan(content)
    
    print("\nTest Status Update")
    print("=================")
    print("Status options:")
    print("p - Passed")
    print("f - Failed")
    print("b - Blocked")
    print("i - In Progress")
    print("s - Skip")
    print("q - Quit")
    
    for section, test, _ in tests:
        while True:
            status = input(f"\n{section} > {test}\nStatus? [p/f/b/i/s/q]: ").lower()
            if status == 'q':
                return updates
            elif status == 's':
                break
            elif status in {'p', 'f', 'b', 'i'}:
                status_map = {
                    'p': STATUS['passed'],
                    'f': STATUS['failed'],
                    'b': STATUS['blocked'],
                    'i': STATUS['in_progress']
                }
                updates[(section, test)] = status_map[status]
                break
    
    return updates

def main():
    test_plan_path = Path('TESTPLAN.md')
    if not test_plan_path.exists():
        print("Error: TESTPLAN.md not found")
        sys.exit(1)
    
    content = test_plan_path.read_text()
    
    if '--auto' in sys.argv:
        # Parse test output for automatic updates
        test_output_path = Path('test_output.txt')
        if not test_output_path.exists():
            print("Error: test_output.txt not found")
            sys.exit(1)
        updates = parse_test_output(str(test_output_path))
    else:
        # Interactive updates
        updates = interactive_update()
    
    if updates:
        content = update_test_status(content, updates)
        content = update_summary(content)
        test_plan_path.write_text(content)
        print("\nTest plan updated successfully!")
    else:
        print("\nNo updates made.")

if __name__ == '__main__':
    main() 