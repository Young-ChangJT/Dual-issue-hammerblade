HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)

.PHONY: all
all: generate


# test name;
test-name = input_$(1)__start_$(2)


# get parameter from test name;
get-input = $(lastword $(subst _, ,$(filter input_%,$(subst __, ,$(1)))))
get-start = $(lastword $(subst _, ,$(filter start_%,$(subst __, ,$(1)))))


# TESTS
TESTS =
include tests.mk

TESTS_DIRS = $(TESTS)

$(addsuffix /parameters.mk,$(TESTS_DIRS)): %/parameters.mk:
	@echo Creating $@
	@mkdir -p $(dir $@)
	@touch $@
	@echo test-name  = $* >> $@
	@echo input = $(call get-input,$*) >> $@
	@echo start = $(call get-start,$*) >> $@


include $(HB_HAMMERBENCH_PATH)/mk/testbench_common.mk
