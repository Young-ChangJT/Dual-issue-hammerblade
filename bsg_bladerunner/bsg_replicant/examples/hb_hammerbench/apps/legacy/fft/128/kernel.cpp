// 128x128 FFT using four-step method

#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>
#include "fft128.hpp"

// Cache warming function
#ifdef WARM_CACHE
#define CACHE_LINE_COMPLEX 8
__attribute__ ((noinline))
static void warmup(FP32Complex *in, FP32Complex *out, FP32Complex *tw, int N)
{
  for (int i = __bsg_id*CACHE_LINE_COMPLEX; i < N; i += bsg_tiles_X*bsg_tiles_Y*CACHE_LINE_COMPLEX) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (in[i]));
      //asm volatile ("sw x0, %[p]" :: [p] "m" (out[i]));
  }
  //for (int i = __bsg_id*CACHE_LINE_COMPLEX; i < N; i += bsg_tiles_X*bsg_tiles_Y*CACHE_LINE_COMPLEX) {
  //    asm volatile ("lw x0, %[p]" :: [p] "m" (tw[i]));
  //}
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
}
#endif


FP32Complex fft_workset[NUM_POINTS];
FP32Complex local_tw[NUM_POINTS];

extern "C" __attribute__ ((noinline))
int
kernel_fft(FP32Complex * bsg_attr_noalias in,
           FP32Complex * bsg_attr_noalias out,
           FP32Complex * bsg_attr_noalias tw,
           int N,
           int num_iter) 
{
    bsg_barrier_hw_tile_group_init();
    bsg_fence();

    #ifdef WARM_CACHE
    warmup(in, out, tw, N);
    #endif

    // load twiddle factor to local.
    load_sequential(local_tw, tw+(__bsg_id*NUM_POINTS));
    bsg_fence();
    bsg_barrier_hw_tile_group_sync();


    // Kernel Start
    bsg_cuda_print_stat_kernel_start();
    
    for (int i = 0; i < num_iter; i++) {
      FP32Complex *input_sq = &in[i*N];
      FP32Complex *input_vec = input_sq + __bsg_id;

      // step 1
      load_strided(fft_workset, input_vec);
      fft128_specialized(fft_workset);
      twiddle_scaling(fft_workset, local_tw);
      store_strided(input_vec, fft_workset);
      bsg_fence();
      bsg_barrier_hw_tile_group_sync();

      // step 2
      //tg_mem_square_transpose_inp(fft_workset, 128);
      //bsg_barrier_hw_tile_group_sync();

      // step 3
      input_vec = input_sq + (__bsg_id * NUM_POINTS);
      FP32Complex *output_sq = &out[i*N];
      FP32Complex *output_vec = output_sq + __bsg_id;
      load_sequential(fft_workset, input_vec);
      fft128_specialized(fft_workset);
      store_strided(output_vec, fft_workset);
      bsg_fence();
      bsg_barrier_hw_tile_group_sync();
    }


    // Kernel end
    bsg_cuda_print_stat_kernel_end();
    bsg_fence();
    bsg_barrier_hw_tile_group_sync();

    return 0;
}
