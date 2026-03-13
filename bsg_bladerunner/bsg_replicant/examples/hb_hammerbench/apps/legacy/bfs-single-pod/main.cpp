#include <bsg_manycore_tile.h>
#include <bsg_manycore_errno.h>
#include <bsg_manycore_tile.h>
#include <bsg_manycore_loader.h>
#include <bsg_manycore_cuda.h>
#include <math.h>
#include <complex.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <bsg_manycore_regression.h>
#include <vector>
#include <string>
#include "read_graph.hpp"
#include "transpose_graph.hpp"
#include "breadth_first_search_graph.hpp"

#define ARRAY_SIZE(x)                           \
    (sizeof(x)/sizeof(x[0]))

#define ALLOC_NAME "default_allocator"

#define NUM_UPDATE 512
#define MAX_NUM_TILE 128

class CommandLine {
public:
    CommandLine(){}
    static CommandLine Parse(int argc, char *argv[]){
        CommandLine cli;
        cli.bin_path    = argv[1];
        cli.graph_input = argv[2];
        cli.start       = argv[3];
        return cli;
    }
    std::string bin_path;
    std::string test_name;
    std::string graph_input;
    std::string start;
};

template <typename T>
int set_symbol(hb_mc_device_t *devp, const std::string &symbol, T val) {
    hb_mc_eva_t addr;
    hb_mc_pod_t *pod = &devp->pods[devp->default_pod_id];
    hb_mc_program_t *prog = pod->program;
    BSG_CUDA_CALL(hb_mc_loader_symbol_to_eva(prog->bin, prog->bin_size, symbol.c_str(), &addr));
    BSG_CUDA_CALL(hb_mc_device_memcpy_to_device(devp, addr, &val, sizeof(T)));
    return HB_MC_SUCCESS;
}

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

int bfs_single_pod(int argc, char **argv) {
  auto cli = CommandLine::Parse(argc, argv);  
  const char *bin_path = cli.bin_path.c_str();  
  int root = atoi(cli.start.c_str());
  int r = HB_MC_SUCCESS;
      
  // Read input graph
  int V, E;
  std::vector<int> fwd_offsets, fwd_nonzeros;
  BSG_CUDA_CALL(read_graph(cli.graph_input, &V, &E, fwd_offsets, fwd_nonzeros));

  // Transpose
  std::vector<int> rev_offsets, rev_nonzeros;
  BSG_CUDA_CALL(transpose_graph(V, E, fwd_offsets, fwd_nonzeros, rev_offsets, rev_nonzeros));

  // distance
  std::vector<int> distance(V, -1);  

  // find a solution
  std::vector<int> solution_distance;
  breadth_first_search_graph(root, V, E, fwd_offsets, fwd_nonzeros, solution_distance);
  
  // Initialize Device.
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, "bfs_single_pod", 0));
  
  // Allocate memory on host
  hb_mc_pod_id_t pod;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    bsg_pr_info("Loading program for pod %d\n.", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));


    // Allocate memory on device
    // 1. allocate curr_frontier
    hb_mc_eva_t d_curr_frontier;
    BSG_CUDA_CALL(malloc_pod_cache_aligned(&device, V*sizeof(int), &d_curr_frontier, nullptr));
    // 2. allocate next_frontier
    hb_mc_eva_t d_next_frontier;
    BSG_CUDA_CALL(malloc_pod_cache_aligned(&device, V*sizeof(int), &d_next_frontier, nullptr));
    // 3. allocate dense_to_sparse_set
    hb_mc_eva_t d_dense_to_sparse_set;
    BSG_CUDA_CALL(malloc_pod_cache_aligned(&device, V*sizeof(int), &d_dense_to_sparse_set, nullptr));
    // 4. allocate sparse_to_dense_set
    hb_mc_eva_t d_sparse_to_dense_set;
    BSG_CUDA_CALL(malloc_pod_cache_aligned(&device, V*sizeof(int), &d_sparse_to_dense_set, nullptr));
    // 5. allocate fwd_offsets
    hb_mc_eva_t d_fwd_offsets;
    BSG_CUDA_CALL(malloc_pod_cache_aligned(&device, fwd_offsets.size()*sizeof(int), &d_fwd_offsets, nullptr));
    // 6. allocate fwd_nonzeros
    hb_mc_eva_t d_fwd_nonzeros;
    BSG_CUDA_CALL(malloc_pod_cache_aligned(&device, fwd_nonzeros.size()*sizeof(int), &d_fwd_nonzeros, nullptr));
    // 7. allocate rev_offsets
    hb_mc_eva_t d_rev_offsets;
    BSG_CUDA_CALL(malloc_pod_cache_aligned(&device, rev_offsets.size()*sizeof(int), &d_rev_offsets, nullptr));
    // 8. allocate rev_nonzeros
    hb_mc_eva_t d_rev_nonzeros;
    BSG_CUDA_CALL(malloc_pod_cache_aligned(&device, rev_nonzeros.size()*sizeof(int), &d_rev_nonzeros, nullptr));
    // 9. allocate distance
    hb_mc_eva_t d_distance;
    BSG_CUDA_CALL(malloc_pod_cache_aligned(&device, V*sizeof(int), &d_distance, nullptr));
    
    // IO to device.
    // set globals
    // 1. set V
    BSG_CUDA_CALL(set_symbol(&device, "g_V", V));
    // 2. set E
    BSG_CUDA_CALL(set_symbol(&device, "g_E", E));
    // 3. set fwd_offsets
    BSG_CUDA_CALL(set_symbol(&device, "g_fwd_offsets", d_fwd_offsets));
    // 4. set fwd_nonzeros
    BSG_CUDA_CALL(set_symbol(&device, "g_fwd_nonzeros", d_fwd_nonzeros));
    // 5. set rev_offsets
    BSG_CUDA_CALL(set_symbol(&device, "g_rev_offsets", d_rev_offsets));
    // 6. set rev_nonzeros
    BSG_CUDA_CALL(set_symbol(&device, "g_rev_nonzeros", d_rev_nonzeros));
    // 7. set distance
    BSG_CUDA_CALL(set_symbol(&device, "g_distance", d_distance));
    // 8. set curr_frontier
    BSG_CUDA_CALL(set_symbol(&device, "g_curr_frontier", d_curr_frontier));
    // 9. set next_frontier
    BSG_CUDA_CALL(set_symbol(&device, "g_next_frontier", d_next_frontier));
    // 10. set dense_to_sparse_set
    BSG_CUDA_CALL(set_symbol(&device, "g_dense_to_sparse_set", d_dense_to_sparse_set));
    // 11. set sparse_to_dense_set
    BSG_CUDA_CALL(set_symbol(&device, "g_sparse_to_dense_set", d_sparse_to_dense_set));
    // 12. set root
    BSG_CUDA_CALL(set_symbol(&device, "g_root", root));
    
    // dma transfers
    std::vector<hb_mc_dma_htod_t> htod_job;
    // 1. xfer fwd_offsets
    htod_job.push_back({d_fwd_offsets, fwd_offsets.data(), fwd_offsets.size()*sizeof(int)});
    // 2. xfer fwd_nonzeros
    htod_job.push_back({d_fwd_nonzeros, fwd_nonzeros.data(), fwd_nonzeros.size()*sizeof(int)});
    // 3. xfer rev_offsets
    htod_job.push_back({d_rev_offsets, rev_offsets.data(), rev_offsets.size()*sizeof(int)});    
    // 4. xfer rev_nonzeros
    htod_job.push_back({d_rev_nonzeros, rev_nonzeros.data(), rev_nonzeros.size()*sizeof(int)});
    // 5. xfer distance
    htod_job.push_back({d_distance, distance.data(), distance.size()*sizeof(int)});
    BSG_CUDA_CALL(hb_mc_device_dma_to_device(&device, htod_job.data(), htod_job.size()));


    // CUDA arguments
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    uint32_t cuda_argv[] = {};
    
    // Enqueue Kernel.
    BSG_CUDA_CALL(hb_mc_kernel_enqueue (&device, grid_dim, tg_dim,
                                        "kernel", ARRAY_SIZE(cuda_argv), cuda_argv));
    
    // Launch kernel.
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

    // Freeze tiles.
    BSG_CUDA_CALL(hb_mc_device_program_finish(&device));
  }

  // Free host memory.
  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  return r;
}

declare_program_main("bfs-single-pod", bfs_single_pod);
