#include <bsg_manycore_tile.h>
#include <bsg_manycore_errno.h>
#include <bsg_manycore_tile.h>
#include <bsg_manycore_loader.h>
#include <bsg_manycore_cuda.h>
#include <cstdlib>
#include <time.h>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <bsg_manycore_regression.h>
#include <bsg_manycore.h>
#include <cstdint>
#include <vector>
#include "aes.hpp"

#define ALLOC_NAME "default_allocator"
#define NUM_TILES (bsg_tiles_X*bsg_tiles_Y)
#define MSG_LEN 1024

int aes_multipod(int argc, char **argv)
{
  int r = 0;
  
  // command line;
  const char *bin_path = argv[1];

  // parameters;
  const int num_iter = NUM_ITER;
  const int msg_len = MSG_LEN;
  printf("num_iter=%d\n", num_iter);  // # of iter per tile;
  printf("msg_len=%d\n", msg_len);    // in bytes;


  // Host memory;
  uint8_t buf[NUM_TILES*NUM_ITER*MSG_LEN];
  struct AES_ctx ctx[NUM_TILES*NUM_ITER];

  for (int i = 0; i < NUM_TILES*NUM_ITER; i++) {
    for (int j = 0; j < MSG_LEN; j++) {
      buf[(MSG_LEN*i)+j] = (uint8_t) j;
    }
  }
  
  memset(ctx, 0, sizeof(ctx));
  uint8_t key[AES_KEYLEN] = {0, 1, 2, 3, 4, 5, 6, 7,
                             8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};
  for (int i = 0; i < NUM_TILES*NUM_ITER; i++) {
    AES_init_ctx(&ctx[i], key);
  }


  // Host calculation;
  uint8_t expected_buf[MSG_LEN];
  struct AES_ctx expected_ctx;

  for (int j = 0; j < MSG_LEN; j++) {
    expected_buf[j] = (uint8_t) j;
  }
  memset(&expected_ctx, 0, sizeof(expected_ctx));
  AES_init_ctx(&expected_ctx, key);
  
  AES_CBC_encrypt_buffer(&expected_ctx, expected_buf, MSG_LEN);



  // Initialize device;
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, "aes_multipod", HB_MC_DEVICE_ID));

  eva_t d_ctx;
  eva_t d_buf;

  hb_mc_pod_id_t pod;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    printf("Loading program for pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));
  
    // Allocate memory on device;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sizeof(ctx), &d_ctx));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sizeof(buf), &d_buf));

    // DMA transfer;
    printf("Transferring data: pod %d\n", pod);
    std::vector<hb_mc_dma_htod_t> htod_job;
    htod_job.push_back({d_ctx, &ctx[0], sizeof(ctx)});
    htod_job.push_back({d_buf, &buf[0], sizeof(buf)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_device(&device, htod_job.data(), htod_job.size()));


    // Cuda args;
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 5
    uint32_t cuda_argv[CUDA_ARGC] = {d_ctx, d_buf, MSG_LEN, NUM_ITER, pod};

    // Enqueue kernel;
    printf("Enqueue Kernel: pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_kernel_enqueue (&device, grid_dim, tg_dim, "kernel", CUDA_ARGC, cuda_argv));
  }


  // Launch pod;
  printf("Launching all pods\n");
  BSG_CUDA_CALL(hb_mc_device_pods_kernels_execute(&device));
 

  // Read from devices;
  uint8_t actual_buf[NUM_TILES*NUM_ITER*MSG_LEN];

  bool fail = false;
  hb_mc_device_foreach_pod_id(&device, pod) {
    printf("Reading results: pods %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));

    // clear buf;
    for (int i = 0; i < NUM_TILES*NUM_ITER*MSG_LEN; i++) {
      actual_buf[i] = (uint8_t) 0;
    }   

    // DMA transfer: device -> host;
    std::vector<hb_mc_dma_dtoh_t> dtoh_job;
    dtoh_job.push_back({d_buf, actual_buf, NUM_TILES*NUM_ITER*MSG_LEN*sizeof(uint8_t)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_host(&device, dtoh_job.data(), dtoh_job.size()));


    // Validate;
    for (int i = 0; i < NUM_TILES*NUM_ITER; i++) {
      for (int j = 0; j < MSG_LEN; j++) {
        uint8_t actual = actual_buf[(MSG_LEN*i)+j];
        uint8_t expected = expected_buf[j];
        if (actual != expected) {
          printf("Mismatch: i=%d, j=%d, actual=%d, expected=%d\n", i, j, actual, expected);
          fail = true;
        }
      }
    }
  }

 
  // FINISH;
  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  if (fail) {
    return HB_MC_FAIL;
  } else {
    return HB_MC_SUCCESS;
  }



}

declare_program_main("aes_multipod", aes_multipod);

