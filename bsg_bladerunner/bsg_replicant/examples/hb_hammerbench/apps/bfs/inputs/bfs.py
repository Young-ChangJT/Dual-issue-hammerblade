import sys

graph_name = sys.argv[1]
root = int(sys.argv[2])
use_push_pull = (int(sys.argv[3]) == 1)

# read fwd offsets;
fwd_offsets = []
with open("{}.fwd_offsets.txt".format(graph_name)) as f0:
  for line in f0:
    offset = int(line.strip())
    fwd_offsets.append(offset)

# read fwd nonzeros;
fwd_nonzeros = []
with open("{}.fwd_nonzeros.txt".format(graph_name)) as f0:
  for line in f0:
    nz = int(line.strip())
    fwd_nonzeros.append(nz)

# read rev offsets;
rev_offsets = []
with open("{}.rev_offsets.txt".format(graph_name)) as f0:
  for line in f0:
    offset = int(line.strip())
    rev_offsets.append(offset)

# read rev nonzeros;
rev_nonzeros = []
with open("{}.rev_nonzeros.txt".format(graph_name)) as f0:
  for line in f0:
    nz = int(line.strip())
    rev_nonzeros.append(nz)


V = len(fwd_offsets)-1

direction = []
distance = [-1]*V
curr_frontier = [root]
distance[root] = 0
next_frontier = []
rev_not_fwd = 0
d=0

while curr_frontier:
  d += 1
  # decide direction;
  if use_push_pull:
    if rev_not_fwd:
      # heuristic
      rev_not_fwd = len(curr_frontier) >= V/20
    else:
      # number of edges going out from the frontier;
      mf = 0
      for src in curr_frontier:
        mf += fwd_offsets[src+1] - fwd_offsets[src]
      # number of edges going out from the unvisited;
      mu = 0
      for v in range(V):
        if distance[v] == -1:
          mu += fwd_offsets[v+1] - fwd_offsets[v]
      # heuristic
      rev_not_fwd = mf >= (mu/20)

  # traverse;
  direction.append(rev_not_fwd)
  if rev_not_fwd:
    for v in range(V):
      if distance[v] == -1:
        nz_start = rev_offsets[v]
        nz_end = rev_offsets[v+1]
        for nz in range(nz_start, nz_end):
          src = rev_nonzeros[nz]
          if distance[src] != -1:
            distance[v] = d
            next_frontier.append(v)
  else:
    for src in curr_frontier:
      nz_start = fwd_offsets[src]
      nz_end = fwd_offsets[src+1]
      for nz in range(nz_start, nz_end):
        dst = fwd_nonzeros[nz]
        if distance[dst] == -1:
          distance[dst] = d
          next_frontier.append(dst)

  # swap
  curr_frontier = next_frontier
  next_frontier = []


# write out distance;
with open("{}.distance.txt".format(graph_name), "w") as f0:
  for d in distance:
    f0.write("{}\n".format(d))
  
# write out direction;
with open("{}.direction.txt".format(graph_name), "w") as f0:
  for d in direction:
    if d:
      f0.write("1\n")
    else:
      f0.write("0\n")
