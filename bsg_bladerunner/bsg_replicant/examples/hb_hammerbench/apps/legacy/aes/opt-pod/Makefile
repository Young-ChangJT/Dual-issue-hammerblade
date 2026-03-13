
################################################################################
# environment.mk verifies the build environment and sets the following
# makefile variables:
#
# LIBRAIRES_PATH: The path to the libraries directory
# HARDWARE_PATH: The path to the hardware directory
# EXAMPLES_PATH: The path to the examples directory
# BASEJUMP_STL_DIR: Path to a clone of BaseJump STL
# BSG_MANYCORE_DIR: Path to a clone of BSG Manycore
###############################################################################

HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
tile-x ?= 16
tile-y ?= 8
override BSG_MACHINE_PATH = $(REPLICANT_PATH)/machines/pod_X1Y1_ruche_X$(tile-x)Y$(tile-y)_hbm_one_pseudo_channel
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk

SPMD_SRC_PATH = $(BSG_MANYCORE_DIR)/software/spmd
CUDALITE_SRC_PATH = $(SPMD_SRC_PATH)/bsg_cuda_lite_runtime


# KERNEL_NAME is the name of the CUDA-Lite Kernel
KERNEL_NAME = aes
VPATH = ../tiny-AES-c

###############################################################################
# Host code compilation flags and flow
###############################################################################
TILE_GROUP_DIM_X = $(tile-x)
TILE_GROUP_DIM_Y = $(tile-y)
NUM_ITER = 4
#TILE_GROUP_DIM_X = 1
#TILE_GROUP_DIM_Y = 1
#NUM_ITER = 1

# TEST_SOURCES is a list of source files that need to be compiled
TEST_SOURCES = main.cpp
TEST_SOURCES += aes.c

aes.o: CFLAGS += -DHOST_CODE

DEFINES += -D_XOPEN_SOURCE=500 -D_BSD_SOURCE -D_DEFAULT_SOURCE -DECB=0 -DCTR=0
DEFINES += -DTILE_GROUP_DIM_X=$(TILE_GROUP_DIM_X)
DEFINES += -DTILE_GROUP_DIM_Y=$(TILE_GROUP_DIM_Y)
DEFINES += -DNUM_ITER=$(NUM_ITER)
CDEFINES += 
CXXDEFINES += 

FLAGS     = -g -Wall -Wno-unused-function -Wno-unused-variable -I../tiny-AES-c/
CFLAGS   += -std=gnu99 $(FLAGS)
CXXFLAGS += -std=c++11 $(FLAGS) -O3

# compilation.mk defines rules for compilation of C/C++
include $(EXAMPLES_PATH)/compilation.mk

###############################################################################
# Host code link flags and flow
###############################################################################

LDFLAGS +=

# link.mk defines rules for linking of the final execution binary.
include $(EXAMPLES_PATH)/link.mk

###############################################################################
# Device code compilation flow
###############################################################################

# BSG_MANYCORE_KERNELS is a list of manycore executables that should
# be built before executing.
BSG_MANYCORE_KERNELS = kernel.riscv

# To switch between g++ and clang, uncomment the line below. g++ is
# the default. To view the disassembly, type `make kernel.dis`

#tiny-AES-c/aes.rvo: RISCV_CXX = $(RISCV_CLANG)
#aes_kernel.rvo: RISCV_OPT_LEVEL = -O3
aes_kernel.rvo: RISCV_DEFINES += -DECB=0 -DCTR=0
aes_kernel.rvo: RISCV_DEFINES += -DBSG_MANYCORE
aes_kernel.rvo: RISCV_DEFINES += -DBSG_MANYCORE_OPTIMIZED
aes_kernel.rvo: RISCV_DEFINES += -DBSG_MANYCORE_SBOX_LOCAL
aes_kernel.rvo: RISCV_DEFINES += -DBSG_MANYCORE_NODECRYPT
kernel.rvo: RISCV_INCLUDES += -I.

kernel.riscv: kernel.rvo aes_kernel.rvo

# Tile Group Dimensions
RISCV_DEFINES += -Dbsg_tiles_X=$(TILE_GROUP_DIM_X)
RISCV_DEFINES += -Dbsg_tiles_Y=$(TILE_GROUP_DIM_Y)

include $(EXAMPLES_PATH)/cuda/riscv.mk

###############################################################################
# Execution flow
#
# C_ARGS: Use this to pass arguments that you want to appear in argv
#         For SPMD tests C arguments are: <Path to RISC-V Binary> <Test Name>
#
# SIM_ARGS: Use this to pass arguments to the simulator
###############################################################################
C_ARGS ?= $(BSG_MANYCORE_KERNELS) $(KERNEL_NAME)

SIM_ARGS ?=

# Include platform-specific execution rules
include $(EXAMPLES_PATH)/execution.mk

###############################################################################
# Regression Flow
###############################################################################

regression: exec.log
	@grep "BSG REGRESSION TEST .*PASSED.*" $< > /dev/null

.DEFAULT_GOAL := help

.PHONY: clean

clean:
	rm -rf aes.rvo
	rm -rf stats
	rm -rf vanilla_core_pc_hist.csv

