HB_HAMMERBENCH_PATH ?= $(shell git rev-parse --show-toplevel)
include $(HB_HAMMERBENCH_PATH)/mk/environment.mk
include $(HB_HAMMERBENCH_PATH)/apps/bfs/inputs.mk

TEST_GRAPH_TYPE = wiki-Vote
TEST_VERTICES = $($(TEST_GRAPH_TYPE)__rows)
TEST_EDGES    = $($(TEST_GRAPH_TYPE)__nnz)
#TEST_ROOT_NODE    = 570170
TEST_ROOT_NODE    = 150
ITE_START     = 0
ITE_END = 0
MAX_POD = 63 
