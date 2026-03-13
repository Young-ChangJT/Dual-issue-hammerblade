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
REPLICANT_PATH=$(shell git rev-parse --show-toplevel)
include $(REPLICANT_PATH)/environment.mk
APPLICATION_PATH=$(HB_HAMMERBENCH_PATH)/apps/spgemm/spmm

TARGETS := partition

CXXFLAGS += -std=c++11 -O3
CXXFLAGS += -I$(HB_HAMMERBENCH_PATH)/apps/spgemm/include/host
CXXFLAGS += -I$(APPLICATION_PATH)/include/host
CXXFLAGS += -I$(HB_HAMMERBENCH_PATH)/apps/spgemm/imports/eigen

partition: $(HB_HAMMERBENCH_PATH)/apps/spgemm/include/host/EigenSparseMatrix.hpp
partition: partition.cpp
	$(CXX) $(CXXFLAGS) -o $@ $(filter %.cpp,$^) $(LDFLAGS)

include $(APPLICATION_PATH)/inputs.mk

PARTITION_TESTS = $(foreach input,$(INPUTS),partition.$(input))
$(PARTITION_TESTS): partition.%: inputs partition
	./partition $($*) $($*__directed) $($*__weighted) $($*__zero-indexed) $@ | tee $@

clean:
	rm -f $(PARTITION_TESTS)
	rm -f $(TARGETS)
	rm -f *.el
	rm -f *.csv
