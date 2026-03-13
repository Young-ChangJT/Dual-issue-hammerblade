import sys
from statistics import stdev, mean

mtxfile = sys.argv[1]
graph_name = mtxfile.replace(".mtx", "")

src_to_dst = {}

# mtx attribute
symmetric = False
V = None
E = None

# parse mtx file;
with open(mtxfile, "r") as file0:
  first_line = file0.readline().strip()
  if first_line.startswith("%%"):
    words = first_line.split()
    if "symmetric" in words:
      symmetric = True
  else:
    print("Error: the first line does not start with %%.")
    sys.exit(1)

  VE_found = False

  for line in file0:
    stripped = line.strip()
    if stripped.startswith("%"):
      continue
    else:
      if VE_found:
        words = stripped.split()
        src = int(words[0])
        dst = int(words[1])
        src -= 1
        dst -= 1
        src_to_dst[src].append(dst)
        if symmetric and (src != dst):
          src_to_dst[dst].append(src)
      else:
        VE_found = True
        words = stripped.split()
        V0 = int(words[0])
        V1 = int(words[1])

        if V0 != V1:
          print("Error: V0, V1 do not match")
          sys.exit(1)
        else:
          V = V0

        for v in range(V):
          src_to_dst[v] = []


# count edges;
e0 = 0
for v in range(V):
  e0 += len(src_to_dst[v])
E = e0

print("V={}".format(V))
print("E={}".format(E))


# sort
for v in range(V):
  src_to_dst[v].sort()




NUMPODS=64
NUMPODS_X=8
NUMPODS_Y=8

# partition by row (vertex);
def partition_by_row_vertex(src_to_dst, V):
  V_per_pod = (V+NUMPODS-1)//NUMPODS
  psum = [0]*NUMPODS
  for pod in range(NUMPODS):
    v_start = min(pod*V_per_pod, V)
    v_end = min(v_start+V_per_pod, V)
    for v in range(v_start, v_end):
      for dst in src_to_dst[v]:
        psum[pod] += len(src_to_dst[dst])
    print("pod={}, psum={}".format(pod, psum[pod]))
  print("max={}".format(max(psum)))
  print("stdev={}".format(stdev(psum)))
  print("mean={}".format(mean(psum)))
  return

def partition_by_grid(src_to_dst, V):
  V_per_grid = (V+NUMPODS_X-1)//NUMPODS_X
  psum = [0]*NUMPODS
  for pod in range(NUMPODS):
    pod_x = pod%8
    pod_y = pod//8
    vx_start = min(pod_x*V_per_grid, V)
    vx_end = min(vx_start+V_per_grid, V)
    vy_start = min(pod_y*V_per_grid, V)
    vy_end = min(vy_start+V_per_grid, V)
    for vy in range(vy_start, vy_end):
      for dst in src_to_dst[vy]:
        for c in src_to_dst[dst]:
          if c >= vx_start and c < vx_end:
            psum[pod] += 1
    print("pod={}, psum={}".format(pod, psum[pod]))
  print("max={}".format(max(psum)))
  print("stdev={}".format(stdev(psum)))
  print("mean={}".format(mean(psum)))

partition_by_row_vertex(src_to_dst, V)
partition_by_grid(src_to_dst, V)
