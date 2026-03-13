#include <bsg_manycore_tile.h>
#include <bsg_manycore_errno.h>
#include <bsg_manycore_tile.h>
#include <bsg_manycore_loader.h>
#include <bsg_manycore_cuda.h>
#include <bsg_manycore_regression.h>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <set>
#include <fstream>
#include "host_bfs.hpp"

#define ALLOC_NAME "default_allocator"


void read_graph(int* ptr, std::string filename)
{
  // allocate mem;
  //ptr = (int *) malloc(sizeof(int)*len);

  std::ifstream fh(filename);
  int idx = 0;
  int num;
   
  while (fh >> num) {
    //printf("%d\n",num);
    ptr[idx] = num;
    idx++;
  }
}


void read_vector(std::vector<int> &vec, std::string filename)
{
  std::ifstream fh(filename);
  int num;
  while (fh >> num) {
    vec.push_back(num);
  }
}


int bfs_multipod(int argc, char ** argv)
{
  int r = 0;

  // Command Line arguments;
  const char * bin_path = argv[1];
  std::string fwd_offsets_file  = argv[2];
  std::string fwd_nonzeros_file = argv[3];
  std::string rev_offsets_file  = argv[4];
  std::string rev_nonzeros_file = argv[5];
  std::string distance_file  = argv[6];
  std::string direction_file = argv[7];
  
  std::cout << "bin_path = " << bin_path << std::endl;
  std::cout << "fwd_offsets_file = "  << fwd_offsets_file << std::endl;
  std::cout << "fwd_nonzeros_file = " << fwd_nonzeros_file << std::endl;
  std::cout << "rev_offsets_file = "  << rev_offsets_file << std::endl;
  std::cout << "rev_nonzeros_file = " << rev_nonzeros_file << std::endl;

  // Define Params;
  int root = ROOT;
  int niter= NITER;
  int pod_id = PODID;
  int numpods = NUMPODS;
  std::cout << "root = " << root << std::endl;
  std::cout << "niter = " << niter << std::endl;
  std::cout << "pod-id = " << pod_id << std::endl;
  std::cout << "numpods = " << numpods << std::endl;


  // Read input graph;
  int V = VERTEX;
  int E = EDGE;
  std::cout << "V = " << V << std::endl;
  std::cout << "E = " << E << std::endl;
  int * fwd_offsets = (int *) malloc(sizeof(int)*(V+1));
  int * fwd_nonzeros = (int *) malloc(sizeof(int)*(E));
  int * rev_offsets = (int *) malloc(sizeof(int)*(V+1));
  int * rev_nonzeros = (int *) malloc(sizeof(int)*(E));
  read_graph(fwd_offsets,  fwd_offsets_file);
  read_graph(fwd_nonzeros,  fwd_nonzeros_file);
  read_graph(rev_offsets,  rev_offsets_file);
  read_graph(rev_nonzeros,  rev_nonzeros_file);

  // BFS on host;
  std::vector<int> distance;
  std::vector<int> direction;
  read_vector(distance, distance_file);
  read_vector(direction, direction_file);
  //host_bfs(root, V, fwd_offsets, fwd_nonzeros, rev_offsets, rev_nonzeros, distance, direction);


  // frontiers;
  std::vector<int> curr_frontier;
  for (int i = 0; i < V; i++) {
    if (distance[i] == niter) {
      curr_frontier.push_back(i);
    }
  }
  printf("curr_frontier.size = %d\n", (uint32_t) curr_frontier.size());
 
  // curr  distance
  std::vector<int> curr_distance(V, -1);
  for (int i = 0; i < V; i++) {
    if (distance[i] <= niter) {
      curr_distance[i] = distance[i];
    }
  }

  // dense frontier size;
  int num_dense_words = (V+31)/32;
  int rem = num_dense_words%16;
  if (rem != 0) {
    num_dense_words += 16-rem;
  }

  int *next_dense_frontier = (int*) malloc(num_dense_words*sizeof(int));
  for (int i = 0; i < num_dense_words; i++) {
    next_dense_frontier[i] = 0;
  }


  // Initialize device;
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, "bfs_multipod", HB_MC_DEVICE_ID));

 
  // Pod
  hb_mc_pod_id_t pod;
 
  hb_mc_eva_t d_offsets;
  hb_mc_eva_t d_nonzeros;
  hb_mc_eva_t d_curr_frontier;
  hb_mc_eva_t d_next_dense_frontier;
  hb_mc_eva_t d_curr_distance;

  hb_mc_device_foreach_pod_id(&device, pod)
  {
    // Loading program;
    printf("Loading program for pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

    // Allocate memory on device;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (V+1)*sizeof(int), &d_offsets));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, E*sizeof(int), &d_nonzeros));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, curr_frontier.size()*sizeof(int), &d_curr_frontier));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, num_dense_words*sizeof(int), &d_next_dense_frontier));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, curr_distance.size()*sizeof(int), &d_curr_distance));

    // DMA transfer;   
    printf("Transferring data: pod %d\n", pod);
    std::vector<hb_mc_dma_htod_t> htod_job;
    if (direction[niter]) {
      htod_job.push_back({d_offsets, rev_offsets, (V+1)*sizeof(int)});
      htod_job.push_back({d_nonzeros, rev_nonzeros, E*sizeof(int)});
    } else {
      htod_job.push_back({d_offsets, fwd_offsets, (V+1)*sizeof(int)});
      htod_job.push_back({d_nonzeros, fwd_nonzeros, E*sizeof(int)});
    }
    htod_job.push_back({d_curr_frontier, curr_frontier.data(), curr_frontier.size()*sizeof(int)});
    htod_job.push_back({d_next_dense_frontier, next_dense_frontier, num_dense_words*sizeof(int)});
    htod_job.push_back({d_curr_distance, curr_distance.data(), curr_distance.size()*sizeof(int)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_device(&device, htod_job.data(), htod_job.size()));

    // CUDA args
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 9
    uint32_t cuda_argv[CUDA_ARGC] = {
      // inputs
      pod_id+(uint32_t)pod,
      V,
      direction[niter],
      d_offsets,
      d_nonzeros,
      d_curr_distance,
      d_curr_frontier,
      (uint32_t) curr_frontier.size(),
      // output
      d_next_dense_frontier
    };
    
    // Enqueue kernel
    printf("Enqueing kernel: pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_kernel_enqueue(&device, grid_dim, tg_dim, "kernel", CUDA_ARGC, cuda_argv));
  }


  // Launch pods
  printf("Launching all pods\n");
  BSG_CUDA_CALL(hb_mc_device_pods_kernels_execute(&device));


  // Read frontier data;
  std::set<int> next_frontier;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    printf("Reading next frontier data: pods %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));

    // clear buffer
    for (int i = 0; i < num_dense_words; i++) {
      next_dense_frontier[i] = 0;
    }

    // DMA
    std::vector<hb_mc_dma_dtoh_t> dtoh_job;
    dtoh_job.push_back({d_next_dense_frontier, next_dense_frontier, num_dense_words*sizeof(int)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_host(&device, dtoh_job.data(), dtoh_job.size()));

    // collect frontier
    for (int i = 0; i < num_dense_words; i++) {
      for (int j = 0; j < 32; j++) {
        int bit = (next_dense_frontier[i] >> j) & 1;
        if (bit) {
          next_frontier.insert((32*i)+j);
        }
      }
    }
  }

  // Expected next frontier;
  std::set<int> expected_next_frontier;


  hb_mc_device_foreach_pod_id(&device, pod) {
    int curr_pod_id = pod_id+(int)pod;
    if (direction[niter]) {
      // REV
      int V_per_pod = (V+numpods-1)/numpods;
      int V_start = std::min(V, curr_pod_id*V_per_pod);
      int V_end = std::min(V, V_start+V_per_pod);
      printf("Calculating expected next frontier: REV, curr_pod_id=%d, v_start=%d, v_end=%d\n", curr_pod_id, V_start, V_end);

      // Iterate through unvisted
      for (int v = V_start; v < V_end; v++) {
        if (curr_distance[v] == -1) {
          int nz_start = rev_offsets[v];
          int nz_end = rev_offsets[v+1];
          for (int nz = nz_start; nz < nz_end; nz++) {
            int src = rev_nonzeros[nz];
            if (curr_distance[src] != -1) {
              expected_next_frontier.insert(v);
              break;
            }
          }
        }
      }

    } else {
      // FWD
      int frontier_per_pod = (curr_frontier.size()+numpods-1) / numpods;
      int f_start = std::min((int) curr_frontier.size(), curr_pod_id*frontier_per_pod);
      int f_end   = std::min((int) curr_frontier.size(), f_start+frontier_per_pod);
      printf("Calculating expected next frontier: FWD, curr_pod_id=%d, f_start=%d, f_end=%d\n", curr_pod_id, f_start, f_end);

      for (int f = f_start; f < f_end; f++) {
        int src = curr_frontier[f];
        int nz_start = fwd_offsets[src];
        int nz_end = fwd_offsets[src+1];
        for (int nz = nz_start; nz < nz_end; nz++) {
          int dst = fwd_nonzeros[nz];
          if (curr_distance[dst] == -1) {
            expected_next_frontier.insert(dst);
          }
        }
      }
    }
  }


  // validate
  bool fail = false;

  // is expected in actual?
  std::set<int>::iterator it;
  for (it = expected_next_frontier.begin(); it != expected_next_frontier.end(); it++) {
    int val = *it;
    if (next_frontier.find(val) == next_frontier.end()) {
      printf("FAIL: Frontier %d not in actual.\n", val);
      fail = true;
    }
  }

  // is actual in expected?
  for (it = next_frontier.begin(); it != next_frontier.end(); it++) {
    int val = *it;
    if (expected_next_frontier.find(val) == expected_next_frontier.end()) {
      printf("FAIL: Frontier %d not in expected.\n", val);
      fail = true;
    }
  }
  // Finish;
  BSG_CUDA_CALL(hb_mc_device_finish(&device));

  if (fail) {
    return HB_MC_FAIL;
  } else {
    return HB_MC_SUCCESS;
  }
  
}



declare_program_main("bfs-multipod", bfs_multipod);

