ifndef (HB_HAMMERBENCH_APPS_COMMON_COMPLEX_MK)
HB_HAMMERBENCH_APPS_COMMON_COMPLEX_MK=1

HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk

###########################################################
# This is for setup; should only be run when first cloned #
###########################################################
.PHONY: setup

###############
# Macro rules #
###############
MACRO_RULES := profile
MACRO_RULES += exec
MACRO_RULES += pc-histogram
MACRO_RULES += debug
MACRO_RULES += repl
MACRO_RULES += saifgen
MACRO_RULES += stats
MACRO_RULES += clean
.PHONY: $(MACRO_RULES)

###########################################################
# List of directories in which execution rules can be run #
###########################################################
EXECUTION_DIRS ?=

############################
# Helper textual functions #
############################
exec-dir-to-rule  = $(subst /,+--+,$1).$2
rule-to-exec-dir  = $(subst +--+,/,$(firstword $(subst ., ,$1)))
rule-to-rule-type = $(lastword $(subst ., ,$1))

########################################################
# for each run rule type			       #
# 	1. create list of rules for that type	       #
# 	2. set dependencies of the macro rule	       #
# 	3. add to list of all run rules		       #
########################################################

PROFILE_RULES := $(foreach dir,$(EXECUTION_DIRS),$(call exec-dir-to-rule,$(dir),profile))
profile: $(PROFILE_RULES)
RUN_RULES += $(PROFILE_RULES)

EXEC_RULES := $(foreach dir,$(EXECUTION_DIRS),$(call exec-dir-to-rule,$(dir),exec))
exec: $(EXEC_RULES)
RUN_RULES += $(EXEC_RULES)

PC-HISTOGRAM_RULES := $(foreach dir,$(EXECUTION_DIRS),$(call exec-dir-to-rule,$(dir),pc-histogram))
pc-histogram: $(PC-HISTOGRAM_RULES)
RUN_RULES += $(PC-HISTOGRAM_RULES)

DEBUG_RULES := $(foreach dir,$(EXECUTION_DIRS),$(call exec-dir-to-rule,$(dir),debug))
debug: $(DEBUG_RULES)
RUN_RULES += $(DEBUG_RULES)

REPL_RULES := $(foreach dir,$(EXECUTION_DIRS),$(call exec-dir-to-rule,$(dir),repl))
repl: $(REPL_RULES)
RUN_RULES += $(REPL_RULES)

SAIFGEN_RULES := $(foreach dir,$(EXECUTION_DIRS),$(call exec-dir-to-rule,$(dir),saifgen))
saifgen: $(SAIFGEN_RULES)
RUN_RULES += $(SAIFGEN_RULES)

########################################################
# for each post-processing rule type		       #
# 	1. create list of rules for that type	       #
# 	2. set dependencies of the macro rule	       #
# 	3. add to list of all post-processing rules    #
########################################################
STATS_RULES := $(foreach dir,$(EXECUTION_DIRS),$(call exec-dir-to-rule,$(dir),stats))
stats: $(STATS_RULES)
POSTPROCESSING_RULES += $(STATS_RULES)

CLEAN_RULES := $(foreach dir,$(EXECUTION_DIRS),$(call exec-dir-to-rule,$(dir),clean))
clean: $(CLEAN_RULES)
POSTPROCESSING_RULES += $(CLEAN_RULES)

#######################################################
# run rules follow the formula of running make in the #
# target directory for that rule's log file.	      #
# e.g.: run profile in aes/opt-pod		      #
# =>    $(MAKE) -C aes/opt-pod profile.log	      #
#######################################################

$(RUN_RULES):
	$(eval rule=$@)
	$(eval exec-dir=$(call rule-to-exec-dir,$@))
	$(eval rule-type=$(call rule-to-rule-type,$@))
	@echo exec-dir=$(exec-dir)
	@echo rule-type=$(rule-type)
	$(MAKE) -C $(exec-dir) $(rule-type).log

############################################################
# post-processing rules follow the formula of running make #
# in the target directory verbatim			   #
# e.g.: runs stats in aes/opt-pod			   #
# =>    $(MAKE) -C aes/opt-pod stats			   #
############################################################
$(POSTPROCESSING_RULES):
	$(eval rule=$@)
	$(eval exec-dir=$(call rule-to-exec-dir,$@))
	$(eval rule-type=$(call rule-to-rule-type,$@))
	$(MAKE) -C $(exec-dir) $(rule-type)

endif
