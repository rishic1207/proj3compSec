# Objective 5: hit-count-guided fuzzer
#   make                       build the fuzzer and the instrumented SUT
#   ./hitcounts.py S0a Sza     loop hit-count check (Objective step 3)
#   ./run_trials.sh 10 ; ./stats.py trial*/trial*.log
CC      = gcc
CLANG   = clang
CFLAGS  = -O2 -Wall -Wextra
SEED   ?=

.PHONY: all run clean

all: fuzzer targetCovLoop

fuzzer: obj5.c
	$(CC) $(CFLAGS) -o $@ $<

# The handlers must NOT be instrumented, so compile them separately.
cov_handlers.o: cov_handlers.c
	$(CLANG) $(CFLAGS) -c -o $@ $<

targetCovLoop.o: targetCovLoop.c
	$(CLANG) -g -fsanitize-coverage=inline-8bit-counters -c -o $@ $<

targetCovLoop: targetCovLoop.o cov_handlers.o
	$(CLANG) -o $@ $^

run: all
	./fuzzer ./targetCovLoop $(SEED)

clean:
	rm -f fuzzer targetCovLoop *.o cov.bin crash.bin