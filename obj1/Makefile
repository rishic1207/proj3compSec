CC = gcc
CFLAGS = -O2 -Wall -Wextra
TARGET ?= ./target
SEED ?=

.PHONY: all run test-pol clean

all: fuzzer

fuzzer: obj1.c
	$(CC) $(CFLAGS) -o $@ $<

# Fuzz the real target (optionally: make run SEED=12345)
run: fuzzer
	./fuzzer $(TARGET) $(SEED)

# Quick sanity check against the proof-of-life program
test-pol: fuzzer
	./fuzzer ./pol $(SEED)

clean:
	rm -f fuzzer crash.bin