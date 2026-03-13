```
How to run Pagerank

- Make sure graph files (.mtx) are downloaded in hb_hammerbench/apps/graph_data/ directory.

- Go into inputs/ directory, and pre-process the graph files so that it can be used by the host program (CUDA-lite).

  Run 'make preprocess GRAPH_NAME={graph_name}' to generate those files.

  It should generate the following files.
  - {graph_name}.fwd_nonzeros.txt
  - {graph_name}.fwd_offsets.txt
  - {graph_name}.rev_nonzeros.txt
  - {graph_name}.rev_offsets.txt

```
