ifndef (HB_HAMMERBENCH_APPS_COMMON_SIMPLE_MK)
HB_HAMMERBENCH_APPS_COMMON_SIMPLE_MK=1

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

profile exec pc-histogram debug repl saifgen: %: %.log

endif
