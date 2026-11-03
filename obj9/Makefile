CC=gcc
CFLAGS=-O2 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
all: fuzzer
fuzzer: obj9.c runner.c runner.h
	$(CC) $(CFLAGS) -o $@ obj9.c runner.c
run: all
	@test -n "$(PDFTOTEXT)" || (echo "Set PDFTOTEXT=/absolute/path/to/instrumented/pdftotext"; exit 1)
	./fuzzer $(PDFTOTEXT) 1000 12345 10
clean:
	rm -f fuzzer *.o cov.bin current.pdf
