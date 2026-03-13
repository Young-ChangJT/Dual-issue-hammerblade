#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>

#define MAX_UPDATES 512
#define CONCURRENCY 8

#ifdef WARM_CACHE
#define CACHE_LINE_WORDS 16
__attribute__((noinline))
static int warmup(int *A, int N)
{
  for (int i = __bsg_id*CACHE_LINE_WORDS; i < N; i += bsg_tiles_X*bsg_tiles_Y*CACHE_LINE_WORDS) {
    asm volatile ("lw x0, %[p]" :: [p] "m" (A[i]));
  }
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
  return 0;

}
#endif


int LOCAL_X[MAX_UPDATES];

extern "C" __attribute__ ((noinline))
int
kernel_gups_rmw(int * dram_A, int * dram_X, int num_update) {

  bsg_barrier_hw_tile_group_init();

  // setup
  int start = num_update * __bsg_id;
  for (int i = 0; i < num_update; i++) {
    LOCAL_X[i] = dram_X[start+i];
  }

  bsg_barrier_hw_tile_group_sync();

#ifdef WARM_CACHE
  warmup(dram_A, A_SIZE);
#endif

  // Run gups
  bsg_cuda_print_stat_kernel_start();
  bsg_fence();

  #define NUM_ITER 8
  for (int iter = 0; iter < NUM_ITER; iter++) {
    for (int i = 0; i < num_update; i += CONCURRENCY) {
      int reg_x[CONCURRENCY];
      int reg_a[CONCURRENCY];

      bsg_unroll(32)
      for (int j = 0; j < CONCURRENCY; j++) {
        reg_x[j] = LOCAL_X[i+j];
        reg_a[j] = dram_A[reg_x[j]]; // load
      }
    
      bsg_unroll(32)
      for (int j = 0; j < CONCURRENCY; j++) {
        dram_A[reg_x[j]] = reg_a[j] ^ reg_x[j]; // modify + store
      }
    }
  }
  

  bsg_fence();
  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();

  return 0;
}
