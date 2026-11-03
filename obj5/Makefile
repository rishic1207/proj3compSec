# Objective 5: hit-count-guided fuzzer
#   make            build the fuzzer and the instrumented SUT
#   make run        fuzz once
#   make trials     launch 10 concurrent trials
#   make stats      mean and std. dev. across finished trials
#   ./hitcounts.py S0a Sya Sza   loop hit-count check (Objective step 3)
.RECIPEPREFIX = >
CC      = gcc
CLANG   = clang
CFLAGS  = -O2 -Wall -Wextra
URL     = https://courses.cs.vt.edu/~cs4264/static/fuzzing
SEED   ?=

.PHONY: all run trials stats clean

all: fuzzer targetCovLoop

fuzzer: obj5.c
> $(CC) $(CFLAGS) -o $@ $<

# The handlers must NOT be instrumented, so compile them separately.
cov_handlers.o: cov_handlers.c
> $(CLANG) $(CFLAGS) -c -o $@ $<

# Download the provided source if it isn't here yet.
targetCovLoop.c:
> wget -q -O $@ $(URL)/$@

targetCovLoop.o: targetCovLoop.c
> $(CLANG) -g -fsanitize-coverage=inline-8bit-counters -c -o $@ $<

targetCovLoop: targetCovLoop.o cov_handlers.o
> $(CLANG) -o $@ $^

run: all
> ./fuzzer ./targetCovLoop $(SEED)

trials: all
> ./run_trials.sh 10

stats:
> ./stats.py trial*/trial*.log

clean:
> rm -f fuzzer targetCovLoop *.o cov.bin crash.bin
