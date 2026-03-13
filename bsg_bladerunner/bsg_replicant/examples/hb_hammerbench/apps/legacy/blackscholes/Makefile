HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk
BLACKSCHOLES_DIR=$(HB_HAMMERBENCH_PATH)/apps/blackscholes

########################
# Setup this directory #
########################
setup:
	mkdir -p $(BLACKSCHOLES_DIR)
	wget http://parsec.cs.princeton.edu/download/3.0/parsec-3.0-input-native.tar.gz
	wget http://parsec.cs.princeton.edu/download/3.0/parsec-3.0-input-sim.tar.gz
	tar -xf parsec-3.0-input-sim.tar.gz
	tar -xf parsec-3.0-input-native.tar.gz
	cp parsec-3.0/pkgs/apps/blackscholes/inputs/* $(BLACKSCHOLES_DIR)/data
	(cd data/; tar -xf input_native.tar)

##############################
# CHANGE ME: IMPLEMENTATIONS #
##############################
IMPLS := opt-pod
# IMPLS += unopt-pod
# IMPLS += opt-single
# IMPLS += unopt-single

EXECUTION_DIRS := $(foreach impl,$(IMPLS),$(BLACKSCHOLES_DIR)/$(impl))

include $(HB_HAMMERBENCH_PATH)/apps/common_complex.mk
