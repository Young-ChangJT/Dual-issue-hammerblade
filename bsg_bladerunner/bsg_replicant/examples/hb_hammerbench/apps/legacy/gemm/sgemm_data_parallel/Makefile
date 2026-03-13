HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)

.PHONY: all
all: generate


# call to generate a test name
test-name = size-m_$(1)__size-n_$(2)__size-p_$(3)__warm-cache_$(4)__prefetch_$(5)

# call to get parameter from test name
get-size-m= $(lastword $(subst _, ,$(filter size-m_%,$(subst __, ,$(1)))))
get-size-n= $(lastword $(subst _, ,$(filter size-n_%,$(subst __, ,$(1)))))
get-size-p= $(lastword $(subst _, ,$(filter size-p_%,$(subst __, ,$(1)))))
get-warm-cache= $(lastword $(subst _, ,$(filter warm-cache_%,$(subst __, ,$(1)))))
get-prefetch= $(lastword $(subst _, ,$(filter prefetch_%,$(subst __, ,$(1)))))

# defines tests
TESTS =
include tests.mk

TESTS_DIRS = $(TESTS)

$(addsuffix /parameters.mk,$(TESTS_DIRS)): %/parameters.mk:
	@echo Creating $@
	@mkdir -p $(dir $@)
	@touch $@
	@echo test-name  = $* >> $@
	@echo size-m  = $(call get-size-m,$*) >> $@
	@echo size-n  = $(call get-size-n,$*) >> $@
	@echo size-p  = $(call get-size-p,$*) >> $@
	@echo warm-cache = $(call get-warm-cache,$*) >> $@
	@echo prefetch = $(call get-prefetch,$*) >> $@

include $(HB_HAMMERBENCH_PATH)/mk/testbench_common.mk

# for regression
EXECUTION_DIRS := $(addprefix $(APP_PATH)/,$(TESTS_DIRS))
include $(HB_HAMMERBENCH_PATH)/apps/common_complex.mk
