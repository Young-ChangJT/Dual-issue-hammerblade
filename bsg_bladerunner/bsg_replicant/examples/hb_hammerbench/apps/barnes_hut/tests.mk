# Sampling pods;
POD_RANGE = $(shell seq -s " " 0 8 63)

# 16K
#$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,16384,$(x))))
# 32k
#$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,32768,$(x))))
# 64k
#$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,65536,$(x))))

# known-slowest pods;
TESTS += $(call test-name,16384,0)
TESTS += $(call test-name,32768,0)
TESTS += $(call test-name,65536,56)
