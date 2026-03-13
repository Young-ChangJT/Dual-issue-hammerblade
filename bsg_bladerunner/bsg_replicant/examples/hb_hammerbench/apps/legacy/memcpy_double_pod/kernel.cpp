#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>

#ifdef WARM_CACHE
#ifndef CACHE_LINE_WORDS
#error "CACHE_LINE_WORDS not defined"
#endif
__attribute__((noinline))
static void warmup(int *A, int *B, int N)
{
  for (int i = __bsg_id*CACHE_LINE_WORDS; i < N; i += bsg_tiles_X*bsg_tiles_Y*CACHE_LINE_WORDS) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (A[i]));
      asm volatile ("lw x0, %[p]" :: [p] "m" (B[i]));
  }
  bsg_fence();
}
#endif

extern "C" __attribute__ ((noinline))
int
kernel_memcpy(int * A, int * B, int N) {

  bsg_barrier_hw_tile_group_init();
#ifdef WARM_CACHE
  warmup(A, B, N);
#endif
  bsg_barrier_hw_tile_group_sync();
  bsg_cuda_print_stat_kernel_start();

  for (int i = __bsg_id*16; i < N; i += bsg_tiles_X*bsg_tiles_Y*16) {
    register int tmp00 = A[i+0];
    bsg_fence();  // this fence makes sure that next packets don't block the network.
    register int tmp01 = A[i+1];
    register int tmp02 = A[i+2];
    register int tmp03 = A[i+3];
    register int tmp04 = A[i+4];
    register int tmp05 = A[i+5];
    register int tmp06 = A[i+6];
    register int tmp07 = A[i+7];
    register int tmp08 = A[i+8];
    register int tmp09 = A[i+9];
    register int tmp10 = A[i+10];
    register int tmp11 = A[i+11];
    register int tmp12 = A[i+12];
    register int tmp13 = A[i+13];
    register int tmp14 = A[i+14];
    register int tmp15 = A[i+15];
    asm volatile("": : :"memory");
    B[i+0] = tmp00;
    bsg_fence();  // this fence makes sure that next packets don't block the network.
    B[i+1] = tmp01;
    B[i+2] = tmp02;
    B[i+3] = tmp03;
    B[i+4] = tmp04;
    B[i+5] = tmp05;
    B[i+6] = tmp06;
    B[i+7] = tmp07;
    B[i+8] = tmp08;
    B[i+9] = tmp09;
    B[i+10] = tmp10;
    B[i+11] = tmp11;
    B[i+12] = tmp12;
    B[i+13] = tmp13;
    B[i+14] = tmp14;
    B[i+15] = tmp15;
  }

  bsg_fence();
  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();

  return 0;
}
