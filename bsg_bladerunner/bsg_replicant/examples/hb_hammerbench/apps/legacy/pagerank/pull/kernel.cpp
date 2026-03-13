#define GRANULARITY_PULL 10
#include <bsg_manycore.h>
#include <bsg_tile_group_barrier.hpp>
#include <atomic>
#include <math.h>
#include <local_range.h>
#include <vertex_struct.h>
#include <cstring>
#include <numeric>
#include <array>

//#define DEBUG

#ifdef DEBUG
#define pr_dbg(fmt, ...)                        \
    do{ if (__bsg_id == 0) bsg_printf(fmt, ##__VA_ARGS__); } while (0)
#else
#define pr_dbg(fmt, ...)
#endif

//keep these:
__attribute__((section(".dram"))) float damp;
__attribute__((section(".dram"))) float beta_score;

__attribute__((section(".dram"))) std::atomic<int> workq;

float damp_dmem;
float beta_score_dmem;

/*
extern "C"
int __attribute__ ((noinline)) pagerank_pull(int bsg_attr_remote * bsg_attr_noalias in_indices,
                                             int bsg_attr_remote * bsg_attr_noalias in_neighbors,
                                             int bsg_attr_remote * bsg_attr_noalias out_degree,
                                             float bsg_attr_remote * bsg_attr_noalias old_rank,
                                             float bsg_attr_remote * bsg_attr_noalias new_rank,
                                             float bsg_attr_remote * bsg_attr_noalias contrib,
                                             float bsg_attr_remote * bsg_attr_noalias contrib_new,
                                             int V) {
  bsg_barrier_hw_tile_group_init();
  bsg_cuda_print_stat_kernel_start();

  int start = 0;
  int end = V;
  int length = end - start;
  for(int id = workq.fetch_add(GRANULARITY_PULL, std::memory_order_relaxed); id < length; id = workq.fetch_add(GRANULARITY_PULL, std::memory_order_relaxed)) {
    int stop = (id + GRANULARITY_PULL) > length ? length : (id + GRANULARITY_PULL);
    for (int d = start + id; d < start + stop; d++) {
      register float temp_new = 0.0;
      register float temp_old = old_rank[d];
      register float error = 0.0;
      register int first = in_indices[d];
      register int last = in_indices[d+1];
      register int od = out_degree[d];
      int s = first;
      for(; s < last; s++) {
        register int idx = in_neighbors[s];
        register float tmp = contrib[idx];
        temp_new = temp_new + tmp;
      }

      temp_new = beta_score + damp * temp_new;
      error = fabs(temp_new - temp_old);
      old_rank[d] = temp_new;
      contrib_new[d] = temp_new / od;
    }
  }

  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
  return 0;
}
*/


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


extern "C"
int __attribute__ ((noinline)) pagerank_pull_u8(int bsg_attr_remote * bsg_attr_noalias in_indices,
                                                int bsg_attr_remote * bsg_attr_noalias in_neighbors,
                                                int bsg_attr_remote * bsg_attr_noalias out_degree,
                                                float bsg_attr_remote * bsg_attr_noalias old_rank,
                                                float bsg_attr_remote * bsg_attr_noalias new_rank,
                                                float bsg_attr_remote * bsg_attr_noalias contrib,
                                                float bsg_attr_remote * bsg_attr_noalias contrib_new,
                                                int V) {
  bsg_barrier_hw_tile_group_init();
  damp_dmem = damp;
  beta_score_dmem = beta_score;
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
  bsg_cuda_print_stat_kernel_start();

  int start = 0;
  int end = V;
  int length = end - start;
  for(int id = workq.fetch_add(GRANULARITY_PULL, std::memory_order_relaxed); id < length; id = workq.fetch_add(GRANULARITY_PULL, std::memory_order_relaxed)) {
    int stop = (id + GRANULARITY_PULL) > length ? length : (id + GRANULARITY_PULL);

    //float fdiv_result;
    int d = start + id;
    for( ; d < start + stop; d++) {
      float temp_new = 0.0f;
      int first = in_indices[d];
      int last = in_indices[d+1];
      // calculate inverse now;
      float od = (float) out_degree[d];
      float onef = 1.0f;
      float od_inv;
      fdiv_asm(od_inv, onef, od);
      asm volatile ("" ::: "memory");

      int s = first;
      for(;s + 7 < last; s += 8) {
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
      for(;s + 3 < last; s += 4) {
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
      for(;s + 2 < last; s += 3) {
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
      for(;s + 1 < last; s += 2) {
              int idx0 = in_neighbors[s + 0];
              int idx1 = in_neighbors[s + 1];
              asm volatile ("" ::: "memory");
              float c0 = contrib[idx0];
              float c1 = contrib[idx1];
              asm volatile ("" ::: "memory");
              temp_new += c0 + c1;
      }
      for(; s < last; ++s){
              int idx0 = in_neighbors[s];
              float c0 = contrib[idx0];
              temp_new += c0;
      }

      fmadd_asm(temp_new, temp_new, damp_dmem, beta_score_dmem);
      contrib_new[d] = temp_new * od_inv;
      old_rank[d] = temp_new;
    }

  }

  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
  return 0;
}

