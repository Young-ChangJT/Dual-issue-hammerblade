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

// You can change this value in Makefile.
#ifndef SIZE
Please define SIZE in Makefile.
#endif


int kernel_prefetch(int argc, char **argv) {

  int rc;
  char *bin_path, *test_name;
  struct arguments_path args = {NULL, NULL};
  
  argp_parse(&argp_path, argc, argv, 0, 0, &args);
  bin_path = args.path;
  test_name = args.name;

  bsg_pr_test_info("Running kernel_prefetch.\n");
  srand(time);
 
  // Initialize Device.
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, test_name, 0));

  hb_mc_pod_id_t pod;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    bsg_pr_info("Loading program for pod %d\n.", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

    // Allocate a block of memory in host.
    int A_host[SIZE];
    for (int i = 0; i < SIZE; i++) {
      A_host[i] = i;
    }

    // Allocate a block of memory in device.
    eva_t A_device;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, SIZE * sizeof(int), &A_device));
  
 
    // DMA Transfer to device.
    hb_mc_dma_htod_t htod_job [] = {
      {
        .d_addr = A_device,
        .h_addr = (void *) &A_host[0],
        .size = SIZE * sizeof(int)
      }
    };

    BSG_CUDA_CALL(hb_mc_device_dma_to_device(&device, htod_job, 1));


    // CUDA arguments
    hb_mc_dimension_t tg_dim = { .x = 16, .y = 8};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 2
    uint32_t cuda_argv[CUDA_ARGC] = {A_device, SIZE};
    
    // Enqueue Kernel.
    BSG_CUDA_CALL(hb_mc_kernel_enqueue (&device, grid_dim, tg_dim, "kernel_prefetch", CUDA_ARGC, cuda_argv));
    
    // Launch kernel.
    hb_mc_manycore_trace_enable((&device)->mc);
    BSG_CUDA_CALL(hb_mc_device_tile_groups_execute(&device));
    hb_mc_manycore_trace_disable((&device)->mc);


    // Freeze tiles.
    BSG_CUDA_CALL(hb_mc_device_program_finish(&device));
  }

  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  return HB_MC_SUCCESS; 
}


declare_program_main("prefetch", kernel_prefetch);
