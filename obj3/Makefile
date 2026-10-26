CC = gcc
CFLAGS = -O2 -Wall -Wextra
TARGET ?= ./target
SEED ?=

.PHONY: all run clean

all: fuzzer

fuzzer: obj3.c
	$(CC) $(CFLAGS) -o $@ $<

# Fuzz the real target (optionally: make run SEED=12345)
run: fuzzer
	./fuzzer $(TARGET) $(SEED)

clean:
	rm -f fuzzer crash.bin