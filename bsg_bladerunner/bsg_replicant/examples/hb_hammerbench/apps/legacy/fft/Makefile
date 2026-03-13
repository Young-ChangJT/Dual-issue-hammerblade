HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk

########################
# Setup this directory #
########################
setup:
	@echo nothing to be done for $@

##############################
# CHANGE ME: IMPLEMENTATIONS #
##############################
IMPLS := 128
# IMPLS += 256

EXECUTION_DIRS := $(foreach impl,$(IMPLS),$(HB_HAMMERBENCH_PATH)/apps/fft/$(impl))

include $(HB_HAMMERBENCH_PATH)/apps/common_complex.mk
