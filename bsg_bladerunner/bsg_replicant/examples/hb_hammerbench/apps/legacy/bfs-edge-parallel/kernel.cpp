#include <atomic>
#include <algorithm>
#include "bsg_manycore.h"
#include "bsg_manycore.hpp"
#include "bsg_manycore_atomic.h"
#include "bsg_cuda_lite_barrier.h"

#define BLOCK_SIZE 16

// DRAM variables;
__attribute__((section(".dram"))) int g_q;
__attribute__((section(".dram"))) int g_curr_frontier_size;
__attribute__((section(".dram"))) int g_rev_not_fwd;
__attribute__((section(".dram"))) int g_mf;
__attribute__((section(".dram"))) int g_mu;

// Dram variables to do edge parallel fwd traversal;
__attribute__((section(".dram"))) int g_q2;
__attribute__((section(".dram"))) int g_edge_start[128];
__attribute__((section(".dram"))) int g_edge_stop[128];


// DMEM variables;
int local_curr_frontier_size;


// zero out dense frontier;
inline void zero_out_dense(int V, int* dense_frontier)
{
  int len = (V+31)/32;
  bsg_unroll(1)
  for (int i = BLOCK_SIZE*__bsg_id; i < len; i+=bsg_tiles_X*bsg_tiles_Y*BLOCK_SIZE) {
    dense_frontier[i+0] = 0; 
    dense_frontier[i+1] = 0; 
    dense_frontier[i+2] = 0; 
    dense_frontier[i+3] = 0; 
    dense_frontier[i+4] = 0; 
    dense_frontier[i+5] = 0; 
    dense_frontier[i+6] = 0; 
    dense_frontier[i+7] = 0; 
    dense_frontier[i+8] = 0; 
    dense_frontier[i+9] = 0; 
    dense_frontier[i+10] = 0; 
    dense_frontier[i+11] = 0; 
    dense_frontier[i+12] = 0; 
    dense_frontier[i+13] = 0; 
    dense_frontier[i+14] = 0; 
    dense_frontier[i+15] = 0; 
  } 
}

// set a bit in dense bit vector;
inline void insert_into_dense(int id, int *dense_frontier)
{
    int word_idx = id/32;
    int bit_idx = id%32;
    int w = (1<<bit_idx);
    bsg_amoor(&dense_frontier[word_idx], w);
}


// Convert to dense frontier;
inline void convert_to_dense(int V, int * dense_frontier, int* sparse_frontier)
{
  // zero out;
  zero_out_dense(V, dense_frontier);
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
  // convert;
  int l_curr_frontier_size = g_curr_frontier_size;
  for (int i = __bsg_id; i < l_curr_frontier_size; i+=bsg_tiles_X*bsg_tiles_Y) {
    insert_into_dense(sparse_frontier[i], dense_frontier);
  }
}


// Convert from dense frontier to sparse frontier;
inline void convert_to_sparse(int V, int* sparse_frontier, int* dense_frontier)
{
  // reset
  if (__bsg_id == 0) {
    g_curr_frontier_size = 0;
    bsg_fence();
  }
  bsg_barrier_hw_tile_group_sync();

  // keep track locally;
  int local_frontier[BLOCK_SIZE*32];
  int local_words[BLOCK_SIZE];

  // statically allocating job;
  bsg_unroll(1)
  for (int bit_start = __bsg_id*BLOCK_SIZE*32; bit_start < V; bit_start+=BLOCK_SIZE*32*bsg_tiles_X*bsg_tiles_Y) {
    int local_frontier_count = 0;

    // load words;
    int word_start = bit_start/32;
    int tmp00 = dense_frontier[word_start+0];
    int tmp01 = dense_frontier[word_start+1];
    int tmp02 = dense_frontier[word_start+2];
    int tmp03 = dense_frontier[word_start+3];
    int tmp04 = dense_frontier[word_start+4];
    int tmp05 = dense_frontier[word_start+5];
    int tmp06 = dense_frontier[word_start+6];
    int tmp07 = dense_frontier[word_start+7];
    int tmp08 = dense_frontier[word_start+8];
    int tmp09 = dense_frontier[word_start+9];
    int tmp10 = dense_frontier[word_start+10];
    int tmp11 = dense_frontier[word_start+11];
    int tmp12 = dense_frontier[word_start+12];
    int tmp13 = dense_frontier[word_start+13];
    int tmp14 = dense_frontier[word_start+14];
    int tmp15 = dense_frontier[word_start+15];
    asm volatile("": : :"memory");
    local_words[0]  = tmp00;
    local_words[1]  = tmp01;
    local_words[2]  = tmp02;
    local_words[3]  = tmp03;
    local_words[4]  = tmp04;
    local_words[5]  = tmp05;
    local_words[6]  = tmp06;
    local_words[7]  = tmp07;
    local_words[8]  = tmp08;
    local_words[9]  = tmp09;
    local_words[10] = tmp10;
    local_words[11] = tmp11;
    local_words[12] = tmp12;
    local_words[13] = tmp13;
    local_words[14] = tmp14;
    local_words[15] = tmp15;
    asm volatile("": : :"memory");

    // check if visited, and store locally and increment count;
    bsg_unroll(1)
    for (int i = 0; i < BLOCK_WORDS; i++) {
      int curr_word = local_words[i];
      bsg_unroll(1)
      for (int j = 0 ; j < 32; j++) {
        int curr_bit = curr_word & (1 << j);
        if (curr_bit) {
          local_frontier[local_frontier_count] = bit_start+(32*i)+j;
          local_frontier_count++;
        }
      }
    }

    // allocate space on curr_frontier 
    int start_idx = bsg_amoadd(&g_curr_frontier_size, local_frontier_count);
    bsg_unroll(1)
    for (int i = 0; i < local_frontier_count; i++) {
      sparse_frontier[start_idx+i] = local_frontier[i];   
    }
  }
}


// check if id is in the denser frontier;
inline int is_in_dense(int id, int* dense_frontier) {
  int word_idx = id/32;
  int bit_idx = id%32;
  int w = dense_frontier[word_idx];
  return w & (1<<bit_idx);
}


// Kernel;
extern "C" int kernel(
  int V,
  int root,
  int* fwd_offsets, int* fwd_nonzeros,
  int* rev_offsets, int* rev_nonzeros,
  int* sparse_frontier,
  int* dense_frontier0, int* dense_frontier1,
  int* distance
)
{
  // Initialize;
  bsg_barrier_hw_tile_group_init();
  if (__bsg_id == 0) {
    distance[root] = 0;
    sparse_frontier[0] = root;
    g_curr_frontier_size = 1;
    g_rev_not_fwd = 0;
    bsg_fence();
  }
  int curr_dense_valid = 0;
  int *curr_dense_frontier = dense_frontier0;
  int *next_dense_frontier = dense_frontier1;
  bsg_barrier_hw_tile_group_sync();

  // Kernel start;
  bsg_cuda_print_stat_kernel_start();

  // curr distance;
  int d = 0;

  // main loop;
  while (g_curr_frontier_size != 0)
  {
    d++;

    // Determine direction;
    if (d != 1) {
      if (g_rev_not_fwd) {
        bsg_barrier_hw_tile_group_sync();
        // switch rev -> fwd?
        if (__bsg_id == 0) {
          g_rev_not_fwd = (g_curr_frontier_size >= V/20);
          bsg_fence();
        }
      } else {
        // switch fwd -> rev?
        if (__bsg_id == 0) {
          g_mf = 0;
          g_mu = 0;
          bsg_fence();
        }
        bsg_barrier_hw_tile_group_sync();

        // count mf;
        int l_curr_frontier_size = g_curr_frontier_size;
        int mf_local = 0;
        bsg_unroll(1)
        for (int i = __bsg_id; i < l_curr_frontier_size; i+=bsg_tiles_X*bsg_tiles_Y) {
          int src = sparse_frontier[i];
          mf_local += (fwd_offsets[src+1] - fwd_offsets[src]);
        }
        bsg_amoadd(&g_mf, mf_local);

        // count mu;
        int mu_local = 0;
        bsg_unroll(1)
        for (int v_start = __bsg_id*BLOCK_SIZE; v_start < V; v_start+=bsg_tiles_X*bsg_tiles_Y*BLOCK_SIZE) {
          int v = v_start;
          int stop = std::min(V, v_start+BLOCK_SIZE);

          // unroll by 2
          #define MU_UNROLL 4
          for (; v+MU_UNROLL<=stop; v+=MU_UNROLL) {
            register int l_distance0[MU_UNROLL];
            register int l_fwd_offsets[MU_UNROLL+1];
            l_distance0[0] = distance[v];
            l_distance0[1] = distance[v+1];
            l_distance0[2] = distance[v+2];
            l_distance0[3] = distance[v+3];
            asm volatile("": : :"memory");
            l_fwd_offsets[0] = fwd_offsets[v];
            l_fwd_offsets[1] = fwd_offsets[v+1];
            l_fwd_offsets[2] = fwd_offsets[v+2];
            l_fwd_offsets[3] = fwd_offsets[v+3];
            l_fwd_offsets[4] = fwd_offsets[v+4];
            asm volatile("": : :"memory");
            if (l_distance0[0] == -1) {
              mu_local += l_fwd_offsets[1]-l_fwd_offsets[0];
            }     
            if (l_distance0[1] == -1) {
              mu_local += l_fwd_offsets[2]-l_fwd_offsets[1];
            }     
            if (l_distance0[2] == -1) {
              mu_local += l_fwd_offsets[3]-l_fwd_offsets[2];
            }     
            if (l_distance0[3] == -1) {
              mu_local += l_fwd_offsets[4]-l_fwd_offsets[3];
            }     
          }
        
          // unroll by 1;
          for (; v < stop; v++) {
            if (distance[v] == -1) {
              mu_local += fwd_offsets[v+1]-fwd_offsets[v];
            }
          }
        }

        bsg_amoadd(&g_mu, mu_local);
        bsg_fence();
        bsg_barrier_hw_tile_group_sync();
      
        if (__bsg_id == 0) {
          g_rev_not_fwd = g_mf > (g_mu/20);
          bsg_fence();
        }
      }
    }

    bsg_barrier_hw_tile_group_sync();

    // Prepare to traverse;
    if (__bsg_id == 0) {
      //bsg_print_int(d);
      //bsg_print_int(g_rev_not_fwd);
      g_q = 0; 
      g_q2 = 0; 
    }
    zero_out_dense(V, next_dense_frontier);
    bsg_fence();
    bsg_barrier_hw_tile_group_sync();

    if (g_rev_not_fwd) {
      // REV traversal
      // convert if necessary;
      if (curr_dense_valid == 0) {
        convert_to_dense(V, curr_dense_frontier, sparse_frontier);
        curr_dense_valid = 1;
        bsg_fence();
        bsg_barrier_hw_tile_group_sync();
      }

      // iterate through unvisited nodes;

      for (int v_start = bsg_amoadd(&g_q,BLOCK_SIZE); v_start < V; v_start = bsg_amoadd(&g_q,BLOCK_SIZE)) {
        int v = v_start;
        int stop = std::min(V, v_start+BLOCK_SIZE);
        //int dst = start;
        //int len = stop-start;


        // unroll by 2;
        #define REV_UNROLL 2
        for (;v+REV_UNROLL <= stop; v+=REV_UNROLL) {
          register int l_distance[REV_UNROLL];
          register int l_rev_offsets[REV_UNROLL+1];
          l_distance[0] = distance[v];
          l_distance[1] = distance[v+1];
          //l_distance[2] = distance[v+2];
          //l_distance[3] = distance[v+3];
          asm volatile("": : :"memory");
          l_rev_offsets[0] = rev_offsets[v];
          l_rev_offsets[1] = rev_offsets[v+1];
          l_rev_offsets[2] = rev_offsets[v+2];
          //l_rev_offsets[0] = rev_offsets[v+3];
          //l_rev_offsets[0] = rev_offsets[v+4];
          asm volatile("": : :"memory");
          // 0
          if (l_distance[0] == -1) {
            int nz_start = l_rev_offsets[0];
            int nz_stop  = l_rev_offsets[1];
            for (int nz = nz_start; nz < nz_stop; nz++) {
              int src = rev_nonzeros[nz];
              if (is_in_dense(src, curr_dense_frontier)) {
                distance[v] = d;
                insert_into_dense(v, next_dense_frontier);
                break;
              }
            }
          }
          // 1
          if (l_distance[1] == -1) {
            int nz_start = l_rev_offsets[1];
            int nz_stop  = l_rev_offsets[2];
            for (int nz = nz_start; nz < nz_stop; nz++) {
              int src = rev_nonzeros[nz];
              if (is_in_dense(src, curr_dense_frontier)) {
                distance[v+1] = d;
                insert_into_dense(v+1, next_dense_frontier);
                break;
              }
            }
          }
        }

        // unroll by 1
        for (; v < stop; v++) {
          if (distance[v] == -1) {
            int nz_start = rev_offsets[v];
            int nz_stop = rev_offsets[v+1];
            for (int nz = nz_start; nz < nz_stop; nz++) {
              int src0 = rev_nonzeros[nz];
              if (is_in_dense(src0, curr_dense_frontier)) {
                distance[v] = d;
                insert_into_dense(v, next_dense_frontier);
                break;
              }
            }
          }
        }

      }

    } else {
      // FWD traversal
      int l_curr_frontier_size = g_curr_frontier_size;
      int do_edge_parallel = l_curr_frontier_size < 128;

      bsg_unroll(1)
      for (int idx = bsg_amoadd(&g_q,1); idx < l_curr_frontier_size; idx = bsg_amoadd(&g_q,1)) {
        int src = sparse_frontier[idx]; 
        // nz boundary
        int nz_start = fwd_offsets[src];
        int nz_stop = fwd_offsets[src+1];
        int nz_len = nz_stop - nz_start;
        if (do_edge_parallel && (nz_len > 128)) {
          int next_idx = bsg_amoadd(&g_q2,1); 
          g_edge_start[next_idx] = nz_start;
          g_edge_stop[next_idx] = nz_stop;
        } else {
          // Forward traversal (vertex parallel);
          int nz = nz_start;
          for (; nz < nz_stop; nz++) {
            int dst = fwd_nonzeros[nz];
            if (distance[dst] == -1) {
              distance[dst] = d;
              insert_into_dense(dst, next_dense_frontier);
            }
          }
        }
      }

      bsg_fence();
      bsg_barrier_hw_tile_group_sync();

      // Edge parallel
      if (do_edge_parallel) {
        int l_q2 = g_q2;
        for (int idx = 0; idx < l_q2; idx++) {
          int nz_start = g_edge_start[idx];
          int nz_stop = g_edge_stop[idx];
          for (int nz = nz_start+__bsg_id; nz < nz_stop; nz+=bsg_tiles_X*bsg_tiles_Y) {
            int dst = fwd_nonzeros[nz];
            if (distance[dst] == -1) {
              distance[dst] = d;
              insert_into_dense(dst, next_dense_frontier);
            }
          }
          bsg_fence();
          bsg_barrier_hw_tile_group_sync();
        }
      }
    }

    bsg_fence();
    bsg_barrier_hw_tile_group_sync();

    // convert to sparse
    convert_to_sparse(V, sparse_frontier, next_dense_frontier);

    // swap frontier;
    int *tmp = curr_dense_frontier;
    curr_dense_frontier = next_dense_frontier;
    next_dense_frontier = tmp;
    curr_dense_valid = 1;
    bsg_fence();
    bsg_barrier_hw_tile_group_sync();
  }

  bsg_fence();
  bsg_barrier_hw_tile_group_sync();


  // Kernel end;
  //bsg_fence();
  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
  return 0;
}
