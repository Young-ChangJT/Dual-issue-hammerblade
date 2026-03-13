#include <bsg_manycore_tile.h>
#include <bsg_manycore_errno.h>
#include <bsg_manycore_tile.h>
#include <bsg_manycore_loader.h>
#include <bsg_manycore_cuda.h>
#include <bsg_manycore_regression.h>
#include <string>
#include <iostream>
#include <vector>
#include "read_graph.hpp"
#include "transpose_graph.hpp"
#include "breadth_first_search_graph.hpp"


#define ALLOC_NAME "default_allocator"



template <typename T>
int set_symbol(hb_mc_device_t *devp, const std::string &symbol, T val) {
    hb_mc_eva_t addr;
    hb_mc_pod_t *pod = &devp->pods[devp->default_pod_id];
    hb_mc_program_t *prog = pod->program;
    BSG_CUDA_CALL(hb_mc_loader_symbol_to_eva(prog->bin, prog->bin_size, symbol.c_str(), &addr));
    BSG_CUDA_CALL(hb_mc_device_memcpy_to_device(devp, addr, &val, sizeof(T)));
    return HB_MC_SUCCESS;
}



/*
int malloc_pod_cache_aligned(hb_mc_device_t *device,
                             int size,
                             hb_mc_eva_t *datap,
                             hb_mc_eva_t*freeablep) {
    hb_mc_eva_t ptr;
    hb_mc_manycore_t *mc = device->mc;
    hb_mc_config_t *cfg = &mc->config;
    int align = sizeof(int)*cfg->vcache_block_words*cfg->pod_shape.x*2;
    int alloc_size = size  + align;
    BSG_CUDA_CALL(hb_mc_device_malloc(device, alloc_size, &ptr));
    hb_mc_eva_t rem = ptr % align;
    *datap = ptr - rem + align;
    // set outputs
    if (datap != nullptr)
        *datap = ptr - rem + align;
    if (freeablep != nullptr)
        *freeablep = ptr;
    return HB_MC_SUCCESS;
}
*/



int bfs_single_pod(int argc, char ** argv)
{
  int r;

  // Command line arguments;
  // [1] bin path
  // [2] graph input
  // [3] start
  const char * bin_path    = argv[1];
  std::string graph_input = argv[2];
  int root = atoi(argv[3]);

  std::cout << "bin_path = " << bin_path << std::endl;
  std::cout << "graph_input = " << graph_input << std::endl;
  std::cout << "root = " << root << std::endl;

  // Read input graph;
  int V, E;
  std::vector<int> fwd_offsets, fwd_nonzeros;
  read_graph(graph_input, &V, &E, fwd_offsets, fwd_nonzeros);

  // Transpose;
  std::vector<int> rev_offsets, rev_nonzeros;
  BSG_CUDA_CALL(transpose_graph(V, E, fwd_offsets, fwd_nonzeros, rev_offsets, rev_nonzeros));

  // distance;
  std::vector<int> distance(V, -1);

  // Find a solution
  std::vector<int> solution_distance;
  breadth_first_search_graph(root, V, fwd_offsets, fwd_nonzeros, rev_offsets, rev_nonzeros, solution_distance);

  // Initialize device;
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, "bfs_single_pod", 0));


  // Pod
  hb_mc_pod_id_t pod;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    bsg_pr_info("Loading program for pod %d\n.", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

    // Allocate memory on device;
    // fwd_offsets
    hb_mc_eva_t d_fwd_offsets;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, fwd_offsets.size()*sizeof(int), &d_fwd_offsets));
    // fwd_nonzeros
    hb_mc_eva_t d_fwd_nonzeros;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, fwd_nonzeros.size()*sizeof(int), &d_fwd_nonzeros));
    // rev_offsets
    hb_mc_eva_t d_rev_offsets;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, rev_offsets.size()*sizeof(int), &d_rev_offsets));
    // rev_nonzeros
    hb_mc_eva_t d_rev_nonzeros;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, rev_nonzeros.size()*sizeof(int), &d_rev_nonzeros));
    // curr frontier
    hb_mc_eva_t d_curr_frontier;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, V*sizeof(int), &d_curr_frontier));
    // dense frontier (curr and next)
    hb_mc_eva_t d_next_dense_frontier;
    hb_mc_eva_t d_curr_dense_frontier;
    int num_dense_words = (V+31)/32;
    int rem = num_dense_words%16;
    if (rem != 0) {
      num_dense_words += 16-rem;
    }
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, num_dense_words*sizeof(int), &d_curr_dense_frontier));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, num_dense_words*sizeof(int), &d_next_dense_frontier));
    // distance
    hb_mc_eva_t d_distance;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, V*sizeof(int), &d_distance));


    // dma transfers
    std::vector<hb_mc_dma_htod_t> htod_job;
    //  xfer fwd_offsets
    htod_job.push_back({d_fwd_offsets, fwd_offsets.data(), fwd_offsets.size()*sizeof(int)});
    //  xfer fwd_nonzeros
    htod_job.push_back({d_fwd_nonzeros, fwd_nonzeros.data(), fwd_nonzeros.size()*sizeof(int)});
    //  xfer rev_offsets
    htod_job.push_back({d_rev_offsets, rev_offsets.data(), rev_offsets.size()*sizeof(int)});
    //  xfer rev_nonzeros
    htod_job.push_back({d_rev_nonzeros, rev_nonzeros.data(), rev_nonzeros.size()*sizeof(int)});
    //  xfer distance
    htod_job.push_back({d_distance, distance.data(), distance.size()*sizeof(int)});
    BSG_CUDA_CALL(hb_mc_device_dma_to_device(&device, htod_job.data(), htod_job.size()));

    // CUDA args;
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 10
    uint32_t cuda_argv[CUDA_ARGC] = {
      V,
      root,
      d_fwd_offsets,
      d_fwd_nonzeros,
      d_rev_offsets,
      d_rev_nonzeros,
      d_curr_frontier, 
      d_curr_dense_frontier,
      d_next_dense_frontier,
      d_distance
    };

    // Enqueue kernel;
    BSG_CUDA_CALL(hb_mc_kernel_enqueue(&device, grid_dim, tg_dim, "kernel", CUDA_ARGC, cuda_argv));

    // Launch Kernel;
    //hb_mc_manycore_trace_enable((&device)->mc);
    BSG_CUDA_CALL(hb_mc_device_tile_groups_execute(&device));
    //hb_mc_manycore_trace_disable((&device)->mc);

    // IO xfers
    std::vector<hb_mc_dma_dtoh_t> dtoh_job;
    // 1. xfer distance
    dtoh_job.push_back({d_distance, distance.data(), distance.size()*sizeof(int)});
    BSG_CUDA_CALL(hb_mc_device_dma_to_host(&device, dtoh_job.data(), dtoh_job.size()));

    // Check correctness
    for (int v = 0; v < V; v++) {
        if (distance[v] != solution_distance[v]) {
            bsg_pr_err("Error: kernel distance[%d] = %d, solution distance[%d]=%d\n",
                       v, distance[v], v, solution_distance[v]);
            r = HB_MC_FAIL;
        }
    }

    // Freeze
    BSG_CUDA_CALL(hb_mc_device_program_finish(&device));
  }

  // Finish;
  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  return r;
}


declare_program_main("bfs-single-pod", bfs_single_pod);

