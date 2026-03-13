HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)

.PHONY: all
all: generate

# call to generate a test name
test-name = tile-x_$(1)__tile-y_$(2)__A-size_$(3)__warm-cache_$(4)

# call to get parameter from test name
get-tile-x = $(lastword $(subst _, ,$(filter tile-x_%,$(subst __, ,$(1)))))
get-tile-y = $(lastword $(subst _, ,$(filter tile-y_%,$(subst __, ,$(1)))))
get-A-size = $(lastword $(subst _, ,$(filter A-size_%,$(subst __, ,$(1)))))
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
	@echo tile-x = $(call get-tile-x,$*) >> $@
	@echo tile-y = $(call get-tile-y,$*) >> $@
	@echo A-size = $(call get-A-size,$*) >> $@
	@echo warm-cache = $(call get-warm-cache,$*) >> $@

include $(HB_HAMMERBENCH_PATH)/mk/testbench_common.mk

# regression
EXECUTION_DIRS := $(addprefix $(APP_PATH)/,$(TESTS_DIRS))
include $(HB_HAMMERBENCH_PATH)/apps/common_complex.mk

