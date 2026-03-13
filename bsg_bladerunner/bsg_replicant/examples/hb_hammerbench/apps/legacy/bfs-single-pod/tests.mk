####################
# CHANGE ME: TESTS #
####################
# TESTS += $(call test-name,[input],[start],[opt-fwd-ilp-innner],[opt-mu-ilp-inner],[opt-rev-pre-outer])
# TESTS += $(call test-name,g7k1,122,1,1,1)
# TESTS += $(call test-name,u7k1,99,1,1,1)
# TESTS += $(call test-name,g10k8,812,1,1,1)
# TESTS += $(call test-name,u10k8,916,1,1,1)
# TESTS += $(call test-name,u16k16,44337,1,1,1)

# TESTS += $(call test-name,g16k16,61526,1,1,1)
# TESTS += $(call test-name,g16k16,61526,1,2,1)
# TESTS += $(call test-name,g16k16,61526,1,4,1)

TESTS += $(call test-name,g16k16,61526,1,2,1)
TESTS += $(call test-name,g16k16,61526,1,2,2)
TESTS += $(call test-name,g16k16,61526,1,2,4)

TESTS += $(call test-name,wiki-Vote,2,1,2,4)
