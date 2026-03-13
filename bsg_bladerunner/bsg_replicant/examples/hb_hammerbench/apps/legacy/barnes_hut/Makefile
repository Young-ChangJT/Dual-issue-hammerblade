##################################################################################
# BSD 3-Clause License								 #
# 										 #
# Copyright (c) 2022, Bespoke Silicon Group					 #
# All rights reserved.								 #
# 										 #
# Redistribution and use in source and binary forms, with or without		 #
# modification, are permitted provided that the following conditions are met:	 #
# 										 #
# 1. Redistributions of source code must retain the above copyright notice, this #
#    list of conditions and the following disclaimer.				 #
# 										 #
# 2. Redistributions in binary form must reproduce the above copyright notice,	 #
#    this list of conditions and the following disclaimer in the documentation	 #
#    and/or other materials provided with the distribution.			 #
# 										 #
# 3. Neither the name of the copyright holder nor the names of its		 #
#    contributors may be used to endorse or promote products derived from	 #
#    this software without specific prior written permission.			 #
# 										 #
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"	 #
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE	 #
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE #
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE	 #
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL	 #
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR	 #
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER	 #
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,	 #
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE	 #
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.		 #
##################################################################################

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
HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
tile-x ?= 16
tile-y ?= 8
override BSG_MACHINE_PATH = $(REPLICANT_PATH)/machines/pod_X1Y1_ruche_X$(tile-x)Y$(tile-y)_hbm_one_pseudo_channel
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk

BARNES_HUT_PATH := $(HB_HAMMERBENCH_PATH)/apps/barnes_hut
# TEST_NAME is the basename of the executable
TEST_NAME = main
# KERNEL_NAME is the name of the CUDA-Lite Kernel
KERNEL_NAME = nbody

Galois:
	git submodule update --init Galois

Galois/install: Galois
	mkdir $@

Galois/build: Galois
	mkdir $@

Galois/build/CMakeCache.txt: | Galois/build
	LLVM_DIR=$(shell llvm-config-11-64 --prefix) cmake -B $(dir $@) -S Galois  -DBOOST_ROOT=/usr/include/boost169 -DCMAKE_INSTALL_PREFIX=Galois/install -DBUILD_SHARED_LIBS=ON

Galois/install/lib64/libgalois_shmem.so: Galois/build/CMakeCache.txt | Galois/install 
	$(MAKE) -C Galois/build/ install

setup: Galois/install/lib64/libgalois_shmem.so


###############################################################################
# Host code compilation flags and flow
###############################################################################
# TEST_SOURCES is a list of source files that need to be compiled

vpath %.cpp $(BARNES_HUT_PATH)/Galois/lonestar/liblonestar/src/
TEST_SOURCES  = Barneshut.cpp  
TEST_SOURCES  += BoilerPlate.cpp

TEST_HEADERS = Node.hpp Point.hpp Body.hpp Config.hpp

TILE_GROUP_DIM_X ?= $(tile-x)
TILE_GROUP_DIM_Y ?= $(tile-y)

DEFINES += -DTILE_GROUP_DIM_X=$(TILE_GROUP_DIM_X)
DEFINES += -DTILE_GROUP_DIM_Y=$(TILE_GROUP_DIM_Y)
DEFINES += -D_XOPEN_SOURCE=500 -D_BSD_SOURCE -D_DEFAULT_SOURCE
CDEFINES += 
CXXDEFINES += 

INCLUDES  = -I$(BARNES_HUT_PATH)/Galois/install/include -I$(BARNES_HUT_PATH)/Galois/lonestar/liblonestar/include/
INCLUDES += -I$(shell llvm-config-11-64  --includedir) -I$(BARNES_HUT_PATH)/.
FLAGS     = -O3 -g -Wall -Wno-unused-function -Wno-unused-variable -Wno-strict-aliasing -Wno-deprecated-declarations
CFLAGS   += -std=c99 $(FLAGS)
CXXFLAGS += -std=c++17 $(FLAGS) 
LDFLAGS  = -lpthread -Wl,-rpath=$(BARNES_HUT_PATH)/Galois/install/lib64 -L$(BARNES_HUT_PATH)/Galois/install/lib64 -lgalois_shmem  $(shell llvm-config-11-64 --ldflags --libs)

main.so: $(BARNES_HUT_PATH)/Galois/install/lib64/libgalois_shmem.so

# compilation.mk defines rules for compilation of C/C++
include $(EXAMPLES_PATH)/compilation.mk

###############################################################################
# Host code link flags and flow
###############################################################################

# link.mk defines rules for linking of the final execution binary.
include $(EXAMPLES_PATH)/link.mk

$(TEST_OBJECTS): $(TEST_HEADERS)

###############################################################################
# Device code compilation flow
###############################################################################

# BSG_MANYCORE_KERNELS is a list of manycore executables that should
# be built before executing.
PHASE    ?= forces
POD_ID_X ?= 0
POD_ID_Y ?= 0
NPODS_X  ?= 8
NPODS_Y  ?= 8
NBODIES  ?= 256

BSG_MANYCORE_KERNELS ?= kernel.riscv

update.rvo: RISCV_CXX = $(RISCV_CLANGXX)
build.rvo: RISCV_CXX = $(RISCV_CLANGXX)

# use comments to switch different implementations
#forces_dmem_stack.rvo: RISCV_CXX = $(RISCV_GCC)
#kernel.riscv: forces_dmem_stack.rvo
forces.rvo: RISCV_CXX = $(RISCV_GCC)
kernel.riscv: forces.rvo

kernel.riscv: update.rvo
kernel.riscv: summarize.rvo
kernel.riscv: build.rvo

RISCV_INCLUDES  += -I$(BARNES_HUT_PATH)
RISCV_CCPPFLAGS += -D__KERNEL__ -O3

RISCV_DEFINES += -DTILE_GROUP_DIM_X=$(TILE_GROUP_DIM_X)
RISCV_DEFINES += -DTILE_GROUP_DIM_Y=$(TILE_GROUP_DIM_Y) -DRISCV
RISCV_DEFINES += -Dbsg_tiles_X=$(TILE_GROUP_DIM_X)
RISCV_DEFINES += -Dbsg_tiles_Y=$(TILE_GROUP_DIM_Y)
ifeq ($(PHASE),update)
RISCV_DEFINES += -DKERNEL_UPDATE
endif
ifeq ($(PHASE),forces)
RISCV_DEFINES += -DKERNEL_FORCES
endif
ifeq ($(PHASE),summarize)
RISCV_DEFINES += -DKERNEL_SUMMARIZE
endif
ifeq ($(PHASE),build)
RISCV_DEFINES += -DKERNEL_BUILD
endif

include $(EXAMPLES_PATH)/cuda/riscv.mk

###############################################################################
# Execution flow
#
# C_ARGS: Use this to pass arguments that you want to appear in argv
#         For SPMD tests C arguments are: <Path to RISC-V Binary> <Test Name>
#
# SIM_ARGS: Use this to pass arguments to the simulator
###############################################################################


C_ARGS   ?= -n $(NBODIES) -steps 1 -t 1 -phase $(PHASE) -px $(POD_ID_X) -py $(POD_ID_Y) -npx $(NPODS_X) -npy $(NPODS_Y)

SIM_ARGS ?=

# Include platform-specific execution rules
include $(EXAMPLES_PATH)/execution.mk

###############################################################################
# Regression Flow
###############################################################################

regression: main.exec.log
	@grep "BSG REGRESSION TEST .*PASSED.*" $< > /dev/null

###############################################################################
# Default rules, help, and clean
###############################################################################
.DEFAULT_GOAL := help
help:
	@echo "Usage:"
	@echo "make {clean | $(TEST_NAME).{profile,debug} | $(TEST_NAME).{profile,debug}.log}"
	@echo "      $(TEST_NAME).profile: Build executable with profilers enabled"
	@echo "      $(TEST_NAME).debug: Build waveform executable (if VCS)"
	@echo "      $(TEST_NAME).{profile,debug}.log: Run specific executable"
	@echo "      clean: Remove all subdirectory-specific outputs"


.PHONY: clean

clean:
	rm -rf bsg_link.ld
###############################################################################
# Sweep flow
###############################################################################

# 16tx_1ty_threads__1_work__128_offset__18_logsz.deploy

#<phase>_bh__<pod_x>x_<pod_y>x_id__<nbodies>_nbodies__<npods_x>x_<npods_y>y__pods

# bh__forces__
podx-from-name        = $(subst x, ,$(word 1, $(subst _, ,$(filter %_id,$(subst __, ,$1)))))
pody-from-name        = $(subst y, ,$(word 2, $(subst _, ,$(filter %_id,$(subst __, ,$1)))))
npodsx-from-name      = $(subst x, ,$(word 1, $(subst _, ,$(filter %_pods,$(subst __, ,$1)))))
npodsy-from-name      = $(subst y, ,$(word 2, $(subst _, ,$(filter %_pods,$(subst __, ,$1)))))
phase-from-name       = $(firstword $(subst _, ,$(filter %_bh,$(subst __, ,$1))))
nbodies-from-name     = $(firstword $(subst _, ,$(filter %_nbodies,$(subst __, ,$1))))

test-name = $(1)_bh__$(2)x_$(3)y_id__$(4)_nbodies__$(5)x_$(6)y_pods

POD_X = 8
POD_Y = 8

# Forces, 512 Bodies, 64 pods (TEST)
FORCES_TEST_512 += $(call test-name,forces,0,0,512,$(POD_X),$(POD_Y))

# Forces, 16384 Bodies, 64 pods
FORCES_TEST_16K += $(call test-name,forces,0,2,16384,$(POD_X),$(POD_Y))
FORCES_TEST_16K += $(call test-name,forces,0,4,16384,$(POD_X),$(POD_Y))
FORCES_TEST_16K += $(call test-name,forces,2,5,16384,$(POD_X),$(POD_Y))
FORCES_TEST_16K += $(call test-name,forces,2,3,16384,$(POD_X),$(POD_Y))
FORCES_TEST_16K += $(call test-name,forces,4,0,16384,$(POD_X),$(POD_Y))
FORCES_TEST_16K += $(call test-name,forces,4,6,16384,$(POD_X),$(POD_Y))
FORCES_TEST_16K += $(call test-name,forces,6,1,16384,$(POD_X),$(POD_Y))
FORCES_TEST_16K += $(call test-name,forces,6,7,16384,$(POD_X),$(POD_Y))

# Forces, 32768 Bodies, 64 pods
FORCES_TEST_32K += $(call test-name,forces,0,2,32768,$(POD_X),$(POD_Y))
FORCES_TEST_32K += $(call test-name,forces,0,4,32768,$(POD_X),$(POD_Y))
FORCES_TEST_32K += $(call test-name,forces,2,5,32768,$(POD_X),$(POD_Y))
FORCES_TEST_32K += $(call test-name,forces,2,3,32768,$(POD_X),$(POD_Y))
FORCES_TEST_32K += $(call test-name,forces,4,0,32768,$(POD_X),$(POD_Y))
FORCES_TEST_32K += $(call test-name,forces,4,6,32768,$(POD_X),$(POD_Y))
FORCES_TEST_32K += $(call test-name,forces,6,1,32768,$(POD_X),$(POD_Y))
FORCES_TEST_32K += $(call test-name,forces,6,7,32768,$(POD_X),$(POD_Y))

# Forces, 65536 Bodies, 64 pods
FORCES_TEST_64K += $(call test-name,forces,0,2,65536,$(POD_X),$(POD_Y))
FORCES_TEST_64K += $(call test-name,forces,0,4,65536,$(POD_X),$(POD_Y))
FORCES_TEST_64K += $(call test-name,forces,2,5,65536,$(POD_X),$(POD_Y))
FORCES_TEST_64K += $(call test-name,forces,2,3,65536,$(POD_X),$(POD_Y))
FORCES_TEST_64K += $(call test-name,forces,4,0,65536,$(POD_X),$(POD_Y))
FORCES_TEST_64K += $(call test-name,forces,4,6,65536,$(POD_X),$(POD_Y))
FORCES_TEST_64K += $(call test-name,forces,6,1,65536,$(POD_X),$(POD_Y))
FORCES_TEST_64K += $(call test-name,forces,6,7,65536,$(POD_X),$(POD_Y))

generate.512: $(addsuffix .dir, $(FORCES_TEST_512))
generate.16k: $(addsuffix .dir, $(FORCES_TEST_16K))
generate.32k: $(addsuffix .dir, $(FORCES_TEST_32K))
generate.64k: $(addsuffix .dir, $(FORCES_TEST_64K))

%.dir:
	mkdir $@
	echo "PHASE    = $(call phase-from-name,$*)">> $@/Makefile
	echo "POD_ID_X = $(call podx-from-name,$*)">> $@/Makefile
	echo "POD_ID_Y = $(call pody-from-name,$*)">> $@/Makefile
	echo "NPODS_X  = $(call npodsx-from-name,$*)">> $@/Makefile
	echo "NPODS_Y  = $(call npodsy-from-name,$*)">> $@/Makefile
	echo "NBODIES  = $(call nbodies-from-name,$*)">> $@/Makefile
	echo "VPATH =.." >> $@/Makefile
	cat Makefile >> $@/Makefile

run.512: $(addsuffix .run, $(FORCES_TEST_512))
run.16k: $(addsuffix .run, $(FORCES_TEST_16K))
run.32k: $(addsuffix .run, $(FORCES_TEST_32K))
run.64k: $(addsuffix .run, $(FORCES_TEST_64K))

%.run:
	$(MAKE) -C $(subst run,dir,$@) profile.log


cleansweep:
	rm -rf *.dir

include $(HB_HAMMERBENCH_PATH)/apps/common_simple.mk
