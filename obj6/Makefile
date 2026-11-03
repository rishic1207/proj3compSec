CC=gcc
CLANG=clang
CFLAGS=-O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
all: fuzzer targetCovLoop
fuzzer: obj6.c runner.c runner.h
	$(CC) $(CFLAGS) -o $@ obj6.c runner.c
targetCovLoop: targetCovLoop.c cov_handlers.c
	$(CLANG) -O1 -g -c -o cov_handlers.o cov_handlers.c
	$(CLANG) -O1 -g -fsanitize-coverage=inline-8bit-counters -c -o targetCovLoop.o targetCovLoop.c
	$(CLANG) -o $@ targetCovLoop.o cov_handlers.o
run: all
	./fuzzer ./targetCovLoop
clean:
	rm -f fuzzer targetCovLoop *.o cov.bin crash.bin
