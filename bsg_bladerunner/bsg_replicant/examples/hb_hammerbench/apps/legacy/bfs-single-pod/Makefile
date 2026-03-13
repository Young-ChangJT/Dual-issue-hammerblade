HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)

.PHONY: all
all: generate

# call to generate a test name
test-name = input_$(1)__start_$(2)__opt-fwd-ilp-inner_$(3)__opt-mu-ilp-inner_$(4)__opt-rev-pre-outer_$(5)

# call to get parameter from test name
get-input = $(lastword $(subst _, ,$(filter input_%,$(subst __, ,$(1)))))
get-start = $(lastword $(subst _, ,$(filter start_%,$(subst __, ,$(1)))))
get-opt-fwd-ilp-inner = $(lastword $(subst _, ,$(filter opt-fwd-ilp-inner_%,$(subst __, ,$(1)))))
get-opt-mu-ilp-inner = $(lastword $(subst _, ,$(filter opt-mu-ilp-inner_%,$(subst __, ,$(1)))))
get-opt-rev-pre-outer = $(lastword $(subst _, ,$(filter opt-rev-pre-outer_%,$(subst __, ,$(1)))))
# defines tests
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
	@echo opt-fwd-ilp-inner = $(call get-opt-fwd-ilp-inner,$*) >> $@
	@echo opt-mu-ilp-inner = $(call get-opt-mu-ilp-inner,$*) >> $@
	@echo opt-rev-pre-outer = $(call get-opt-rev-pre-outer,$*) >> $@

include $(HB_HAMMERBENCH_PATH)/mk/testbench_common.mk

# for regression
# EXECUTION_DIRS := (addprefix $(APP_PATH)/,$(TESTS_DIRS))
# include $(HB_HAMMERBENCH_PATH)/apps/common_complex.mk


