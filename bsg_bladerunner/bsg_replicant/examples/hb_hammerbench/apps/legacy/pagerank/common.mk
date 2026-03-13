# Run `make checkout_graphit`
HB_HAMMERBENCH_PATH:=$(shell git rev-parse --show-toplevel)
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk
PAGERANK_PATH:=$(HB_HAMMERBENCH_PATH)/apps/pagerank
GRAPHIT_PATH:=$(HB_HAMMERBENCH_PATH)/apps/pagerank/graphit
GAPBS_PATH:=$(HB_HAMMERBENCH_PATH)/apps/pagerank/gapbs

setup: $(GAPBS_PATH)/converter checkout_graphit 

.PHONY: checkout_graphit checkout_gapbs

checkout_graphit: $(GRAPHIT_PATH)
checkout_gapbs: $(GAPBS_PATH)

$(GRAPHIT_PATH):
	cd $(PAGERANK_PATH) && git clone git@github.com:bespoke-silicon-group/graphit
	cd $(GRAPHIT_PATH) && git checkout pagerank

$(GAPBS_PATH):
	cd $(PAGERANK_PATH) && git clone git@github.com:sbeamer/gapbs

$(GAPBS_PATH)/converter: | $(GAPBS_PATH)
	make -j -C $(GAPBS_PATH)

# Matrix setup:
%.el: %.mtx $(GAPBS_PATH)/converter
	$(GAPBS_PATH)/converter -f $< -e $@

cit-HepTh.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/SNAP/cit-HepTh.tar.gz"

wiki-Vote.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/SNAP/wiki-Vote.tar.gz"

rgg_n_2_15_s0.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/rgg_n_2_15_s0.tar.gz"

rgg_n_2_16_s0.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/rgg_n_2_16_s0.tar.gz"

rgg_n_2_20_s0.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/rgg_n_2_20_s0.tar.gz"

p2p-Gnutella09.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/SNAP/p2p-Gnutella09.tar.gz"

email-Enron.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/SNAP/email-Enron.tar.gz"

#soc-Pokec.tar.gz:
#	wget "https://suitesparse-collection-website.herokuapp.com/MM/SNAP/soc-Pokec.tar.gz"

hollywood-2009.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/LAW/hollywood-2009.tar.gz"

ljournal-2008.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/LAW/ljournal-2008.tar.gz"

roadNet-CA.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/SNAP/roadNet-CA.tar.gz"

road_central.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/road_central.tar.gz"

road_usa.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/road_usa.tar.gz"

kron_g500-logn16.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/kron_g500-logn16.tar.gz"

hi2010.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/hi2010.tar.gz"

delaunay_n15.tar.gz:
	wget "https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/delaunay_n15.tar.gz"

%.mtx: %.tar.gz
	tar -xf $< --strip-components 1 $(basename $@)/$@


all-graphs: wiki-Vote.el p2p-Gnutella09.el email-Enron.el hollywood-2009.el \
	road_usa.el road_central.el roadNet-CA.el ljournal-2008.el cit-HepTh.el \
	rgg_n_2_16_s0.el rgg_n_2_15_s0.el kron_g500-logn16.el hi2010.el delaunay_n15.el \
	rgg_n_2_20_s0.el
