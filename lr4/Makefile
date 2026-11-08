################################################################################
# Makefile для ЛР №4 (CUDA 8.0, Fermi sm_20)
################################################################################

CUDA_PATH     ?= /usr/local/cuda
HOST_COMPILER ?= g++
C_COMPILER    ?= gcc

NVCC := $(CUDA_PATH)/bin/nvcc -ccbin $(HOST_COMPILER)
CC   := $(C_COMPILER)

TARGET_SIZE := 64
NVCCFLAGS   := -m$(TARGET_SIZE) -std=c++11 -O3
CCFLAGS     := -std=c99 -O2 -D_POSIX_C_SOURCE=200809L -Wall -Wextra \
               -Wno-missing-field-initializers
LDFLAGS     := -lm

SMS           := 20
GENCODE_FLAGS := -gencode arch=compute_$(SMS),code=sm_$(SMS)

SRC_DIR   := src
OBJ_DIR   := bin
C_SRC     := $(wildcard $(SRC_DIR)/*.c)
CU_SRC    := $(wildcard $(SRC_DIR)/*.cu)
HDR       := $(wildcard $(SRC_DIR)/*.h)
C_OBJ     := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(C_SRC))
CU_OBJ    := $(patsubst $(SRC_DIR)/%.cu,$(OBJ_DIR)/%.o,$(CU_SRC))
OBJ       := $(C_OBJ) $(CU_OBJ)

TARGET    := lab.elf

all: $(TARGET)

$(TARGET): $(OBJ)
	$(NVCC) $(GENCODE_FLAGS) $(NVCCFLAGS) -o $@ $+ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HDR) | $(OBJ_DIR)
	$(CC) $(CCFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cu $(HDR) | $(OBJ_DIR)
	$(NVCC) $(GENCODE_FLAGS) $(NVCCFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

test: $(TARGET)
	./test.sh

.PHONY: all clean test