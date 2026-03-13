import sys

graph_name = sys.argv[1]
V = int(sys.argv[2])

# load distance data;
distances = []
with open("{}.distance.txt".format(graph_name), "r") as f0:
  for line in f0:
    dist = int(line.strip())
    distances.append(dist)


max_distance = max(distances)
dist_count = [0] * (max_distance+1)
for d in distances:
  dist_count[d] += 1

total_overhead = 0
for i in range(max_distance+1):
  count = dist_count[i]
  # for 8x8, 16x8 pods
  average_hop = 50
  overhead = (count*2*average_hop/(16*32)) + (V/32/64/0.8/8)
  total_overhead += overhead
  print("d={}, frontier_size={}, overhead={}".format(i, count, overhead))

print("total_overhead = {}".format(total_overhead))


