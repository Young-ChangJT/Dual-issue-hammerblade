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
include parameters.mk
include app_path.mk
HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
override BSG_MACHINE_PATH = $(REPLICANT_PATH)/machines/pod_X2Y1_ruche_X$(tile-x)Y$(tile-y)_hbm_one_pseudo_channel 
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk


###############################################################################
# Host code compilation flags and flow
###############################################################################
# import parameters and APP_PATH

# Tile Group Dimensions
TILE_GROUP_DIM_X ?= $(tile-x)
TILE_GROUP_DIM_Y ?= $(tile-y)

vpath %.c   $(APP_PATH)
vpath %.cpp $(APP_PATH)

# TEST_SOURCES is a list of source files that need to be compiled
TEST_SOURCES = main.cpp

DEFINES += -D_XOPEN_SOURCE=500 -D_BSD_SOURCE -D_DEFAULT_SOURCE
DEFINES += -DSIZE=$(buffer-size)
CXXDEFINES += -Dbsg_tiles_X=$(TILE_GROUP_DIM_X) -Dbsg_tiles_Y=$(TILE_GROUP_DIM_Y)
CXXDEFINES += -DCACHE_LINE_WORDS=$(BSG_MACHINE_VCACHE_LINE_WORDS)
CXXDEFINES +=

FLAGS     = -g -Wall -Wno-unused-function -Wno-unused-variable
CFLAGS   += -std=c99 $(FLAGS)
CXXFLAGS += -std=c++11 $(FLAGS)
ifeq ($(warm-cache),yes)
FLAGS += -DWARM_CACHE
endif
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

RISCV_CCPPFLAGS += -O3 -std=c++14
RISCV_CCPPFLAGS += -Dbsg_tiles_X=$(TILE_GROUP_DIM_X)
RISCV_CCPPFLAGS += -Dbsg_tiles_Y=$(TILE_GROUP_DIM_Y)
RISCV_CCPPFLAGS += -DCACHE_LINE_WORDS=$(BSG_MACHINE_VCACHE_LINE_WORDS)
ifeq ($(warm-cache),yes)
RISCV_CCPPFLAGS += -DWARM_CACHE
endif
RISCV_LDFLAGS += -flto
RISCV_TARGET_OBJECTS = kernel.rvo
BSG_MANYCORE_KERNELS = main.riscv

include $(EXAMPLES_PATH)/cuda/riscv.mk
###############################################################################
# Execution flow
#
# C_ARGS: Use this to pass arguments that you want to appear in argv
#         For SPMD tests C arguments are: <Path to RISC-V Binary> <Test Name>
#
# SIM_ARGS: Use this to pass arguments to the simulator
###############################################################################
C_ARGS ?= $(BSG_MANYCORE_KERNELS) memcpy

SIM_ARGS ?=

# Include platform-specific execution rules
include $(EXAMPLES_PATH)/execution.mk

###############################################################################
# Regression Flow
###############################################################################

regression: exec.log
	@grep "BSG REGRESSION TEST .*PASSED.*" $< > /dev/null

.DEFAULT_GOAL := help


wave:
	dve -full64 -vpd debug.vpd &
