import sys

mtxfile = sys.argv[1]
graph_name = mtxfile.replace(".mtx", "")

src_to_dst = {}
dst_to_src = {}

# mtx attributes;
symmetric = False

V = None
E = None

# Parse mtx file
with open(mtxfile, "r") as file0:

  # Parse the first line;
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
        if src == dst:
          continue
        src_to_dst[src].append(dst)
        dst_to_src[dst].append(src)
        if symmetric:
          src_to_dst[dst].append(src)
          dst_to_src[src].append(dst)
      else:
        VE_found = True
        words = stripped.split()
        V0 = int(words[0])
        V1 = int(words[1])
        #E  = int(words[2])
        #if symmetric:
        #  E *= 2
        if V0 != V1:
          print("Error: V0, V1 do not match")
          sys.exit(1)
        else:
          V = V0

        for v in range(V):
          src_to_dst[v] = []
          dst_to_src[v] = []

# count edges
e0 = 0
e1 = 0
for v in range(V):
  e0 += len(src_to_dst[v])
  e1 += len(dst_to_src[v])

if e0 != e1:
  print("Error: e0, e1 do not match.")
  sys.exit()
else:
  E = e0

print("V={}".format(V))
print("E={}".format(E))


# sort;     
for v in range(V):
  src_to_dst[v].sort()
  dst_to_src[v].sort()


# write fwd offsets / nonzeros;
fwd_offsets_file = open("{}.fwd_offsets.txt".format(graph_name), "w")
fwd_nonzeros_file = open("{}.fwd_nonzeros.txt".format(graph_name), "w")
offset = 0
for v in range(V):
  fwd_offsets_file.write(str(offset) + "\n")
  for nz in src_to_dst[v]:
    fwd_nonzeros_file.write(str(nz) + "\n")
    offset += 1
fwd_offsets_file.write(str(offset) + "\n")


# write rev offsets / nonzeros;
rev_offsets_file = open("{}.rev_offsets.txt".format(graph_name), "w")
rev_nonzeros_file = open("{}.rev_nonzeros.txt".format(graph_name), "w")
offset = 0
for v in range(V):
  rev_offsets_file.write(str(offset) + "\n")
  for nz in dst_to_src[v]:
    rev_nonzeros_file.write(str(nz) + "\n")
    offset += 1
rev_offsets_file.write(str(offset) + "\n")
