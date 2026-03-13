# compute directories relative to this testbench
BSG_MANYCORE_DIR ?= $(abspath ../..)
BASEJUMP_STL_DIR ?= $(abspath $(BSG_MANYCORE_DIR)/../basejump_stl)

INCDIR = +incdir+$(BSG_MANYCORE_DIR)/v/vanilla_bean
INCDIR += +incdir+$(BASEJUMP_STL_DIR)/bsg_misc

all: sim

sim:
	vcs +v2k -R +lint=all,noSVA-UA,noSVA-NSVU,noVCDE -sverilog -full64 -f sv.include \
		$(INCDIR) -debug_pp -timescale=1ps/1ps +vcs+vcdpluson -l vcs.log

dve:
	dve -full64 -vpd vcdplus.vpd &

clean:
	rm -rf DVEfiles csrc simv.daidir simv.vdb
	rm -f ucli.key vcdplus.vpd simv *.tar.gz vcs.log
	rm -rf stack.info.*
