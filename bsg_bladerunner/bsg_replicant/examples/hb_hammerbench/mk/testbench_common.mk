# include environment
HB_HAMMERBENCH_PATH ?= $(shell git rev-parse --show-toplevel)
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk

# define TESTS_DIR
TESTS_DIRS ?= $(TESTS)

###########################################
# rules for creating all test directories #
###########################################
# rules for {test name}/app_path.mk
$(addsuffix /app_path.mk,$(TESTS_DIRS)): %/app_path.mk: app_path.mk
	@echo Creating $@
	@mkdir -p $(dir $@)
	@cp $< $@

# rules for {test name}Makefile
$(addsuffix /Makefile,$(TESTS_DIRS)): %/Makefile: template.mk
	@echo Creating $@
	@mkdir -p $(dir $@)
	@cp template.mk $@

# rule for creating {test name}
$(TESTS_DIRS): %: %/app_path.mk
$(TESTS_DIRS): %: %/parameters.mk
$(TESTS_DIRS): %: %/Makefile

# generate all test directories
generate: $(TESTS_DIRS)

####################################################
# rules for running {test name}.{profile|exec|...} #
####################################################
$(addsuffix .profile,$(TESTS)): %.profile: %
	$(MAKE) -C $< profile.log

$(addsuffix .pc-histogram,$(TESTS)): %.pc-histogram: %
	$(MAKE) -C $< pc-histogram.log

$(addsuffix .exec,$(TESTS)): %.exec: %
	$(MAKE) -C $< exec.log

$(addsuffix .stats,$(TESTS)): %.stats: %
	$(MAKE) -C $< stats

$(addsuffix .pchistpdf,$(TESTS)): %.pchistpdf: %
	$(MAKE) -C $< pchistpdf

.PHONY: profile exec

####################################
# meta rules for running all tests #
####################################
pc-histogram: $(addsuffix .pc-histogram,$(TESTS))
profile: $(addsuffix .profile,$(TESTS))
exec:    $(addsuffix .exec,$(TESTS))
stats:	 $(addsuffix .stats,$(TESTS))
pchistpdf: $(addsuffix .pchistpdf,$(TESTS))

##############################
# purge all test directories #
##############################
purge:
	rm -rf $(TESTS_DIRS)

