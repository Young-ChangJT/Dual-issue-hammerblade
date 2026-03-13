HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)

.PHONY: all
all: generate

# test name
test-name = N_$(1)__NITER_$(2)


# Get parameters;
get-N     = $(lastword $(subst _, ,$(filter N_%,$(subst __, ,$(1)))))
get-NITER = $(lastword $(subst _, ,$(filter NITER_%,$(subst __, ,$(1)))))


# tests;
TESTS =
include tests.mk

TESTS_DIRS = $(TESTS)


$(addsuffix /parameters.mk,$(TESTS_DIRS)): %/parameters.mk:
	@echo Creating $@
	@mkdir -p $(dir $@)
	@touch $@
	@echo test-name  = $* >> $@
	@echo N     = $(call get-N,$*) >> $@
	@echo NITER = $(call get-NITER,$*) >> $@


include $(HB_HAMMERBENCH_PATH)/mk/testbench_common.mk

clean:
	rm -rf N_*NITER*
