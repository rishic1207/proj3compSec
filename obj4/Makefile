# Objective 4: coverage-guided fuzzer
#   make            build the fuzzer and the instrumented SUT
#   make run        fuzz once
#   ./run_trials.sh 10 ; ./stats.py trial*/trial*.log
CC      = gcc
CLANG   = clang
CFLAGS  = -O2 -Wall -Wextra
SEED   ?=

.PHONY: all run clean

all: fuzzer targetCov

fuzzer: obj4.c
	$(CC) $(CFLAGS) -o $@ $<

# The handlers must NOT be instrumented, so compile them separately.
cov_handlers.o: cov_handlers.c
	$(CLANG) $(CFLAGS) -c -o $@ $<

targetCov.o: targetCov.c
	$(CLANG) -g -fsanitize-coverage=trace-pc-guard -c -o $@ $<

targetCov: targetCov.o cov_handlers.o
	$(CLANG) -o $@ $^

run: all
	./fuzzer ./targetCov $(SEED)

clean:
	rm -f fuzzer targetCov *.o cov.bin crash.bin