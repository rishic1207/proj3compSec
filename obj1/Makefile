# Objective 1: black-box random fuzzer (also holds the Objective 2 trial scripts)
#   make            build the fuzzer (downloads the provided target if missing)
#   make run        fuzz the target once (optionally: make run SEED=12345)
#   make test-pol   sanity check against the proof-of-life program
#   make trials     Objective 2: launch 3 concurrent trials (trial1..trial3)
#   make stats      Objective 2: mean and std. dev. across finished trials
.RECIPEPREFIX = >
CC     = gcc
CFLAGS = -O2 -Wall -Wextra
URL    = https://courses.cs.vt.edu/~cs4264/static/fuzzing
SEED  ?=

.PHONY: all run test-pol trials stats clean

all: fuzzer target

fuzzer: obj1.c
> $(CC) $(CFLAGS) -o $@ $<

# Provided binaries are downloaded, not submitted.
target pol:
> wget -q -O $@ $(URL)/$@ && chmod +x $@

run: fuzzer target
> ./fuzzer ./target $(SEED)

test-pol: fuzzer pol
> ./fuzzer ./pol $(SEED)

trials: fuzzer target
> ./run_trials.sh 3

stats:
> ./stats.py trial*/trial*.log

clean:
> rm -f fuzzer crash.bin
