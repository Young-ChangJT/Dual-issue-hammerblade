#include <bsg_cuda_lite_barrier.h>
#include "bsg_barrier_multipod.h"
#include <bsg_manycore_atomic.h>
#include <bsg_manycore.h>


#define fmadd_asm(rd_p, rs1_p, rs2_p, rs3_p) \
    asm volatile ("fmadd.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (rd_p) \
      : [rs1] "f" ((rs1_p)), [rs2] "f" ((rs2_p)), [rs3] "f" ((rs3_p)))

#define fadd_asm(rd_p, rs1_p, rs2_p) \
    asm volatile ("fadd.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (rd_p) \
      : [rs1] "f" ((rs1_p)), [rs2] "f" ((rs2_p)))

#define fdiv_asm(rd_p, rs1_p, rs2_p) \
    asm volatile ("fdiv.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (rd_p) \
      : [rs1] "f" ((rs1_p)), [rs2] "f" ((rs2_p)))

// Const in DMEM;
float damp = 0.85f;
float beta_score = (1.0f - damp) / (float) EDGE;

// multipod barrier;
volatile int done[NUM_POD_X] = {0};
int alert = 0;


// Kernel main;
extern "C" int kernel(
  int* in_indices,
  int* in_neighbors,
  float* out_degree_inv,
  float* new_rank,
  float* contrib,
  float* contrib_new,
  int* start_id,
  int end_id,
  int pod_id
)
{
  bsg_barrier_tile_group_init();
  bsg_barrier_tile_group_sync();
  // prepare here;
  bsg_barrier_multipod(pod_id, NUM_POD_X, done, &alert);
  bsg_cuda_print_stat_kernel_start();

  for (int curr_id = bsg_amoadd(start_id,1); curr_id < end_id; curr_id = bsg_amoadd(start_id,1)) {
    float temp_new = 0.0f;
    // start, end idx;
    int in_start = in_indices[curr_id];
    int in_end = in_indices[curr_id+1];
    // Load inv of out-degree;
    float od_inv = out_degree_inv[curr_id];
    asm volatile ("" ::: "memory");

    int s = in_start;

    // unroll by 8
    for (; s + 7 < in_end; s+=8) {
      int idx0 = in_neighbors[s + 0];
      int idx1 = in_neighbors[s + 1];
      int idx2 = in_neighbors[s + 2];
      int idx3 = in_neighbors[s + 3];
      int idx4 = in_neighbors[s + 4];
      int idx5 = in_neighbors[s + 5];
      int idx6 = in_neighbors[s + 6];
      int idx7 = in_neighbors[s + 7];
      asm volatile ("" ::: "memory");
      float c0 = contrib[idx0];
      float c1 = contrib[idx1];
      float c2 = contrib[idx2];
      float c3 = contrib[idx3];
      float c4 = contrib[idx4];
      float c5 = contrib[idx5];
      float c6 = contrib[idx6];
      float c7 = contrib[idx7];
      asm volatile ("" ::: "memory");
      float t0, t1, t2, t3;
      fadd_asm(t0, c0, c1);
      fadd_asm(t1, c2, c3);
      fadd_asm(t2, c4, c5);
      fadd_asm(t3, c6, c7);
      fadd_asm(t0, t0, t1);
      fadd_asm(t2, t2, t3);
      fadd_asm(t0, t0, t2);
      temp_new += t0;
    }
    
    // unroll by 4
    for (; s + 3 < in_end; s+=4) {
      int idx0 = in_neighbors[s + 0];
      int idx1 = in_neighbors[s + 1];
      int idx2 = in_neighbors[s + 2];
      int idx3 = in_neighbors[s + 3];
      asm volatile ("" ::: "memory");
      float c0 = contrib[idx0];
      float c1 = contrib[idx1];
      float c2 = contrib[idx2];
      float c3 = contrib[idx3];
      asm volatile ("" ::: "memory");
      float t0, t1;
      fadd_asm(t0, c0, c1);
      fadd_asm(t1, c2, c3);
      fadd_asm(t0, t0, t1);
      temp_new += t0;
    }

    // unroll by 3
    for (; s + 2 < in_end; s+=3) {
      int idx0 = in_neighbors[s + 0];
      int idx1 = in_neighbors[s + 1];
      int idx2 = in_neighbors[s + 2];
      asm volatile ("" ::: "memory");
      float c0 = contrib[idx0];
      float c1 = contrib[idx1];
      float c2 = contrib[idx2];
      asm volatile ("" ::: "memory");
      temp_new += c0 + c1 + c2;
    }

    // unroll by 2
    for (; s + 1 < in_end; s+=2) {
      int idx0 = in_neighbors[s + 0];
      int idx1 = in_neighbors[s + 1];
      asm volatile ("" ::: "memory");
      float c0 = contrib[idx0];
      float c1 = contrib[idx1];
      asm volatile ("" ::: "memory");
      temp_new += c0 + c1;
    }

    // no unroll
    for (; s < in_end; s++) {
      int idx0 = in_neighbors[s + 0];
      asm volatile ("" ::: "memory");
      float c0 = contrib[idx0];
      asm volatile ("" ::: "memory");
      temp_new += c0;
    }
    
    fmadd_asm(temp_new, temp_new, damp, beta_score);
    contrib_new[curr_id] = temp_new * od_inv;
    new_rank[curr_id] = temp_new;
  }


  // Kernel end;
  bsg_barrier_tile_group_sync();
  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_tile_group_sync();
  return 0;

}
