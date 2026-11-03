CC=gcc
CLANG=clang
CFLAGS=-O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
all: fuzzer targetAbnormal
fuzzer: obj7.c runner.c runner.h
	$(CC) $(CFLAGS) -o $@ obj7.c runner.c
targetAbnormal: targetAbnormal.c cov_handlers.c
	$(CLANG) -O1 -g -c -o cov_handlers.o cov_handlers.c
	$(CLANG) -O1 -g -fsanitize-coverage=inline-8bit-counters -c -o targetAbnormal.o targetAbnormal.c
	$(CLANG) -o $@ targetAbnormal.o cov_handlers.o
run: all
	./fuzzer ./targetAbnormal 100 70001 10
clean:
	rm -f fuzzer targetAbnormal *.o cov.bin crash.bin
