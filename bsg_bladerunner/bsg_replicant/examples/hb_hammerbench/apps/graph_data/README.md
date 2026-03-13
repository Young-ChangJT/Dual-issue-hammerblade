```
Graph Data Directory

- A small test graph (u10k8.mtx) has been checked in the repo here.
  Type 'make u10k8.mtx' to decompress the file.

- For other large graphs, we have to download them from the source.
  Type 'make {graph_name}.mtx' to download and decompress files.
  These graphs are from https://sparse.tamu.edu/
  These graphs are very large, so download them only if needed...

- Following graphs can be download with the makefile targets;
    wiki-Vote
    offshore
    soc-Pokec
    ljournal-2008
    hollywood-2009
    roadNet-CA
    road-central
    road-usa

- Note: Don't use underscore '_' in the graph name (e.g. road_central, road_usa)
  It throws off the test directory naming convestion used in this repo.
  Replace '_' with '-'.

```
