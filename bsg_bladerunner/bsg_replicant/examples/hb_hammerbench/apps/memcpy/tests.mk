#########################################################
# CHANGE ME: TESTS				        #
# TESTS += $(call test-name,[buffer-size],[warm-cache]) #
#########################################################
#MSIZE = 524288
MSIZE = 4096
TESTS += $(call test-name,16,8,$(MSIZE),no)
