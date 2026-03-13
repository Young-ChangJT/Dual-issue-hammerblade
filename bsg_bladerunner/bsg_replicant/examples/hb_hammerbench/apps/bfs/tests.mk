# NUMPODS
POD_RANGE = $(shell seq -s " " 0 63)


# TEST graph u16k16
# d=1, rev_not_fwd=0, frontier_size=1, edge_traversed=32
# d=2, rev_not_fwd=0, frontier_size=32, edge_traversed=1091
# d=3, rev_not_fwd=0, frontier_size=1052, edge_traversed=34643
# d=4, rev_not_fwd=1, frontier_size=25858, edge_traversed=95775
# d=5, rev_not_fwd=1, frontier_size=38593, edge_traversed=0
#TEST_ITER_RANGE = $(shell seq -s " " 0 4)
#$(foreach n, $(TEST_ITER_RANGE), \
#	$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,u16k16,$(n),$(x)))))



# soc-Pokec
#PK_ITER_RANGE = $(shell seq -s " " 0 10)
#$(foreach n, $(PK_ITER_RANGE), \
	$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,soc-Pokec,$(n),$(x)))))

#hollywood-2009
#HW_ITER_RANGE = $(shell seq -s " " 0 7)
#$(foreach n, $(HW_ITER_RANGE), \
	$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,hollywood-2009,$(n),$(x)))))

#ljournal-2008
#LJ_ITER_RANGE = $(shell seq -s " " 0 13)
#$(foreach n, $(LJ_ITER_RANGE), \
	$(foreach x, $(POD_RANGE), $(eval TESTS += $(call test-name,ljournal-2008,$(n),$(x)))))

#roadnet CA
#CA_ITER_RANGE = $(shell seq -s " " 120 135)
#CA_POD_RANGE = $(shell seq -s " " 0 63)
#$(foreach n, $(CA_ITER_RANGE), \
	$(foreach x, $(CA_POD_RANGE), $(eval TESTS += $(call test-name,roadNet-CA,$(n),$(x)))))

#road_central (iter ~4167)
#RC_ITER_RANGE = $(shell seq -s " " 1613 1628)
#RC_POD_RANGE = $(shell seq -s " " 0 63)
#$(foreach n, $(RC_ITER_RANGE), \
	$(foreach x, $(RC_POD_RANGE), $(eval TESTS += $(call test-name,road-central,$(n),$(x)))))

#road_usa (iter ~ 4611)
#US_ITER_RANGE = $(shell seq -s " " 960 975)
#US_POD_RANGE = $(shell seq -s " " 0 63)
#$(foreach n, $(US_ITER_RANGE), \
	$(foreach x, $(US_POD_RANGE), $(eval TESTS += $(call test-name,road-usa,$(n),$(x)))))


# known-slow pods;
TESTS += $(call test-name,soc-Pokec,4,36)
TESTS += $(call test-name,hollywood-2009,2,9)
TESTS += $(call test-name,ljournal-2008,4,61)
TESTS += $(call test-name,roadNet-CA,131,0)
TESTS += $(call test-name,road-central,1625,0)
TESTS += $(call test-name,road-usa,968,0)
