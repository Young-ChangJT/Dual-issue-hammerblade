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
HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk
APPLICATION_PATH=$(HB_HAMMERBENCH_PATH)/apps/spgemm/spmm
include $(APPLICATION_PATH)/utils.mk

solver: $(HB_HAMMERBENCH_PATH)/apps/spgemm/include/host/Random.hpp
solver: $(HB_HAMMERBENCH_PATH)/apps/spgemm/include/host/EigenSparseMatrix.hpp
solver: $(APPLICATION_PATH)/include/host/Solver.hpp
solver: $(HB_HAMMERBENCH_PATH)/apps/spgemm/src/Random.cpp
solver: $(APPLICATION_PATH)/src/host/solver.cpp
	$(eval INCLUDES+=-I$(HB_HAMMERBENCH_PATH)/apps/spgemm/include/host)
	$(eval INCLUDES+=-I$(APPLICATION_PATH)/include/host)
	$(eval INCLUDES+=-I$(HB_HAMMERBENCH_PATH)/apps/spgemm/imports/eigen)
	$(CXX) -std=c++11 $(INCLUDES) -o $@ $(filter %.cpp,$^)

partition_solver: $(HB_HAMMERBENCH_PATH)/apps/spgemm/include/host/Random.hpp
partition_solver: $(HB_HAMMERBENCH_PATH)/apps/spgemm/include/host/EigenSparseMatrix.hpp
partition_solver: $(APPLICATION_PATH)/include/host/Solver.hpp
partition_solver: $(HB_HAMMERBENCH_PATH)/apps/spgemm/src/Random.cpp
partition_solver: $(APPLICATION_PATH)/src/host/partition_solver.cpp
	$(eval INCLUDES+=-I$(HB_HAMMERBENCH_PATH)/apps/spgemm/include/host)
	$(eval INCLUDES+=-I$(APPLICATION_PATH)/include/host)
	$(eval INCLUDES+=-I$(HB_HAMMERBENCH_PATH)/apps/spgemm/imports/eigen)
	$(CXX) -std=c++11 $(INCLUDES) -o $@ $(filter %.cpp,$^)



include $(APPLICATION_PATH)/inputs.mk

$(foreach input,$(INPUTS),$(input).stats): %.stats: inputs solver
	./solver $($*) $($*__directed) $($*__weighted) $($*__zero-indexed) $@.solve_row.csv \
	$*.input.csv $*.solution.csv $*.answer.csv \
	$*.A_stats.csv $*.B_stats.csv

PARTITION_FACTOR = 8

$(foreach input,$(INPUTS),$(input).partition.stats): %.partition.stats: inputs partition_solver
	./partition_solver $($*) $($*__directed) $($*__weighted) $($*__zero-indexed) $@.solve_row.csv \
	$*.input.csv $*.solution.csv $*.answer.csv \
	$*.A_stats.csv $*.B_stats.csv \
	$(PARTITION_FACTOR)
	$(eval parts\
	= $(call cartesian\
	,$(call range,0,$(PARTITION_FACTOR))\
	,$(call range,0,$(PARTITION_FACTOR))\
	,_))
	$(eval part_stats = $(foreach part,$(parts),$(part).$@.solve_row.csv))
	python3 $(APPLICATION_PATH)/py/summarize_partitions.py $*.solve_row.partitions.summary.csv $(part_stats)

$(foreach input,$(INPUTS),$(input).memcount): %.memcount:
	@echo $*:
	@printf "%e rows\n" $($*__rows)
	@printf "%e columns\n" $($*__cols)
	@printf "%e nonzeros\n" $($*__nnz)
	@printf "%e solution nonzeros\n" $($*__solnnz)
	@$(eval matrices=3)
	@$(eval omatrices=2)
	@$(eval imatrices=2)
	@$(eval wordbytes=4)
	$(eval rbytes=$(shell echo $($*__rows)*2*$(wordbytes)*$(matrices)+$($*__rows)*$(wordbytes)|bc))
	$(eval nzbytes=$(shell echo $($*__nnz)*2*$(wordbytes)*$(omatrices)|bc))
	$(eval solnzbytes=$(shell echo $($*__solnnz)*4*$(wordbytes)*$(imatrices)|bc))
	$(eval total=$(shell echo $(rbytes)+$(nzbytes)+$(solnzbytes)|bc))
	@printf "%e bytes\n" $(total)

clean:
	rm -rf solver *.csv partition_solver
