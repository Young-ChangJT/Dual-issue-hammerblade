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

#define ALLOC_NAME "default_allocator"

#define NUM_UPDATE 512
#define MAX_NUM_TILE (TILE_GROUP_DIM_X*TILE_GROUP_DIM_Y)

int kernel_gups_rmw(int argc, char **argv) {

  int rc;
  char *bin_path, *test_name;
  struct arguments_path args = {NULL, NULL};
  
  argp_parse(&argp_path, argc, argv, 0, 0, &args);
  bin_path = args.path;
  test_name = args.name;

  srand(time);
 
  // Initialize Device.
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, test_name, 0));

  // Allocate a block of memory in host.
  int * A_host = (int *) malloc(A_SIZE*sizeof(int));
  int * X_host = (int *) malloc(NUM_UPDATE*MAX_NUM_TILE*sizeof(int));
  for (int i = 0; i < A_SIZE; i++) {
    A_host[i] = i; 
  }
  for (int i = 0; i < NUM_UPDATE*MAX_NUM_TILE; i++) {
    X_host[i] = rand() % A_SIZE;
  }


  hb_mc_pod_id_t pod;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    bsg_pr_info("Loading program for pod %d\n.", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));


    // Allocate a block of memory in device.
    eva_t A_device, X_device;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, A_SIZE * sizeof(int), &A_device));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, NUM_UPDATE * MAX_NUM_TILE * sizeof(int), &X_device));
  
 
    // DMA Transfer to device.
    hb_mc_dma_htod_t htod_job [] = {
      {
        .d_addr = A_device,
        .h_addr = (void *) &A_host[0],
        .size = A_SIZE * sizeof(int)
      },
      {
        .d_addr = X_device,
        .h_addr = (void *) &X_host[0],
        .size = NUM_UPDATE * MAX_NUM_TILE * sizeof(int)
      }
    };

    BSG_CUDA_CALL(hb_mc_device_dma_to_device(&device, htod_job, 2));


    // CUDA arguments
    hb_mc_dimension_t tg_dim = { .x = TILE_GROUP_DIM_X, .y = TILE_GROUP_DIM_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 3
    uint32_t cuda_argv[CUDA_ARGC] = {A_device, X_device, NUM_UPDATE};
    
    // Enqueue Kernel.
    BSG_CUDA_CALL(hb_mc_kernel_enqueue (&device, grid_dim, tg_dim, "kernel_gups_rmw", CUDA_ARGC, cuda_argv));
    
    // Launch kernel.
    //hb_mc_manycore_trace_enable((&device)->mc);
    BSG_CUDA_CALL(hb_mc_device_tile_groups_execute(&device));
    //hb_mc_manycore_trace_disable((&device)->mc);

    // Freeze tiles.
    BSG_CUDA_CALL(hb_mc_device_program_finish(&device));
  }

  // Free host memory.
  free(A_host);
  free(X_host);

  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  return HB_MC_SUCCESS; 
}


declare_program_main("gups_rmw", kernel_gups_rmw);
