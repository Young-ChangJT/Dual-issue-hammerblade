
# This Makefile compiles, links, and executes examples Run `make help`
# to see the available targets for the selected platform.

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

# Meta-Parameters
-include parameters.mk
include app_path.mk
tile-x ?= 16
tile-y ?= 8
HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
override BSG_MACHINE_PATH = $(REPLICANT_PATH)/machines/pod_X1Y1_ruche_X$(tile-x)Y$(tile-y)_hbm_one_pseudo_channel
include $(HB_HAMMERBENCH_PATH)/apps/pagerank/common.mk

TILE_GROUP_DIM_X ?= $(tile-x)
TILE_GROUP_DIM_Y ?= $(tile-y)
vpath %.c   $(APP_PATH)/$(direction)
vpath %.cpp $(APP_PATH)/$(direction)


###############################################################################
# Host code compilation flags and flow
###############################################################################

# TEST_SOURCES is a list of source files that need to be compiled
TEST_SOURCES = pagerank.cpp

DEFINES += -D_XOPEN_SOURCE=500 -D_BSD_SOURCE -D_DEFAULT_SOURCE
DEFINES += -DSIM_CURRENT_POD=$(pod-id)
DEFINES += -DNUM_PODS=$(num-pods)
DEFINES += -DKERNEL_FUNCTION=\"$(function)\"
DEFINES += -DCOSIM
DEFINES += -Dbsg_tiles_X=$(TILE_GROUP_DIM_X)
DEFINES += -Dbsg_tiles_Y=$(TILE_GROUP_DIM_Y)
CDEFINES +=
CXXDEFINES +=

FLAGS     = -g -Wall -Wno-unused-function -Wno-unused-variable
FLAGS	 += -I$(GRAPHIT_PATH)/src/runtime_lib -I$(HB_HAMMERBENCH_PATH)/apps/pagerank/
FLAGS	 += -O2
CFLAGS   += -std=c99 $(FLAGS)
CXXFLAGS += -std=c++14 $(FLAGS)

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

# kernel.rvo:RISCV_CXX=$(RISCV_CLANGXX)
kernel.riscv: kernel.rvo

# Tile Group Dimensions
RISCV_DEFINES += -Dbsg_tiles_X=$(TILE_GROUP_DIM_X)
RISCV_DEFINES += -Dbsg_tiles_Y=$(TILE_GROUP_DIM_Y)
RISCV_DEFINES += -DSIM_KERNEL_CURRENT_POD=1
RISCV_DEFINES += -DSIM_KERNEL_CURRENT_BLOCK=1
RISCV_CCPPFLAGS += -O2
RISCV_CCPPFLAGS += -I$(GRAPHIT_PATH)/src/runtime_lib/infra_hb/device
RISCV_CXXFLAGS += 

include $(EXAMPLES_PATH)/cuda/riscv.mk

###############################################################################
# Execution flow
#
# C_ARGS: Use this to pass arguments that you want to appear in argv
#         For SPMD tests C arguments are: <Path to RISC-V Binary> <Test Name>
#
# SIM_ARGS: Use this to pass arguments to the simulator
###############################################################################
GRAPH_PATH ?= $(PAGERANK_PATH)/$(graph).el
C_ARGS ?= $(BSG_MANYCORE_KERNELS) -g $(GRAPH_PATH)

SIM_ARGS ?=

# Include platform-specific execution rules
include $(EXAMPLES_PATH)/execution.mk

###############################################################################
# Regression Flow
###############################################################################

regression: exec.log
	@grep "BSG REGRESSION TEST .*PASSED.*" $< > /dev/null

.DEFAULT_GOAL := help

