CC     = gcc
CFLAGS = -Wall -std=c99 -O0 -g -I../include

.PHONY: all run clean run_buddy run_bitmap run_generics run_mm

all: test_buddy test_bitmap test_generics test_mm

test_buddy: test_buddy.c ../memory/buddy_mm.c
	$(CC) $(CFLAGS) -o $@ $^

test_bitmap: test_bitmap.c ../memory/bitmap_mm.c
	$(CC) $(CFLAGS) -o $@ $^

test_generics: test_generics.c ../memory/buddy_mm.c
	$(CC) $(CFLAGS) -o $@ $^

test_mm: test_mm.c ../memory/buddy_mm.c
	$(CC) $(CFLAGS) -o $@ $^

run_buddy: test_buddy
	./test_buddy

run_bitmap: test_bitmap
	./test_bitmap

run_generics: test_generics
	./test_generics

run_mm: test_mm
	./test_mm

run: run_buddy run_bitmap run_generics

clean:
	rm -f test_buddy test_bitmap test_generics test_mm
