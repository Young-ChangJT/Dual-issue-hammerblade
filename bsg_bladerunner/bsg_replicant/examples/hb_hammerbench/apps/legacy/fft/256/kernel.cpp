// 256x256 FFT using four-step method
// In this implementation all tiles write back the intermediate 256-point
// FFT results to the DRAM

#include <bsg_manycore.h>
#include <bsg_set_tile_x_y.h>
#include <bsg_cuda_lite_barrier.h>

#include <fft256.hpp>

#define NUM_TILES (bsg_tiles_X*bsg_tiles_Y)

// Cache warming function.
// Can only fit A and TW in the cache.
#ifdef WARM_CACHE
#define CACHE_LINE_COMPLEX 8
__attribute__ ((noinline))
static void warmup(FP32Complex *in, FP32Complex* tw, int N)
{
  for (int i = __bsg_id*CACHE_LINE_COMPLEX; i < N; i += bsg_tiles_X*bsg_tiles_Y*CACHE_LINE_COMPLEX) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (in[i]));
  }
  for (int i = __bsg_id*CACHE_LINE_COMPLEX; i < N; i += bsg_tiles_X*bsg_tiles_Y*CACHE_LINE_COMPLEX) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (tw[i]));
  }
  bsg_fence();
}
#endif



FP32Complex fft_workset[NUM_POINTS];

extern "C" __attribute__ ((noinline))
int
kernel_fft(FP32Complex * bsg_attr_noalias  in,
           FP32Complex * bsg_attr_noalias out,
           FP32Complex * bsg_attr_noalias tw,
           int N,
           int num_iter // how many of batches of fft?
            ) {

    bsg_barrier_hw_tile_group_init();
    bsg_fence();

    #ifdef WARM_CACHE
    warmup(in, tw, N);
    #endif
    bsg_barrier_hw_tile_group_sync();

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
      bsg_barrier_hw_tile_group_sync();



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
      bsg_barrier_hw_tile_group_sync();
    }


    // Kernel end
    bsg_cuda_print_stat_kernel_end();
    bsg_fence();
    bsg_barrier_hw_tile_group_sync();



    return 0;
}
