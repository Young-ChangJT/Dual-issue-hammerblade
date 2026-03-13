#########################################################
# CHANGE ME: TESTS				        #
# TESTS += $(call test-name,[nx],[ny],[nz],[num-iter],[warm-cache]) #
#########################################################
TESTS += $(call test-name,16,16,512,1,yes)
TESTS += $(call test-name,32,16,512,1,no)
#TESTS += $(call test-name,32,16,512,1,no)
