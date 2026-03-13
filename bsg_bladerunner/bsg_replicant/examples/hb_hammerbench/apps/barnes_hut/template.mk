include parameters.mk
include app_path.mk

# Hardware;
HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
NUMPODS?=64
tile-x?=16
tile-y?=8
override BSG_MACHINE_PATH = $(REPLICANT_PATH)/machines/pod_X1Y1_ruche_X$(tile-x)Y$(tile-y)_hbm_one_pseudo_channel
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk

# Tile group DIM
TILE_GROUP_DIM_X ?= $(tile-x)
TILE_GROUP_DIM_Y ?= $(tile-y)

# number of pods participating in barrier;
NUM_POD_X=$(BSG_MACHINE_PODS_X)
NUM_POD_Y=$(BSG_MACHINE_PODS_X)

vpath %.c   $(APP_PATH)
vpath %.cpp $(APP_PATH)

# stack size
STACK_SIZE=4096


# Test sources;
TEST_SOURCES := main.cpp

DEFINES += -D_XOPEN_SOURCE=500 -D_BSD_SOURCE -D_DEFAULT_SOURCE
DEFINES += -Dbsg_tiles_X=$(TILE_GROUP_DIM_X) -Dbsg_tiles_Y=$(TILE_GROUP_DIM_Y)
DEFINES += -DNUMPODS=$(NUMPODS)
DEFINES += -DPODID=$(pod-id)
DEFINES += -DNBODIES=$(nbodies)
DEFINES += -DSTACK_SIZE=$(STACK_SIZE)

FLAGS     = -g -Wall -Wno-unused-function -Wno-unused-variable
CFLAGS   += -std=c99 $(FLAGS)
CXXFLAGS += -std=c++11 $(FLAGS)


# compilation rules;
include $(EXAMPLES_PATH)/compilation.mk


# Host Link flags;
LDFLAGS +=
include $(EXAMPLES_PATH)/link.mk


# Device Code;
RISCV_CCPPFLAGS += -O3 -std=c++14
RISCV_CCPPFLAGS += -I$(HB_HAMMERBENCH_PATH)/apps/common
RISCV_CCPPFLAGS += -DNUM_POD_X=$(NUM_POD_X)
RISCV_CCPPFLAGS += -DBSG_MACHINE_GLOBAL_X=$(BSG_MACHINE_GLOBAL_X)
RISCV_CCPPFLAGS += -DBSG_MACHINE_GLOBAL_Y=$(BSG_MACHINE_GLOBAL_Y)
RISCV_CCPPFLAGS += -Dbsg_tiles_X=$(TILE_GROUP_DIM_X)
RISCV_CCPPFLAGS += -Dbsg_tiles_Y=$(TILE_GROUP_DIM_Y)
RISCV_CCPPFLAGS += -DNUMPODS=$(NUMPODS)
RISCV_CCPPFLAGS += -DHB_KERNEL
RISCV_CCPPFLAGS += -DNBODIES=$(nbodies)
RISCV_CCPPFLAGS += -DSTACK_SIZE=$(STACK_SIZE)

RISCV_TARGET_OBJECTS = kernel.rvo
BSG_MANYCORE_KERNELS = main.riscv

include $(EXAMPLES_PATH)/cuda/riscv.mk

# Execution args, flags;
C_ARGS ?= $(BSG_MANYCORE_KERNELS)
SIM_ARGS ?=  +vcs+nostdout

# Exec rules;
include $(EXAMPLES_PATH)/execution.mk
RUN_RULES += saifgen.log
RUN_RULES += repl.log
RUN_RULES += pc-histogram.log
RUN_RULES += debug.log
RUN_RULES += profile.log
RUN_RULES += exec.log
.DEFAULT_GOAL := help

