# The HammerBlade Benchmark Suite

HammerBench is a collection of parallel benchmarks ported for HammerBlade RISC-V Manycore. 


## How to Use This Repository


### Install
This repository is meant to be cloned into [bsg_replicant](https://github.com/bespoke-silicon-group/bsg_replicant) which is meant to be cloned into [bsg_bladerunner](https://github.com/bespoke-silicon-group/bsg_bladerunner).

For initial setup do the following:
1. Clone [bsg_bladerunner](https://github.com/bespoke-silicon-group/bsg_bladerunner) and follow the setup instructions.
2. From bsg_bladerunner, `cd bsg_replicant/examples; git clone git@github.com:bespoke-silicon-group/hb_hammerbench`


### Running a benchmark
- `apps/` directory contains all the benchmarks.
- Go into one of the benchmarks (`apps/sgemm`), and run `make generate`. This will generate some launch directories (e.g. N_512__NITER_2).
- From the launch directory, run `make profile.log`. This will launch the simulation and generate profiling data.


### Downloading sparse graph datasets
- `apps/graph_data` contains README on how to download and extract sparse graphs (.mtx).
- Currently, bfs, pagerank, and spgemm use these sparse graphs.
- These benchmarks have READMEs and makefile targets on how to preprocess the sparse graph.
