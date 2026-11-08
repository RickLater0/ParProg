CC      = mpicc
CFLAGS  = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -flto -funroll-loops
LDFLAGS = -lm -flto
TARGET  = lab.elf

M     ?= 1
N     ?= 1000000
RANKS ?= 4

SRC_DIR := src
OBJ_DIR := bin

SRC := $(wildcard $(SRC_DIR)/*.c)
HDR := $(wildcard $(SRC_DIR)/*.h)

# src/common.c -> bin/common.o
OBJ := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HDR) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

test: $(TARGET)
	./test.sh $(M) $(N) $(RANKS)

.PHONY: all clean test