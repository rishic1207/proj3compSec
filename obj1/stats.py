#!/usr/bin/env python3
"""Summarize fuzzing trials: ./stats.py trial*/trial*.log
Prints each trial's result plus mean and sample std. dev. (n-1)."""
import re, statistics, sys

rows = []
for path in sys.argv[1:]:
    text = open(path, errors="replace").read()
    if text.count("=== CRASH FOUND ===") != 1:
        print(f"{path}: skipped (no crash yet, or log is corrupted)")
        continue
    case = re.search(r"\(text\): (.*)", text).group(1)
    n = int(re.search(r"Number of test cases: (\d+)", text).group(1))
    t = int(re.search(r"Wall clock time: (\d+)", text).group(1))
    rows.append((path, case, n, t))
    print(f"{path}: input={case} test_cases={n:,} time={t:,}s")

if len(rows) >= 2:
    cases = {r[1] for r in rows}
    print(f"\nAll trials found the same input: {'yes' if len(cases) == 1 else 'NO: ' + str(cases)}")
    ns, ts = [r[2] for r in rows], [r[3] for r in rows]
    print(f"Trials: {len(rows)}")
    print(f"Average number of test cases: {statistics.mean(ns):,.0f}")
    print(f"Std. dev. number of test cases: {statistics.stdev(ns):,.0f}")
    print(f"Average wall clock time: {statistics.mean(ts):,.0f} seconds")
    print(f"Std. dev. wall clock time: {statistics.stdev(ts):,.0f} seconds")