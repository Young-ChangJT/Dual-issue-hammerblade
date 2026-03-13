HB_HAMMERBENCH_PATH = $(shell git rev-parse --show-toplevel)
TESTBENCH  ?= unnamed_benchmark
PARAMETERS ?= p0 p1 p2

.PHONY: all
all: $(TESTBENCH)

$(TESTBENCH): $(TESTBENCH)/Makefile
$(TESTBENCH): $(TESTBENCH)/template.mk
$(TESTBENCH): $(TESTBENCH)/app_path.mk
$(TESTBENCH): $(TESTBENCH)/tests.mk
$(TESTBENCH): $(TESTBENCH)/main.c
$(TESTBENCH): $(TESTBENCH)/kernel.cpp
$(TESTBENCH): $(TESTBENCH)/.gitignore

$(TESTBENCH)/Makefile: $(HB_HAMMERBENCH_PATH)/py/benchmark_makefile_gen.py
	mkdir -p $(dir $@)
	python3 $< $(PARAMETERS) > $@

$(TESTBENCH)/template.mk: $(HB_HAMMERBENCH_PATH)/mk/testbench_template.mk
	mkdir -p $(dir $@)
	cp $< $@

$(TESTBENCH)/app_path.mk:
	mkdir -p $(dir $@)
	echo 'APP_PATH = $$(HB_HAMMERBENCH_PATH)/apps/$(TESTBENCH)' > $@

$(TESTBENCH)/tests.mk: $(HB_HAMMERBENCH_PATH)/py/benchmark_tests_file_gen.py
	mkdir -p $(dir $@)
	python3 $< $(PARAMETERS) > $@

$(TESTBENCH)/main.c:
	mkdir -p $(dir $@)
	touch $@

$(TESTBENCH)/kernel.cpp:
	mkdir -p $(dir $@)
	touch $@

$(TESTBENCH)/.gitignore:
	mkdir -p $(dir $@)
	echo "*/" > $@
