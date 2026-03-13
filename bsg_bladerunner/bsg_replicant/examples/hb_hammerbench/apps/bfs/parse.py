import sys
import core_utilization as co

# input arguments
graph         = sys.argv[1]
iter_start    = int(sys.argv[2])
iter_end      = int(sys.argv[3])
numpods       = int(sys.argv[4])


total_runtime = 0
for n in range(iter_start, iter_end):
  max_runtime = 0
  max_pid = 0
  for pid in range(numpods):
    # parse
    filename = "graph_{}__niter_{}__pod-id_{}/vanilla_stats.csv".format(graph, n, pid)
    stats = co.parse_vanilla_stat(filename)
    if stats is None:
      continue
    curr_runtime = stats["runtime"]
    if max_runtime < curr_runtime:
      max_runtime = curr_runtime
      max_pid = pid
      #print("pid={}, runtime={}".format(pid, stats["runtime"]))
  print("iter={}, max_pid={}, runtime={}".format(n, max_pid, max_runtime))
  total_runtime += max_runtime

print("total_runtime: {}, adjusted={}".format(total_runtime, total_runtime-((iter_end-iter_start)*512)))
