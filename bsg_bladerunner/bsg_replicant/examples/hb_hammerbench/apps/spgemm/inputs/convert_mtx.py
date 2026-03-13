import sys

mtxfile = sys.argv[1]
graph_name = mtxfile.replace(".mtx", "")

src_to_dst = {}

# mtx attributes;
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


# row offset
# col idx
# nnz
row_offset_file = open("{}.row_offset.txt".format(graph_name), "w")
col_idx_file    = open("{}.col_idx.txt".format(graph_name), "w")
nnz_file        = open("{}.nnz.txt".format(graph_name), "w")
offset = 0

for v in range(V):
  row_offset_file.write(str(offset) + "\n")
  for idx in src_to_dst[v]:
    col_idx_file.write(str(idx) + "\n")
    nnz_file.write(str(1) + "\n") # every nnz = 1
    offset += 1
row_offset_file.write(str(offset) + "\n")



# Do spgemm; AxA;
output_row_offset_file = open("{}.output_row_offset.txt".format(graph_name), "w")
output_col_idx_file    = open("{}.output_col_idx.txt".format(graph_name), "w")
output_nnz_file        = open("{}.output_nnz.txt".format(graph_name), "w")
offset = 0

max_output_row_size = 0
max_output_row = 0
for v in range(V):
  psum = {}
  for row in src_to_dst[v]:
    for col in src_to_dst[row]:
      if col in psum:
        psum[col] += 1
      else:
        psum[col] = 1

  if len(psum) > max_output_row_size:
    max_output_row_size = len(psum)
    max_output_row = v
  
  output_row_offset_file.write(str(offset) + "\n")
  for k in sorted(psum.keys()):
    output_col_idx_file.write(str(k) + "\n")
    output_nnz_file.write(str(psum[k]) + "\n")
    offset += 1
       
output_row_offset_file.write(str(offset) + "\n")
print("OUTPUT_E={}".format(offset))
print("MAX_OUTPUT_ROW_SIZE={}".format(max_output_row_size))
print("MAX_OUTPUT_ROW={}".format(max_output_row))
