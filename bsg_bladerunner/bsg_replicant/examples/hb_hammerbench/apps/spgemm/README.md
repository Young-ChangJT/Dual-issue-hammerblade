```
How to run SPGEMM

- Make sure graph files (.mtx) are downloaded in hb_hammerbench/apps/graph_data/ directory.

- Go into inputs/ directory, and pre-process the graph files so that it can be used by the host program (CUDA-lite).

  Run 'make preprocess GRAPH_NAME={graph_name}' to generate those files.

  It should generate the following files.
  - {graph_name}.col_idx.txt
  - {graph_name}.nnz.txt
  - {graph_name}.row_offset.txt
  - {graph_name}.output_col_idx.txt
  - {graph_name}.output_nnz.txt
  - {graph_name}.output_row_offset.txt

```
