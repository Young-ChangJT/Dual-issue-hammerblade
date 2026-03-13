
###############################################################################
# Host code compilation flags and flow
###############################################################################
# import parameters and APP_PATH
include parameters.mk
include app_path.mk

HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
tile-x ?= 16
tile-y ?= 8
override BSG_MACHINE_PATH = $(REPLICANT_PATH)/machines/pod_X1Y1_ruche_X$(tile-x)Y$(tile-y)_hbm_one_pseudo_channel
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk

# Tile Group Dimensions
TILE_GROUP_DIM_X ?= $(tile-x)
TILE_GROUP_DIM_Y ?= $(tile-y)

vpath %.c   $(APP_PATH)
vpath %.cpp $(APP_PATH)

# TEST_SOURCES is a list of source files that need to be compiled
TEST_SOURCES := main.cpp
TEST_SOURCES += read_graph.cpp
TEST_SOURCES += transpose_graph.cpp
TEST_SOURCES += breadth_first_search_graph.cpp
TEST_SOURCES += mmio.c

DEFINES += -D_XOPEN_SOURCE=500 -D_BSD_SOURCE -D_DEFAULT_SOURCE
DEFINES += -Dbsg_tiles_X=$(TILE_GROUP_DIM_X) -Dbsg_tiles_Y=$(TILE_GROUP_DIM_Y)
CXXDEFINES +=

FLAGS     = -g -Wall -Wno-unused-function -Wno-unused-variable
CFLAGS   += -std=c99 $(FLAGS)
CXXFLAGS += -std=c++11 $(FLAGS)

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
RISCV_CCPPFLAGS += -DBLOCK_WORDS=$(BSG_MACHINE_VCACHE_BLOCK_SIZE_WORDS)
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
C_ARGS ?= $(BSG_MANYCORE_KERNELS) $(APP_PATH)/inputs/$(input).mtx $(start)

SIM_ARGS ?=

# Include platform-specific execution rules
include $(EXAMPLES_PATH)/execution.mk

RUN_RULES += saifgen.log
RUN_RULES += repl.log
RUN_RULES += pc-histogram.log
RUN_RULES += debug.log
RUN_RULES += profile.log
RUN_RULES += exec.log
#$(RUN_RULES): $(APP_PATH)/inputs/$(input).mtx
#$(APP_PATH)/inputs/$(input).mtx:
#	$(MAKE) -C $(APP_PATH)/inputs/ $(input).mtx

###############################################################################
# Regression Flow
###############################################################################

.DEFAULT_GOAL := help


