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

test-name		= $(1)_input__$(2)_row-base__$(3)_rows__$(4)_opt__$(5)_parallel
get-input		= $(firstword $(subst _, ,$(filter %_input,$(subst __, ,$1))))
get-row-base		= $(firstword $(subst _, ,$(filter %_row-base,$(subst __, ,$1))))
get-rows		= $(firstword $(subst _, ,$(filter %_rows,$(subst __, ,$1))))
get-opt			= $(firstword $(subst _, ,$(filter %_opt,$(subst __, ,$1))))
get-parallel		= $(firstword $(subst _, ,$(filter %_parallel,$(subst __, ,$1))))

# CHANGE ME: TESTS
# optimized
TESTS += $(call test-name,08blocks,0,300,yes,yes)
TESTS += $(call test-name,wiki-vote,0,8297,yes,yes)
TESTS += $(call test-name,roadNet-CA,0,1971281,yes,yes)
#TESTS += $(call test-name,road-central,0,14081816,yes,yes)
#TESTS += $(call test-name,road-usa,0,1024,yes,yes)
# unoptimized
TESTS += $(call test-name,08blocks,0,300,no,yes)
TESTS += $(call test-name,wiki-vote,0,8297,no,yes)
TESTS += $(call test-name,roadNet-CA,0,1971281,no,yes)
#TESTS += $(call test-name,road-central,0,14081816,no,yes)
#TESTS += $(call test-name,road-usa,0,23947347,no,yes)

define get-test-parameters
$(eval INPUT=$(call get-input,$1))
$(eval ROW_BASE=$(call get-row-base,$1))
$(eval ROWS=$(call get-rows,$1))
$(eval OPT=$(call get-opt,$1))
$(eval PARALLEL=$(call get-parallel,$1))
endef

# dwarfs should define this function hook to add
# app specific parameters
# 1: test-name
# 2: parameters.mk target
#
# $(2) is set to the parameters.mk of the test directory
# typically this is $(APPLICATION_PATH)/$(test-name)/parameters.mk
define parameters-mk-add-application-params
$(eval $(call get-test-parameters,$1))
@echo INPUT=$(INPUT)		>> $2
@echo ROW_BASE=$(ROW_BASE)	>> $2
@echo ROWS=$(ROWS)		>> $2
@echo OPT=$(OPT)		>> $2
@echo PARALLEL=$(PARALLEL)	>> $2
endef

# This can be overriden to set a custom simulation directory for a test.
# Defaults to $(APPLICATION_PATH)/$(test-name)
#
# See spmm for examples of overriding
define get-sim-dir-from-test
$(eval SIMULATION_DIR=$(APPLICATION_PATH)/spmm_abrev/$1)
endef

# This function needs to be defined
APPLICATION_PATH=$(HB_HAMMERBENCH_PATH)/apps/spgemm

include $(HB_HAMMERBENCH_PATH)/apps/spgemm/parameterized-tb.mk

plots: $(addsuffix .plots,$(TESTS))
$(addsuffix .plots,$(TESTS)): %.plots: %
	$(MAKE) -C $* plots

vcache.summary.csv: $(addsuffix /vcache_stats.csv,$(TESTS))
	@PYTHONPATH=$(HB_HAMMERBENCH_PATH)/apps/spgemm/imports/hammerblade-helpers/py python3 $(APPLICATION_PATH)/py/vcache_abrev.py $^

dramsim3.summary.csv: $(addsuffix /dramsim3.tag.json,$(TESTS))
	@PYTHONPATH=$(HB_HAMMERBENCH_PATH)/apps/spgemm/imports/hammerblade-helpers/py python3 $(APPLICATION_PATH)/py/dramsim3_abrev.py $^
