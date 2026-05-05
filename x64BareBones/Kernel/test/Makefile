CC     = gcc
CFLAGS = -Wall -std=c99 -O0 -g -I../include

.PHONY: all run clean run_buddy run_bitmap

all: test_buddy test_bitmap

test_buddy: test_buddy.c ../memory/buddy_mm.c
	$(CC) $(CFLAGS) -o $@ $^

test_bitmap: test_bitmap.c ../memory/bitmap_mm.c
	$(CC) $(CFLAGS) -o $@ $^

run_buddy: test_buddy
	./test_buddy

run_bitmap: test_bitmap
	./test_bitmap

run: run_buddy run_bitmap

clean:
	rm -f test_buddy test_bitmap
