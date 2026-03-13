HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk

########################
# Setup this directory #
########################
setup:
	$(MAKE) -C imports/ all
	$(MAKE) -C spmm_abrev_multi_pod_model/ tests

##############################
# CHANGE ME: IMPLEMENTATIONS #
##############################
INPUTS += spmm_abrev_multi_pod_model/offshore_input__1_partfactor__0x0_partition__yes_opt__yes_parallel


EXECUTION_DIRS := $(foreach input,$(INPUTS),$(HB_HAMMERBENCH_PATH)/apps/spgemm/$(input))

include $(HB_HAMMERBENCH_PATH)/apps/common_complex.mk
