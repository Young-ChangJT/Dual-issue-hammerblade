#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>
#include "bs_kernel.hpp"
#include "option_data.hpp"
#include "bsg_barrier_multipod.h"

#define NUM_TILES (bsg_tiles_X*bsg_tiles_Y)

// Multipod barrier;
volatile int done[NUM_POD_X]={0};
int alert = 0;


// Cache warm;
void warmup(OptionData *data, int numOptions) {
  for (int i = __bsg_id; i < numOptions; i += NUM_TILES) {
    asm volatile ("lw x0, %[p]" :: [p] "m" (data[i]));
  }
}

// Local storage;
#define CHUNK_SIZE 4
OptionData local_options[CHUNK_SIZE];


// Kernel main;
extern "C"
int kernel(OptionData *data, int num_option, int pod_id)
{
  bsg_barrier_tile_group_init();
  bsg_barrier_tile_group_sync();
  warmup(data, num_option);
  bsg_fence();

  // kernel start;
  bsg_barrier_multipod(pod_id, NUM_POD_X, done, &alert);
  bsg_cuda_print_stat_kernel_start();

  for (int i = __bsg_id*CHUNK_SIZE; i < num_option; i+=NUM_TILES*CHUNK_SIZE) {
    // Copy from DRAM;
    float s0 = data[i+0].s;
    float strike0 = data[i+0].strike;
    float r0 = data[i+0].r;
    float v0 = data[i+0].v;
    float t0 = data[i+0].t;

    float s1 = data[i+1].s;
    float strike1 = data[i+1].strike;
    float r1 = data[i+1].r;
    float v1 = data[i+1].v;
    float t1 = data[i+1].t;

    float s2 = data[i+2].s;
    float strike2 = data[i+2].strike;
    float r2 = data[i+2].r;
    float v2 = data[i+2].v;
    float t2 = data[i+2].t;

    float s3 = data[i+3].s;
    float strike3 = data[i+3].strike;
    float r3 = data[i+3].r;
    float v3 = data[i+3].v;
    float t3 = data[i+3].t;
    asm volatile("" ::: "memory");

    local_options[0].s = s0;
    local_options[0].strike = strike0;
    local_options[0].r = r0;
    local_options[0].v = v0;
    local_options[0].t = t0;

    local_options[1].s = s1;
    local_options[1].strike = strike1;
    local_options[1].r = r1;
    local_options[1].v = v1;
    local_options[1].t = t1;

    local_options[2].s = s2;
    local_options[2].strike = strike2;
    local_options[2].r = r2;
    local_options[2].v = v2;
    local_options[2].t = t2;

    local_options[3].s = s3;
    local_options[3].strike = strike3;
    local_options[3].r = r3;
    local_options[3].v = v3;
    local_options[3].t = t3;
    asm volatile("" ::: "memory");

    // Compute;
    for (int j = 0; j < CHUNK_SIZE; j++) {
      BlkSchlsEqEuroNoDiv_kernel(&local_options[j]);
    }

    // write back;
    data[i+0].call = local_options[0].call;
    data[i+0].put  = local_options[0].put;
    data[i+1].call = local_options[1].call;
    data[i+1].put  = local_options[1].put;
    data[i+2].call = local_options[2].call;
    data[i+2].put  = local_options[2].put;
    data[i+3].call = local_options[3].call;
    data[i+3].put  = local_options[3].put;
  }


  // Kernel end;
  bsg_fence();
  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_tile_group_sync();
  return 0;
}