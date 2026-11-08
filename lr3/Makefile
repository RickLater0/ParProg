CC      = mpicc
CFLAGS  = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -O2
LDFLAGS = -lm
TARGET  = lab.elf

M     ?= 1
N     ?= 1000000
RANKS ?= 0

SRC_DIR := src
SRC     := $(SRC_DIR)/*.c
HDR     := $(SRC_DIR)/*.h

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./test.sh $(M) $(N) $(RANKS)

.PHONY: all clean test