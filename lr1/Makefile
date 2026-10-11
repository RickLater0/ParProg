# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lOpenCL

TARGET = program

SRC_DIR := src
SRC = $(SRC_DIR)/*.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean run