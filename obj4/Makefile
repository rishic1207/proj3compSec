# Objective 4: coverage-guided fuzzer
#   make            build the fuzzer and the instrumented SUT
#   make run        fuzz once
#   make trials     launch 10 concurrent trials
#   make stats      mean and std. dev. across finished trials
.RECIPEPREFIX = >
CC      = gcc
CLANG   = clang
CFLAGS  = -O2 -Wall -Wextra
URL     = https://courses.cs.vt.edu/~cs4264/static/fuzzing
SEED   ?=

.PHONY: all run trials stats clean

all: fuzzer targetCov

fuzzer: obj4.c
> $(CC) $(CFLAGS) -o $@ $<

# The handlers must NOT be instrumented, so compile them separately.
cov_handlers.o: cov_handlers.c
> $(CLANG) $(CFLAGS) -c -o $@ $<

# Download the provided source if it isn't here yet.
targetCov.c:
> wget -q -O $@ $(URL)/$@

targetCov.o: targetCov.c
> $(CLANG) -g -fsanitize-coverage=trace-pc-guard -c -o $@ $<

targetCov: targetCov.o cov_handlers.o
> $(CLANG) -o $@ $^

run: all
> ./fuzzer ./targetCov $(SEED)

trials: all
> ./run_trials.sh 10

stats:
> ./stats.py trial*/trial*.log

clean:
> rm -f fuzzer targetCov *.o cov.bin crash.bin
