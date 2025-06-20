CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic
TARGET = montador
SRC = main.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

.PHONY: clean
