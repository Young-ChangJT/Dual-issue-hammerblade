#########################################################
# CHANGE ME: TESTS				        #
# TESTS += $(call test-name,[buffer-size],[warm-cache]) #
#########################################################
#TESTS += $(call test-name,131072,yes)
#MSIZE = 8192
#MSIZE = 522488

#TESTS += $(call test-name,4,2,131072,no)
#TESTS += $(call test-name,4,4,131072,no)
#TESTS += $(call test-name,8,4,262144,no)
#TESTS += $(call test-name,8,8,262144,no)
#TESTS += $(call test-name,16,8,524288,no)
#TESTS += $(call test-name,16,16,524288,no)

#MSIZE = 524288
#MSIZE = 32768
#MSIZE = 65536
MSIZE = 261244
#TESTS += $(call test-name,4,2,$(MSIZE),no)
#TESTS += $(call test-name,4,4,$(MSIZE),no)
#TESTS += $(call test-name,8,4,$(MSIZE),no)
#TESTS += $(call test-name,8,8,$(MSIZE),no)
TESTS += $(call test-name,16,8,$(MSIZE),no)
#TESTS += $(call test-name,16,16,$(MSIZE),no)
#TESTS += $(call test-name,32,8,$(MSIZE),no)
