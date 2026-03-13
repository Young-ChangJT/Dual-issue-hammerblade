#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <random>
#include <limits>
#include <bsg_manycore_cuda.h>
#include <bsg_manycore_regression.h>
#include <bsg_manycore_eva.h>

#define ALLOC_NAME "default_allocator"

void host_mm(float *result, float *mat1, float *mat2)
{
  for (int y = 0; y < N; y++) {
    for (int x =0; x < N; x++) {
      float sum = 0.0f;
      for (int z = 0; z < N; z++) {
        sum += mat1[(N*y)+z]*mat2[(N*z)+x];
      }
      result[(N*y)+x] = sum;
    }
  }
}


// Host Main;
int sgemm_multipod(int argc, char **argv)
{
  int r = 0;
  
  // Command line arg;
  const char *bin_path = argv[1];

  // parameters;
  printf("N=%d\n", N);
  printf("NITER=%d\n", NITER);

  


  // Pod;
  // matrix;
  float *mat1 = (float*) malloc(NITER*N*N*sizeof(float));
  float *mat2 = (float*) malloc(NITER*N*N*sizeof(float));
  float *mat_result = (float*) malloc(NITER*N*N*sizeof(float));
  for (int i = 0; i < NITER*N*N; i++) {
    mat1[i] = (float) (i % 7);
    mat2[i] = (float) (i % 3);
  }
  
  for (int i = 0; i < NITER; i++) {
    host_mm(&mat_result[N*N*i], &mat1[N*N*i], &mat2[N*N*i]);
  } 


  // Initialize devices;
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, "sgemm_multipod", HB_MC_DEVICE_ID));
  eva_t d_mat1;
  eva_t d_mat2;
  eva_t d_result;

  hb_mc_pod_id_t pod;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    printf("Loading program for pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

    // Allocate memory on device;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, NITER*N*N*sizeof(float), &d_mat1));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, NITER*N*N*sizeof(float), &d_mat2));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, NITER*N*N*sizeof(float), &d_result));

    // DMA transfer;
    printf("Transferring data: pod %d\n", pod);
    std::vector<hb_mc_dma_htod_t> htod_job;
    htod_job.push_back({d_mat1, mat1, NITER*N*N*sizeof(float)});
    htod_job.push_back({d_mat2, mat2, NITER*N*N*sizeof(float)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_device(&device, htod_job.data(), htod_job.size()));

    // cuda arguments
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y };
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 4
    uint32_t cuda_argv[CUDA_ARGC] = {d_mat1, d_mat2, d_result, pod};

    // Enqueue kernel
    printf("Enqueue Kernel: pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_kernel_enqueue (&device, grid_dim, tg_dim, "kernel", CUDA_ARGC, cuda_argv));
  }

  // Launch pods;
  printf("Launching all pods\n");
  BSG_CUDA_CALL(hb_mc_device_pods_kernels_execute(&device));


  // Read from devices;
  bool fail = false;
  float *actual_result = (float*) malloc(NITER*N*N*sizeof(float));

  hb_mc_device_foreach_pod_id(&device, pod) {
    printf("Reading results: pods %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));

    // clear;
    for (int i = 0; i < NITER*N*N; i++) {
      actual_result[i] = 0.0f;
    }

    // DMA transfer; device -> host;
    std::vector<hb_mc_dma_dtoh_t> dtoh_job;
    dtoh_job.push_back({d_result, actual_result, NITER*N*N*sizeof(float)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_host(&device, dtoh_job.data(), dtoh_job.size()));

    // validate
    float sse = 0.0f;
    for (int i = 0; i < NITER*N*N; i++) {
      float actual = actual_result[i];
      float expected = mat_result[i];
      float diff = expected - actual;
      sse += diff*diff;
      if (diff != 0.0f) {
        printf("[%d] actual=%f, expected=%f\n", i, actual, expected);
      }
    }

    if (sse >= .01f) {
      printf("Matrix Mismatch. SSE= %f\n", sse);
      fail = true;
    }
  }

  free(mat1);
  free(mat2);
  free(mat_result);
  free(actual_result);

  // FINISH;
  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  if (fail) {
    return HB_MC_FAIL;
  } else {
    return HB_MC_SUCCESS;
  }
}

declare_program_main("sgemm_multipod", sgemm_multipod);
