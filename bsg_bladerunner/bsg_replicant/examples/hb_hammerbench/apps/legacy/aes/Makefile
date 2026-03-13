HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk

########################
# Setup this directory #
########################
setup:
	git submodule update --init

##############################
# CHANGE ME: IMPLEMENTATIONS #
##############################
IMPLS := opt-pod
# IMPLS += unopt-pod
# IMPLS += opt-single
# IMPLS += unopt-single

EXECUTION_DIRS := $(foreach impl,$(IMPLS),$(HB_HAMMERBENCH_PATH)/apps/aes/$(impl))

include $(HB_HAMMERBENCH_PATH)/apps/common_complex.mk
