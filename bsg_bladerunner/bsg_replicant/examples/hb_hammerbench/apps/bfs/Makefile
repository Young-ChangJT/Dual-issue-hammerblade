HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)

.PHONY: all
all: generate


# test name;
test-name = graph_$(1)__niter_$(2)__pod-id_$(3)


# get parameter from test name;
get-graph  = $(lastword $(subst _, ,$(filter graph_%,$(subst __, ,$(1)))))
get-niter  = $(lastword $(subst _, ,$(filter niter_%,$(subst __, ,$(1)))))
get-pod-id = $(lastword $(subst _, ,$(filter pod-id_%,$(subst __, ,$(1)))))

# TESTS
TESTS =
include tests.mk

TESTS_DIRS = $(TESTS)

$(addsuffix /parameters.mk,$(TESTS_DIRS)): %/parameters.mk:
	@echo Creating $@
	@mkdir -p $(dir $@)
	@touch $@
	@echo test-name  = $* >> $@
	@echo graph  = $(call get-graph,$*) >> $@
	@echo niter  = $(call get-niter,$*) >> $@
	@echo pod-id = $(call get-pod-id,$*) >> $@


include $(HB_HAMMERBENCH_PATH)/mk/testbench_common.mk


clean:
	rm -rf graph_*pod-id*
