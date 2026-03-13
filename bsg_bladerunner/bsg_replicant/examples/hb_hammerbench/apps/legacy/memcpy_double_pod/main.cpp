#include <bsg_manycore_tile.h>
#include <bsg_manycore_errno.h>
#include <bsg_manycore_tile.h>
#include <bsg_manycore_loader.h>
#include <bsg_manycore_cuda.h>
#include <bsg_manycore_vcache.h>
#include <math.h>
#include <complex.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <map>
#include <bsg_manycore_regression.h>

#define ALLOC_NAME "default_allocator"

// How much memcpy to distribute (in # of words)??
// You can change this value in Makefile.
#ifndef SIZE
Please define SIZE in Makefile.
#endif


int kernel_memcpy(int argc, char **argv) {

  int rc;
  char *bin_path, *test_name;
  struct arguments_path args = {NULL, NULL};
  
  argp_parse(&argp_path, argc, argv, 0, 0, &args);
  bin_path = args.path;
  test_name = args.name;

  bsg_pr_test_info("Running kernel_memcpy.\n");
  srand(0);
 
  // Initialize Device.
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, test_name, 0));

  std::map<hb_mc_pod_id_t,int*> A_host_map;
  std::map<hb_mc_pod_id_t,int*> B_host_map;
  std::map<hb_mc_pod_id_t,int*> C_host_map;
  std::map<hb_mc_pod_id_t,eva_t> A_device_map;
  std::map<hb_mc_pod_id_t,eva_t> B_device_map;

  // print some facts.
  const hb_mc_config_t *cfg = hb_mc_manycore_get_config(device.mc);
  printf("num_caches = %d\n", hb_mc_vcache_num_caches(device.mc));
  printf("dram_channels = %d\n", hb_mc_config_get_dram_channels(cfg));

  hb_mc_pod_id_t pod;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    bsg_pr_info("Loading program for pod %d\n.", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

    // Allocate a block of memory in host.
    int* A_host = (int *) malloc(sizeof(int)*SIZE);
    int* B_host = (int *) malloc(sizeof(int)*SIZE);
    int* C_host = (int *) malloc(sizeof(int)*SIZE);
    A_host_map[pod] = A_host;
    B_host_map[pod] = B_host;
    C_host_map[pod] = C_host;
    for (int i = 0; i < SIZE; i++) {
      A_host[i] = i + (((int) pod)*SIZE);
    }

    // Make it pod-cache aligned
#define POD_CACHE_ALIGNED
#ifdef POD_CACHE_ALIGNED
    eva_t temp_device1, temp_device2;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, CACHE_LINE_WORDS*sizeof(int), &temp_device1));
    printf("temp Addr: %x\n", temp_device1);
    int align_size = (32)-1-((temp_device1>>2)%(CACHE_LINE_WORDS*32)/CACHE_LINE_WORDS);
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, align_size*sizeof(int)*CACHE_LINE_WORDS, &temp_device2));
#endif

    // create offset
#define CREATE_CACHE_OFFSET
#ifdef CREATE_CACHE_OFFSET
    const int cache_offset = 8;
    eva_t temp_device3;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, cache_offset*CACHE_LINE_WORDS*sizeof(int), &temp_device3));
#endif


    // Allocate a block of memory in device.
    eva_t A_device, B_device;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, SIZE * sizeof(int), &A_device));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, SIZE * sizeof(int), &B_device));
    A_device_map[pod] = A_device;
    B_device_map[pod] = B_device;

    printf("A_device Addr: %x\n", A_device);
    printf("B_device Addr: %x\n", B_device);
 
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
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 3
    uint32_t cuda_argv[CUDA_ARGC] = {A_device, B_device, SIZE};
    
    // Enqueue Kernel.
    BSG_CUDA_CALL(hb_mc_kernel_enqueue (&device, grid_dim, tg_dim, "kernel_memcpy", CUDA_ARGC, cuda_argv));
    

    // Freeze tiles.
    //BSG_CUDA_CALL(hb_mc_device_program_finish(&device));
  }

  // Launch on all pods
  BSG_CUDA_CALL(hb_mc_device_pods_kernels_execute(&device));

  bool failed = 0;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));

    // Copy result and validate.
    hb_mc_dma_dtoh_t dtoh_job0;
    dtoh_job0.d_addr = A_device_map[pod];
    dtoh_job0.h_addr = (void *) &C_host_map[pod][0];
    dtoh_job0.size = SIZE * sizeof(int);

    hb_mc_dma_dtoh_t dtoh_job1;
    dtoh_job1.d_addr = B_device_map[pod];
    dtoh_job1.h_addr = (void *) &B_host_map[pod][0];
    dtoh_job1.size = SIZE * sizeof(int);


    BSG_CUDA_CALL(hb_mc_device_dma_to_host(&device, &dtoh_job0, 1));
    BSG_CUDA_CALL(hb_mc_device_dma_to_host(&device, &dtoh_job1, 1));
    for (int i = 0; i < SIZE; i++) {
      if (C_host_map[pod][i] != A_host_map[pod][i]) {
        printf("FAIL: A and C does not match. pod=%d, i=%d, C=%d, A=%d\n", pod, i, C_host_map[pod][i], A_host_map[pod][i]);
        failed = 1;
      }
    }


    for (int i = 0; i < SIZE; i++) {
      if (B_host_map[pod][i] != A_host_map[pod][i]) {
        printf("FAIL: A and B does not match. pod=%d, i=%d, B=%d, A=%d\n", pod, i, B_host_map[pod][i], A_host_map[pod][i]);
        failed = 1;
      }
    }

  }

  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  if (failed) {
    return HB_MC_FAIL;
  } else {
    return HB_MC_SUCCESS; 
  }
}


declare_program_main("memcpy", kernel_memcpy);
