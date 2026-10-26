#!/usr/bin/env python3
"""Loop hit-count check (Objective 5, step 3).
Runs the instrumented SUT on each input and prints every edge's hit count,
then the edges whose counts differ between the inputs (the loop edges).

Usage: ./hitcounts.py S0a Sza [more inputs...]
"""
import os, subprocess, sys

TARGET = "./targetCovLoop"
COV = "cov.bin"

runs = {}
for inp in sys.argv[1:]:
    if os.path.exists(COV):
        os.remove(COV)
    r = subprocess.run([TARGET, inp], capture_output=True)
    if r.returncode < 0 or r.returncode > 128:
        print(f"{inp!r}: crashed (exit {r.returncode}), no coverage written")
        continue
    counts = list(open(COV, "rb").read())
    runs[inp] = counts
    hit = {i: c for i, c in enumerate(counts) if c}
    print(f"{inp!r}: edge hit counts {hit}")

if len(runs) >= 2:
    n = max(len(c) for c in runs.values())
    diff = [i for i in range(n)
            if len({(c[i] if i < len(c) else 0) for c in runs.values()}) > 1]
    print("\nEdges whose hit counts differ between inputs:")
    for i in diff:
        print(f"  edge {i}: " + ", ".join(f"{k!r}={v[i] if i < len(v) else 0}"
                                          for k, v in runs.items()))