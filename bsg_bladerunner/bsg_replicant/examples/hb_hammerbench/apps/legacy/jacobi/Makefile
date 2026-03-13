HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)

.PHONY: all
all: generate

# call to generate a test name
test-name = nx_$(1)__ny_$(2)__nz_$(3)__num-iter_$(4)__warm-cache_$(5)

# call to get parameter from test name
get-nx = $(lastword $(subst _, ,$(filter nx_%,$(subst __, ,$(1)))))
get-ny = $(lastword $(subst _, ,$(filter ny_%,$(subst __, ,$(1)))))
get-nz = $(lastword $(subst _, ,$(filter nz_%,$(subst __, ,$(1)))))
get-num-iter   = $(lastword $(subst _, ,$(filter num-iter_%,$(subst __, ,$(1)))))
get-warm-cache = $(lastword $(subst _, ,$(filter warm-cache_%,$(subst __, ,$(1)))))

# defines tests
TESTS =
include tests.mk

TESTS_DIRS = $(TESTS)

$(addsuffix /parameters.mk,$(TESTS_DIRS)): %/parameters.mk:
	@echo Creating $@
	@mkdir -p $(dir $@)
	@touch $@
	@echo test-name  = $* >> $@
	@echo nx =  $(call get-nx,$*) >> $@ 
	@echo ny =  $(call get-ny,$*) >> $@ 
	@echo nz =  $(call get-nz,$*) >> $@
	@echo num-iter =  $(call get-num-iter,$*) >> $@
	@echo warm-cache = $(call get-warm-cache,$*) >> $@

include $(HB_HAMMERBENCH_PATH)/mk/testbench_common.mk

# for regression
EXECUTION_DIRS := $(addprefix $(APP_PATH)/,$(TESTS_DIRS))
include $(HB_HAMMERBENCH_PATH)/apps/common_complex.mk
