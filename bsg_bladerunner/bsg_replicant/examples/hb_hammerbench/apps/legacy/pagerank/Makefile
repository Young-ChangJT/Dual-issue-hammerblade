HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)

.PHONY: all
all: generate

# Call test-name to generate a test name
#   direction: [push,pull] (Only pull supported right now)
#   fn (function): Name of a function in pull/kernel.cpp or push/kernel.cpp
#   graph: Filename of a graph (without .el extension)
#   pod-id: Which pod to simulate
#   npods: How many pods is work distributed over
test-name = direction_$(1)__fn_$(2)__graph_$(3)__pod-id_$(4)__npods_$(5)

get-element = $(lastword $(subst _, ,$(filter $(1)_%,$(subst __, ,$(2)))))
# call these to get parameters from test name
get-direction = $(call get-element,direction,$(1))
get-npods = $(call get-element,npods,$(1))
get-pod-id = $(call get-element,pod-id,$(1))
# get-graph and get-function are special because some have underscores in their names
get-graph = $(lastword $(subst graph_, ,$(filter graph_%,$(subst __, ,$(1)))))
get-function = $(lastword $(subst fn_, ,$(filter fn_%,$(subst __, ,$(1)))))

# defines tests
TESTS =
include tests.mk

TESTS_DIRS =$(TESTS)

$(TESTS_DIRS):
	@echo Creating $@
	@mkdir -p $@

PULL_TEST_DIRS = $(filter direction_pull%,$(TESTS_DIRS))
PUSH_TEST_DIRS = $(filter direction_push%,$(TESTS_DIRS))

%/parameters.mk: %
$(addsuffix /parameters.mk,$(PULL_TEST_DIRS)): %/parameters.mk:
	@touch $@
	@echo test-name  = $* >> $@
	@echo num-pods=$(call get-npods,$*) >> $@
	@echo graph=$(call get-graph,$*) >> $@
	@echo pod-id=$(call get-pod-id,$*) >> $@
	@echo direction=$(call get-direction,$*) >> $@
	@echo function=\"$(call get-function,$*)\" >> $@

$(addsuffix /parameters.mk,$(PUSH_TEST_DIRS)): %/parameters.mk:
	@touch $@
	@echo test-name  = $* >> $@
	@echo num-pods=$(call get-npods,$*) >> $@
	@echo graph=$(call get-graph,$*) >> $@
	@echo pod-id=$(call get-pod-id,$*) >> $@
	@echo direction=$(call get-direction,$*) >> $@
	@echo function=\"$(call get-function,$*)\" >> $@

include $(HB_HAMMERBENCH_PATH)/mk/testbench_common.mk

# for regression
EXECUTION_DIRS := $(addprefix $(APP_PATH)/,$(TESTS_DIRS))
#include $(HB_HAMMERBENCH_PATH)/apps/common_complex.mk


