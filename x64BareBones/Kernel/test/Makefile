CC     = gcc
CFLAGS = -Wall -std=c99 -O0 -g -I../include

<<<<<<< HEAD
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
=======
TARGET_BUDDY    = test_buddy
SRCS_BUDDY      = test_buddy.c ../memory/buddy_mm.c

TARGET_GENERIC  = test_generics
SRCS_GENERIC    = test_generics.c ../memory/buddy_mm.c

TARGET_MM       = test_mm
SRCS_MM         = test_mm.c ../memory/buddy_mm.c

all: $(TARGET_BUDDY) $(TARGET_GENERIC) $(TARGET_MM)

$(TARGET_BUDDY): $(SRCS_BUDDY)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET_GENERIC): $(SRCS_GENERIC)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET_MM): $(SRCS_MM)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET_BUDDY)
	./$(TARGET_BUDDY)

clean:
	rm -f $(TARGET_BUDDY) $(TARGET_GENERIC) $(TARGET_MM)

.PHONY: all run clean
>>>>>>> b9dd36379b5a00b7a3826cabbdc60ae64081e1db
