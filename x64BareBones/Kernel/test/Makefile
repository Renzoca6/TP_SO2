CC     = gcc
CFLAGS = -Wall -std=c99 -O0 -g -I../include

TARGET = test_buddy
SRCS   = test_buddy.c ../memory/buddy_mm.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
