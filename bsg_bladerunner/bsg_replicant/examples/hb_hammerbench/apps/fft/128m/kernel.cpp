#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>
#include "fft128.hpp"
#include "bsg_barrier_multipod.h"

#define N (NUM_POINTS*NUM_POINTS)
#define NUM_TILES (bsg_tiles_X*bsg_tiles_Y)

// Multipod barrier;
volatile int done[NUM_POD_X]={0};
int alert = 0;


// Cache warming function
#ifdef WARM_CACHE
#define CACHE_LINE_COMPLEX 8
__attribute__ ((noinline))
static void warmup(FP32Complex *in, FP32Complex *out, FP32Complex *tw, int n)
{
  for (int i = __bsg_id*CACHE_LINE_COMPLEX; i < n; i += NUM_TILES*CACHE_LINE_COMPLEX) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (in[i]));
  }
}
#endif


FP32Complex fft_workset[NUM_POINTS];
FP32Complex local_tw[NUM_POINTS];


// Kernel main;
extern "C" int
kernel(FP32Complex *in,
       FP32Complex *out,
       FP32Complex *tw,
       int num_iter,
       int pod_id) 
{
    bsg_barrier_tile_group_init();
    #ifdef WARM_CACHE
    warmup(in, out, tw, N);
    #endif
    bsg_fence();
    bsg_barrier_tile_group_sync();


    // load twiddle factor to local.
    //load_sequential(local_tw, tw+(my_id*NUM_POINTS));
    bsg_fence();
    bsg_barrier_multipod(pod_id, NUM_POD_X, done, &alert);

    // Kernel Start
    bsg_cuda_print_stat_kernel_start();
    
    for (int i = 0; i < num_iter; i++) {
      // step 1
      FP32Complex *input_sq = &in[i*N];
      for (int j = __bsg_id; j < NUM_POINTS; j+=NUM_TILES) {
        FP32Complex *input_vec = input_sq + j;
        load_strided(fft_workset, input_vec);
        load_sequential(local_tw, tw+(j*NUM_POINTS));
        fft128_specialized(fft_workset);
        twiddle_scaling(fft_workset, local_tw);
        store_strided(input_vec, fft_workset);
      }
      bsg_fence();
      bsg_barrier_tile_group_sync();

      // step 2
      FP32Complex *output_sq = &out[i*N];
      for (int j = __bsg_id; j < NUM_POINTS; j+=NUM_TILES) {
        FP32Complex *input_vec = input_sq + (j * NUM_POINTS);
        FP32Complex *output_vec = output_sq + j;
        load_sequential(fft_workset, input_vec);
        fft128_specialized(fft_workset);
        store_strided(output_vec, fft_workset);
      }
      bsg_fence();
      bsg_barrier_tile_group_sync();
    }


    // Kernel end
    bsg_cuda_print_stat_kernel_end();
    bsg_fence();
    bsg_barrier_tile_group_sync();

    return 0;
}
