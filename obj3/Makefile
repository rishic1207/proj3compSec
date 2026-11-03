# Objective 3: grammar-based fuzzer
#   make            build the fuzzer (downloads the provided target if missing)
#   make run        fuzz the target once (optionally: make run SEED=12345)
#   make trials     launch 5 concurrent trials (trial1..trial5)
#   make stats      mean and std. dev. across finished trials
.RECIPEPREFIX = >
CC     = gcc
CFLAGS = -O2 -Wall -Wextra
URL    = https://courses.cs.vt.edu/~cs4264/static/fuzzing
SEED  ?=

.PHONY: all run trials stats clean

all: fuzzer target

fuzzer: obj3.c
> $(CC) $(CFLAGS) -o $@ $<

# The provided binary is downloaded, not submitted.
target:
> wget -q -O $@ $(URL)/$@ && chmod +x $@

run: fuzzer target
> ./fuzzer ./target $(SEED)

trials: fuzzer target
> ./run_trials.sh 5

stats:
> ./stats.py trial*/trial*.log

clean:
> rm -f fuzzer crash.bin
