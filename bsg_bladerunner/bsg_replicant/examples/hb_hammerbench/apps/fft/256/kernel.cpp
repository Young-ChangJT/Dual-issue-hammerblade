#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>
#include "fft256.hpp"
#include "bsg_barrier_multipod.h"

#define N (NUM_POINTS*NUM_POINTS)
#define NUM_TILES (bsg_tiles_X*bsg_tiles_Y)

// Multipod barrier;
volatile int done[NUM_POD_X]={0};
int alert = 0;


// Cache warming function.
// Can only fit A and TW in the cache.
#ifdef WARM_CACHE
#define CACHE_LINE_COMPLEX 8
__attribute__ ((noinline))
static void warmup(FP32Complex *in, FP32Complex* tw, int n)
{
  for (int i = __bsg_id*CACHE_LINE_COMPLEX; i < n; i += NUM_TILES*CACHE_LINE_COMPLEX) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (in[i]));
  }
  for (int i = __bsg_id*CACHE_LINE_COMPLEX; i < n; i += NUM_TILES*CACHE_LINE_COMPLEX) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (tw[i]));
  }
}
#endif


// Local storage;
FP32Complex fft_workset[NUM_POINTS];


// Kernel main;
extern "C" int
kernel(FP32Complex * in,
           FP32Complex * out,
           FP32Complex * tw,
           int num_iter, // how many of batches of fft?
           int pod_id)
{
    bsg_barrier_tile_group_init();
    #ifdef WARM_CACHE
    warmup(in, tw, N);
    #endif
    bsg_fence();
    bsg_barrier_tile_group_sync();

    // Kernel start;
    bsg_barrier_multipod(pod_id, NUM_POD_X, done, &alert);
    bsg_cuda_print_stat_kernel_start();

    for (int i = 0; i < num_iter; i++) {
      FP32Complex *input_sq  = &in[i*N];
      // Step 1
      for (int iter = 0; iter < (NUM_POINTS/NUM_TILES); iter++) {
        FP32Complex* input_vec = input_sq + (iter*NUM_TILES) + __bsg_id;
        // load strided
        load_strided(fft_workset, input_vec);
        // fft256
        fft256_specialized(fft_workset);
        // twiddle scaling
        twiddle_scaling(fft_workset, tw+(((iter*NUM_TILES)+__bsg_id)*NUM_POINTS));
        // store strided
        store_strided(input_vec, fft_workset);
      }
      asm volatile("": : :"memory");
      bsg_barrier_tile_group_sync();



      // step 2
      FP32Complex *output_sq = &out[i*N];
      for (int iter = 0; iter < (NUM_POINTS/NUM_TILES); iter++) {
        FP32Complex* input_vec = input_sq + (((iter * NUM_TILES) + __bsg_id) * NUM_POINTS);
        FP32Complex* output_vec = output_sq + (iter * NUM_TILES) + __bsg_id;
        // load sequential
        load_sequential(fft_workset, input_vec);
        // fft256
        fft256_specialized(fft_workset);
        // store strided
        store_strided(output_vec, fft_workset);
      }
      bsg_barrier_tile_group_sync();
    }


    // Kernel end
    bsg_cuda_print_stat_kernel_end();
    bsg_fence();
    bsg_barrier_tile_group_sync();



    return 0;
}
