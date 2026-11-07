CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -DCL_TARGET_OPENCL_VERSION=120 -O2 
LDFLAGS = -lOpenCL -L/usr/local/cuda/lib64/
IFLAGS = -I/usr/local/cuda/include/
TARGET = lab.elf

SRC_DIR := src
SRC = $(SRC_DIR)/*.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(IFLAGS) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./test.sh 70

.PHONY: all clean