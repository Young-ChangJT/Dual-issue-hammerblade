POD_RANGE = $(shell seq -s " " 0 63)

# test
#TESTS += $(call test-name,test4,0)
#TESTS += $(call test-name,test8,0)

# wiki-Vote
#$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,wiki-Vote,$(x))))
# offshore
#$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,offshore,$(x))))
# road central
#$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,road-central,$(x))))
# roadNet CA
#$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,roadNet-CA,$(x))))


# known-slow pods;
TESTS += $(call test-name,wiki-Vote,0)
TESTS += $(call test-name,offshore,51)
TESTS += $(call test-name,roadNet-CA,18)
TESTS += $(call test-name,road-central,16)
