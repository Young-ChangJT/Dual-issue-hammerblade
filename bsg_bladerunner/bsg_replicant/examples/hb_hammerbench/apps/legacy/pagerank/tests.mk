################################################################
# CHANGE ME: TESTS				               #
# TESTS += $(call test-name,[pull/push],[graph name],[pod-id]) #
################################################################
# These tests run relatively quickly:
# Social network-like

TESTS +=$(foreach i,$(shell seq -s" " 0 0),$(call test-name,pull,pagerank_pull_u8,wiki-Vote,$(i),1))

# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull_u8,wiki-Vote,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull_u8,p2p-Gnutella09,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull_u8,email-Enron,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull_u8,cit-HepTh,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull_u8,kron_g500-logn16,$(i),64))

# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,wiki-Vote,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,p2p-Gnutella09,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,email-Enron,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,cit-HepTh,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,kron_g500-logn16,$(i),64))

# Planar:
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,hi2010,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull_u8,hi2010,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,delaunay_n15,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull_u8,delaunay_n15,$(i),64))

#Random:
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,rgg_n_2_15_s0,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull_u8,rgg_n_2_15_s0,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,rgg_n_2_16_s0,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull_u8,rgg_n_2_16_s0,$(i),64))

# Slower Running:
# Social network-like
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,soc-Pokec,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,ljournal-2008,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,hollywood-2009,$(i),64))

# Planar:
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,roadNet-CA,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,road_central,$(i),64))
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,road_usa,$(i),64))

# Random:
# TESTS +=$(foreach i,$(shell seq -s" " 0 63),$(call test-name,pull,pagerank_pull,rgg_n_2_20_s0,$(i),64))


