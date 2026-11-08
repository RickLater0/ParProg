HOST_COMPILER ?= g++
C_COMPILER    ?= gcc

NVCC := $(shell command -v nvcc 2>/dev/null)
ifeq ($(NVCC),)
$(error nvcc not found in PATH; add CUDA bin to PATH)
endif

CUDA_MAJOR := $(shell nvcc --version \
              | sed -n 's/.*release \([0-9]*\)\..*/\1/p')

ifeq ($(CUDA_MAJOR),)
$(error Cannot detect CUDA version; run 'nvcc --version' manually)
endif

ifeq ($(shell test $(CUDA_MAJOR) -lt 9 && echo 1),1)
    GENCODE_FLAGS := -gencode arch=compute_20,code=sm_20
else
    GPU_SM := $(shell nvidia-smi --query-gpu=compute_cap \
                              --format=csv,noheader 2>/dev/null \
              | head -n1 | sed 's/[^0-9]//g')
    ifeq ($(strip $(GPU_SM)),)
        $(info >>> CUDA $(CUDA_MAJOR).x: no GPU detected, fallback sm_75)
        GENCODE_FLAGS := -arch=sm_75
    else
        $(info >>> CUDA $(CUDA_MAJOR).x: detected GPU sm_$(GPU_SM))
        GENCODE_FLAGS := -arch=sm_$(GPU_SM)
    endif
endif

TARGET_SIZE := 64
NVCCFLAGS   := -m$(TARGET_SIZE) -std=c++11 -O3 \
               -Wno-deprecated-gpu-targets \
               -Xcompiler -Wno-redundant-decls
CCFLAGS     := -std=c99 -O2 -D_POSIX_C_SOURCE=200809L \
               -Wall -Wextra -Wno-missing-field-initializers
LDFLAGS     := -lm

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
	$(NVCC) -ccbin $(HOST_COMPILER) $(GENCODE_FLAGS) $(NVCCFLAGS) \
	    -o $@ $+ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HDR) | $(OBJ_DIR)
	$(C_COMPILER) $(CCFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cu $(HDR) | $(OBJ_DIR)
	$(NVCC) -ccbin $(HOST_COMPILER) $(GENCODE_FLAGS) $(NVCCFLAGS) \
	    -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

K			?= 128
M			?= 128
N			?= 128
T1			?= 100
T2			?= -10
RADIUS 		?= 10
MAX_ITER	?= 10000

test: $(TARGET)
	./test.sh $(K) $(M) $(N) $(T1) $(T2) $(RADIUS) $(MAX_ITER)

.PHONY: all clean test