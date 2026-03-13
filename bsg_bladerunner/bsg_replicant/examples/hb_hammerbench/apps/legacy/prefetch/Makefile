HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)

.PHONY: all
all: generate

# call to generate a test name
test-name = buffer-size_$(1)

# call to get parameter from test name
get-buffer-size = $(lastword $(subst _, ,$(filter buffer-size_%,$(subst __, ,$(1)))))

# defines tests
TESTS =
include tests.mk

TESTS_DIRS = $(TESTS)

$(addsuffix /parameters.mk,$(TESTS_DIRS)): %/parameters.mk:
	@echo Creating $@
	@mkdir -p $(dir $@)
	@touch $@
	@echo test-name  = $* >> $@
	@echo buffer-size = $(call get-buffer-size,$*) >> $@

include $(HB_HAMMERBENCH_PATH)/mk/testbench_common.mk

# for regression
EXECUTION_DIRS := $(addprefix $(APP_PATH)/,$(TESTS_DIRS))
include $(HB_HAMMERBENCH_PATH)/apps/common_complex.mk


