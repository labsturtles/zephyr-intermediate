
# Module 8 - Unit Testing and Code Coverage

## Overview

This document summarizes the code coverage improvements
achieved during the Zephyr Intermediate Module 8 homework.

Testing environment:
- Zephyr RTOS 4.4
- Platform: native_sim
- Test framework: Ztest
- Mock framework: FFF
- Coverage tools: gcov and gcovr
- Test runner: Twister

## Coverage Comparison

| Metric | Task 1 - Happy Paths | Task 3 - Final |
|--------|----------------------|----------------|
| Line coverage | 43/54 (79.6%) | 54/54 (100%) |
| Function coverage | 5/5 (100%) | 5/5 (100%) |
| Branch coverage | 10/18 (55.6%) | 18/18 (100%) |
| Passed tests | 12 | 26 |
| Skipped tests | 14 | 0 |
| Failed tests | 0 | 0 |

## Task 1 - Happy-Path Tests

Git tag: `v1.0.8-l8t1-course2026`

The initial tests verified normal module behavior,
including temperature thresholds, alarm activation,
threshold changes, and alarm reset.

Results:
- Lines: 79.6%
- Functions: 100%
- Branches: 55.6%
- 12 tests passed
- 14 tests skipped

HTML report: [Task 1 Coverage](coverage-task1/index.html)

## Task 2 - Error-Path Tests

Git tag: `v1.0.8-l8t2-course2026`

Additional tests verified:
- Sensor sample-fetch failures
- Sensor channel-get failures
- Recovery after sensor failures
- NULL pointers
- Invalid threshold values
- Rejected threshold updates

These tests exercised error-handling branches
that were not covered by the happy-path tests.

## Task 3 - Final Coverage

Git tag: `v1.0.8-l8t3-course2026`

Final results:
- Lines: 100%
- Functions: 100%
- Branches: 100%
- 26 tests passed
- 0 tests skipped
- 0 tests failed

HTML report: [Final Coverage](twister-out/coverage/index.html)

## Coverage Improvement

| Metric | Improvement |
|--------|-------------|
| Lines | +20.4 percentage points |
| Functions | Already at 100% |
| Branches | +44.4 percentage points |

## Reproduce Coverage

Run from the `l8-task0` directory:

```sh
west twister -T tests/temp_alarm -p native_sim \
  --coverage \
  --coverage-tool gcovr \
  --gcov-tool gcov \
  --coverage-basedir app/modules/temp_alarm
