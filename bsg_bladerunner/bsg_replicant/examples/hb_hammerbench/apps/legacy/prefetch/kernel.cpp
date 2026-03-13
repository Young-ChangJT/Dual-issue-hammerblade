#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>


extern "C" __attribute__ ((noinline))
int
kernel_prefetch(int * A, int N) {

  bsg_barrier_hw_tile_group_init();
  bsg_barrier_hw_tile_group_sync();
  bsg_cuda_print_stat_kernel_start();
  bsg_fence();

  for (int i = __bsg_id*CACHE_LINE_WORDS; i < N; i += bsg_tiles_X*bsg_tiles_Y*CACHE_LINE_WORDS) {
    asm volatile ("lw x0, %[p]" :: [p] "m" (A[i]));
  }

  bsg_fence();
  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();

  return 0;
}
