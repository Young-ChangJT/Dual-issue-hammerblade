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


#define PRECISION (15)
int is_close(float complex n, float complex r) {
  float nr = crealf(n), ni = cimagf(n);
  float rr = crealf(r), ri = cimagf(r);
  float dr = nr-rr, di = ni-ri;
  float dist = sqrt(dr*dr+di*di);
  if (dist < PRECISION)
    return 1;
  else
    return 0;
}


int verify_fft (float complex *out, int N) { 
  int r  = N / 16;
  int ar = N - r;

  for (int i = 0; i < N; i++) {
    float rr = crealf(out[i]), ii = cimagf(out[i]);
    printf("%d-th result is %.6f+%.6fi (0x%08X 0x%08X)\n", i, rr, ii, *(uint32_t*)&rr, *(uint32_t*)&ii);
  }

  for (int i = 0; i < N; i++) {
    double complex ref = 0.0;
    printf("%d-th component is %.3f+%.3fi\n", i, crealf(out[i]), cimagf(out[i]));
    if ((i == r) || (i == ar)) {
      ref = N / 2.0;
    } else {
      ref = 0.0;
    }

    if (!is_close(out[i], ref)) {
      printf("Mismatch: out[%d]: %.3f+%.3fi; ref is %.3f+%.3fi",
          i , crealf(out[i]), cimagf(out[i]), crealf(ref), cimagf(ref)
      );
      return 1;
    }
  }
  return 0;
}


// Host main;
int fft_multipod (int argc, char **argv)
{
  int r = 0;

  // Command args;
  const char *bin_path = argv[1];

  // parameters;
  printf("NUM_POINTS=%d\n", NUM_POINTS);
  printf("NUM_ITER=%d\n", NUM_ITER);
  const int N = (NUM_POINTS*NUM_POINTS);

  // Host data;
  float complex A_host[N];
  float complex B_host[N];
  float complex TW_host[N];

  for (int i = 0; i < N; i++) {
    A_host[i] = cosf(i*M_PI/8.0);
  }

  for (int r = 0; r < NUM_POINTS; r++) {
    for (int c = 0; c < NUM_POINTS; c++) {
      float ref_sinf = sinf(-2.0f*M_PI*(float)(r*c)/(float)(N));
      float ref_cosf = cosf(-2.0f*M_PI*(float)(r*c)/(float)(N));
      TW_host[(r*NUM_POINTS)+c] = ref_cosf + I*ref_sinf;
    }
  }


  // Initialize devices;
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, "fft_multipod", HB_MC_DEVICE_ID));
  eva_t d_A;
  eva_t d_B;
  eva_t d_TW;

  hb_mc_pod_id_t pod;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    bsg_pr_info("Loading program for pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

    // Allocate memory on devices;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, N*sizeof(float complex)*NUM_ITER, &d_A));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, N*sizeof(float complex)*NUM_ITER, &d_B));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, N*sizeof(float complex), &d_TW));


    // DMA transfer;
    printf("Transferring data: pod %d\n", pod);
    for (int i = 0; i < NUM_ITER; i++) {
      hb_mc_dma_htod_t htod_A_job [] = {
        {
          .d_addr = d_A + (sizeof(float complex)*N*i),
          .h_addr = A_host,
          .size   = N * sizeof(float complex)
        }
      };
      BSG_CUDA_CALL(hb_mc_device_transfer_data_to_device(&device, htod_A_job, 1));
    }

    hb_mc_dma_htod_t htod_TW_job [] = {
      {
        .d_addr = d_TW,
        .h_addr = TW_host,
        .size   = N * sizeof(float complex)
      }
    };
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_device(&device, htod_TW_job, 1));

          
    // Cuda arguments;
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 5
    uint32_t cuda_argv[CUDA_ARGC] = {d_A, d_B, d_TW, NUM_ITER, pod};

    // Enqueue kernel;
    BSG_CUDA_CALL(hb_mc_kernel_enqueue (&device, grid_dim, tg_dim, "kernel", CUDA_ARGC, cuda_argv));
  }


  // Launch pods;
  //hb_mc_manycore_trace_enable((&device)->mc);
  printf("Launching all pods\n");
  BSG_CUDA_CALL(hb_mc_device_pods_kernels_execute(&device));
  //hb_mc_manycore_trace_disable((&device)->mc);


  // Read from devices;
  int fail = 0;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    bsg_pr_info("Reading from pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));

    // Verify FFT;
    for (int i = 0; i < NUM_ITER; i++) {
      // Clear B;
      for (int j = 0; j < N; j++) {
        B_host[i] = 0.0f;
      }

      // DMA transfer: device -> host;
      hb_mc_dma_dtoh_t dtoh_B_job [] = {
        {
          .d_addr = d_B + (N*i*sizeof(float complex)),
          .h_addr = B_host,
          .size   = N * sizeof(float complex)
        }
      };
      BSG_CUDA_CALL(hb_mc_device_transfer_data_to_host(&device, dtoh_B_job, 1));

      if (verify_fft(B_host, NUM_POINTS*NUM_POINTS)) {
        fail = 1;
      }
    }
  }

  // Finish device.
  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  if (fail) {
    return HB_MC_FAIL;
  } else {
    return HB_MC_SUCCESS;
  }
}

declare_program_main("fft_multipod", fft_multipod);
