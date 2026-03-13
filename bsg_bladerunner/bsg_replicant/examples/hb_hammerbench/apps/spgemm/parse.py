import sys
import core_utilization as co
import numpy as np

# input arguments
graph         = sys.argv[1]
numpods       = int(sys.argv[2])

runtimes = []
max_runtime = 0
max_pid = 0

for pid in range(numpods):
  # parse
  filename = "graph_{}__pod-id_{}/vanilla_stats.csv".format(graph, pid)
  stats = co.parse_vanilla_stat(filename)
  if stats is None:
    continue
  curr_runtime = stats["runtime"]
  runtimes.append(curr_runtime)
  if max_runtime < curr_runtime:
    max_runtime = curr_runtime
    max_pid = pid
  print("pid={}, runtime={}".format(pid, curr_runtime))

print("max_pid={}, max_={}, average={}, stdev={}".format(max_pid, max_runtime, np.mean(runtimes), np.std(runtimes)))
