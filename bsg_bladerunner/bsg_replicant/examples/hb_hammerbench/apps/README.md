```
Directory Structure


- {aes, barnes_hut, bfs, blackscholes, fft, jacobi, pagerank, sgemm, smithwaterman, spgemm}
    The latest implementation of ten parallel benchmarks used for the ISCA paper.
    It supports both single and two-pod simulations.
    Use only these for benchmarking purposes.


- {memcpy, vector_add}
    Microbenchmarks used for tutorials.


- {common}
    Some common library functions.


- {graph_data}
    graph files (.mtx) used by bfs, pagerank, and spgemm.


- {legacy}
    The old implementation of some benchmarks and some experiments. These are not supported.
```
