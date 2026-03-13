#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>

// array of remote DMEM EVA;
#define MAX_UPDATE 512
float * LOCAL_ADDR[MAX_UPDATE];

// this is the memory location that every tile hits on.
float data = 0.0f;


// A = list of EVA (remote dmem without offset)
// N = length of A
extern "C" __attribute__ ((noinline))
int
kernel_tile_gups(int * A, int N) {

  bsg_barrier_hw_tile_group_init();
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();

  // load remote addresses
  float ** myA = (float**)  &A[__bsg_id*N];
  bsg_unroll(8)
  for (int i = 0; i < N; i++) {
    LOCAL_ADDR[i] = (float *) myA[i];
    LOCAL_ADDR[i] = (float *) ((int) LOCAL_ADDR[i] | (int) (&data));
  }
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
  bsg_cuda_print_stat_kernel_start();

  // Run tile gups
  #define NUM_ITER 16
  bsg_unroll(1)
  for (int iter = 0; iter < NUM_ITER; iter++) {
    bsg_unroll(1)
    for (int i = 0; i < N; i += UNROLL) {
      float * reg_addr[UNROLL];
      float reg_data[UNROLL];

      bsg_unroll(UNROLL)
      for (int j = 0; j < UNROLL; j++) {
        reg_addr[j] = LOCAL_ADDR[i+j];
        reg_data[j] = *reg_addr[j]; // read
      }

      asm volatile("": : :"memory");

      bsg_unroll(UNROLL)
      for (int j = 0; j < UNROLL; j++) {
        *reg_addr[j] = reg_data[j] + 1.0f; // modify + write
      }

    }
  }


  bsg_fence();
  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
  return 0;
}
