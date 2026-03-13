#include <bsg_manycore.h>
#include "bsg_cuda_lite_barrier.h"
#include <stdbool.h>
#include <stdint.h>

__attribute__ ((always_inline)) 
void copySelf(
  float bsg_attr_remote * bsg_attr_noalias src,
  float bsg_attr_remote * bsg_attr_noalias dst,
  int start_idx,
  int nz
)
{
  // set first
  if (start_idx == 0) {
    dst[0] = 0.0f;
  } else {
    dst[0] = src[start_idx-1];
  }

  // set middle
  int line_id = __bsg_id % (LOCAL_SIZE/16);

  bsg_unroll(1)
  for (int i = 0; i < (LOCAL_SIZE/16); i++) {
    float bsg_attr_remote * bsg_attr_noalias temp_dst = &dst[1+(16*line_id)];
    float bsg_attr_remote * bsg_attr_noalias temp_src = &src[start_idx+(16*line_id)];

    register float tmp00 =  temp_src[0];
    register float tmp01 =  temp_src[1];
    register float tmp02 =  temp_src[2];
    register float tmp03 =  temp_src[3];
    register float tmp04 =  temp_src[4];
    register float tmp05 =  temp_src[5];
    register float tmp06 =  temp_src[6];
    register float tmp07 =  temp_src[7];
    register float tmp08 =  temp_src[8];
    register float tmp09 =  temp_src[9];
    register float tmp10 =  temp_src[10];
    register float tmp11 =  temp_src[11];
    register float tmp12 =  temp_src[12];
    register float tmp13 =  temp_src[13];
    register float tmp14 =  temp_src[14];
    register float tmp15 =  temp_src[15];
    asm volatile("": : :"memory");
    temp_dst[0] = tmp00;
    temp_dst[1] = tmp01;
    temp_dst[2] = tmp02;
    temp_dst[3] = tmp03;
    temp_dst[4] = tmp04;
    temp_dst[5] = tmp05;
    temp_dst[6] = tmp06;
    temp_dst[7] = tmp07;
    temp_dst[8] = tmp08;
    temp_dst[9] = tmp09;
    temp_dst[10] = tmp10;
    temp_dst[11] = tmp11;
    temp_dst[12] = tmp12;
    temp_dst[13] = tmp13;
    temp_dst[14] = tmp14;
    temp_dst[15] = tmp15;
    line_id = (line_id+1)%(LOCAL_SIZE/16);
  }

  // set last
  if (start_idx + LOCAL_SIZE == nz) {
    dst[LOCAL_SIZE+1] = 0.0f;
  } else {
    dst[LOCAL_SIZE+1] = src[start_idx+LOCAL_SIZE];
  }
}



__attribute__ ((always_inline)) 
void prefetch_dram(
  float bsg_attr_remote * bsg_attr_noalias a_left,
  float bsg_attr_remote * bsg_attr_noalias a_right,
  float bsg_attr_remote * bsg_attr_noalias a_up,
  float bsg_attr_remote * bsg_attr_noalias a_down,
  int start_idx
)
{
  if (((uintptr_t) a_left) & 0x80000000) {
    float bsg_attr_remote * bsg_attr_noalias ptr = &a_left[start_idx];
    int idx = (__bsg_id % (LOCAL_SIZE/16)) * 16;
    bsg_unroll(1)
    for (int i = 0; i < (LOCAL_SIZE/16); i++) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (ptr[idx]));
      idx = (idx + 16) % LOCAL_SIZE;
    }
  }
  asm volatile("": : :"memory");

  if (((uintptr_t) a_right) & 0x80000000) {
    float bsg_attr_remote * bsg_attr_noalias ptr = &a_right[start_idx];
    int idx = (__bsg_id % (LOCAL_SIZE/16)) * 16;
    bsg_unroll(1)
    for (int i = 0; i < (LOCAL_SIZE/16); i++) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (ptr[idx]));
      idx = (idx + 16) % LOCAL_SIZE;
    }
  }
  asm volatile("": : :"memory");

  if (((uintptr_t) a_up) & 0x80000000) {
    float bsg_attr_remote * bsg_attr_noalias ptr = &a_up[start_idx];
    int idx = (__bsg_id % (LOCAL_SIZE/16)) * 16;
    bsg_unroll(1)
    for (int i = 0; i < (LOCAL_SIZE/16); i++) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (ptr[idx]));
      idx = (idx + 16) % LOCAL_SIZE;
    }
  }
  asm volatile("": : :"memory");

  if (((uintptr_t) a_down) & 0x80000000) {
    float bsg_attr_remote * bsg_attr_noalias ptr = &a_down[start_idx];
    int idx = (__bsg_id % (LOCAL_SIZE/16)) * 16;
    bsg_unroll(1)
    for (int i = 0; i < (LOCAL_SIZE/16); i++) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (ptr[idx]));
      idx = (idx + 16) % LOCAL_SIZE;
    }
  }
  asm volatile("": : :"memory");
}





void compute (
  int c0, int c1,
  float bsg_attr_remote * bsg_attr_noalias dram_self,
  float bsg_attr_remote * bsg_attr_noalias dram_next,
  float bsg_attr_remote * bsg_attr_noalias a_left,
  float bsg_attr_remote * bsg_attr_noalias a_right,
  float bsg_attr_remote * bsg_attr_noalias a_up,
  float bsg_attr_remote * bsg_attr_noalias a_down,
  float bsg_attr_remote * bsg_attr_noalias a_self,
  const int nz
) {

  float c0f = (float) c0;
  float c1f = (float) c1;
  bsg_unroll(1)
  for (int ii = 0; ii < nz; ii += LOCAL_SIZE) {

    copySelf(dram_self, a_self, ii, nz);
    prefetch_dram(a_left, a_right, a_up, a_down, ii);
    bsg_fence();
    bsg_barrier_hw_tile_group_sync();

    // compute 4 at a time
    bsg_unroll(1)
    for (int i = 0; i < LOCAL_SIZE; i += 4) {
      int self_idx = i+1;
      register float left0, left1, left2, left3;
      register float right0, right1, right2, right3;
      register float up0, up1, up2, up3;
      register float down0, down1, down2, down3;

      if (a_left == 0) {
        left0 = 0.0f;
        left1 = 0.0f;
        left2 = 0.0f;
        left3 = 0.0f;
      } else {
        left0 = a_left[i];
        left1 = a_left[i+1];
        left2 = a_left[i+2];
        left3 = a_left[i+3];
      }

      if (a_right == 0) {
        right0 = 0.0f;
        right1 = 0.0f;
        right2 = 0.0f;
        right3 = 0.0f;
      } else {
        right0 = a_right[i];
        right1 = a_right[i+1];
        right2 = a_right[i+2];
        right3 = a_right[i+3];
      }

      if (a_up == 0) {
        up0 = 0.0f;
        up1 = 0.0f;
        up2 = 0.0f;
        up3 = 0.0f;
      } else {
        up0 = a_up[i];
        up1 = a_up[i+1];
        up2 = a_up[i+2];
        up3 = a_up[i+3];
      }

      if (a_down == 0) {
        down0 = 0.0f;
        down1 = 0.0f;
        down2 = 0.0f;
        down3 = 0.0f;
      } else {
        down0 = a_down[i];
        down1 = a_down[i+1];
        down2 = a_down[i+2];
        down3 = a_down[i+3];
      }

      register float bot = a_self[self_idx-1];
      register float self0 = a_self[self_idx];
      register float self1 = a_self[self_idx+1];
      register float self2 = a_self[self_idx+2];
      register float self3 = a_self[self_idx+3];
      register float top = a_self[self_idx+4];

      register float next0 = left0 + right0 + up0 + down0 + bot   + self1;
      register float next1 = left1 + right1 + up1 + down1 + self0 + self2;
      register float next2 = left2 + right2 + up2 + down2 + self1 + self3;
      register float next3 = left3 + right3 + up3 + down3 + self2 + top;
      
      self0 = self0 * c0f;
      self1 = self1 * c0f;
      self2 = self2 * c0f;
      self3 = self3 * c0f;
    
      asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
        : [rd] "=f" (next0) \
        : [rs1] "f" (next0), [rs2] "f" (c1f), [rs3] "f" (self0) \
      );
      asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
        : [rd] "=f" (next1) \
        : [rs1] "f" (next1), [rs2] "f" (c1f), [rs3] "f" (self1) \
      );
      asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
        : [rd] "=f" (next2) \
        : [rs1] "f" (next2), [rs2] "f" (c1f), [rs3] "f" (self2) \
      );
      asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
        : [rd] "=f" (next3) \
        : [rs1] "f" (next3), [rs2] "f" (c1f), [rs3] "f" (self3) \
      );

      dram_next[ii+i] = next0; 
      dram_next[ii+i+1] = next1; 
      dram_next[ii+i+2] = next2; 
      dram_next[ii+i+3] = next3; 
    }
    bsg_fence();
    bsg_barrier_hw_tile_group_sync();

  }
}
