HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)

.PHONY: all
all: generate


# test name
test-name = num-option_$(1)
get-num-option = $(lastword $(subst _, ,$(filter num-option_%,$(subst __, ,$(1)))))

# tests;
TESTS =
include tests.mk

TESTS_DIRS = $(TESTS)


$(addsuffix /parameters.mk,$(TESTS_DIRS)): %/parameters.mk:
	@echo Creating $@
	@mkdir -p $(dir $@)
	@touch $@
	@echo test-name  = $* >> $@
	@echo num-option = $(call get-num-option,$*) >> $@

include $(HB_HAMMERBENCH_PATH)/mk/testbench_common.mk

clean:
	rm -rf num-option*

