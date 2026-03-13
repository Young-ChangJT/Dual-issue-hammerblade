#include <bsg_manycore.h>
#include "bsg_cuda_lite_barrier.h"
#include <stdbool.h>
#include <stdint.h>
#include "bsg_barrier_multipod.h"

__attribute__ ((always_inline)) 
void copySelf(
  float *src,
  float *dst,
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
  bsg_unroll(1)
  for (int i = 0; i < (LOCAL_SIZE); i+=16) {
    float *temp_dst = &dst[1+i];
    float *temp_src = &src[start_idx+i];

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
    asm volatile("": : :"memory");
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
  float *a_left,
  float *a_right,
  float *a_up,
  float *a_down,
  int start_idx
)
{
  // left;
  if (((uintptr_t) a_left) & 0x80000000) {
    float *ptr = &a_left[start_idx];
    bsg_unroll(1)
    for (int i = 0; i < LOCAL_SIZE; i+=16) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (ptr[i]));
    }
  }
  asm volatile("": : :"memory");

  if (((uintptr_t) a_right) & 0x80000000) {
    float * ptr = &a_right[start_idx];
    bsg_unroll(1)
    for (int i = 0; i < LOCAL_SIZE; i+=16) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (ptr[i]));
    }
  }
  asm volatile("": : :"memory");

  if (((uintptr_t) a_up) & 0x80000000) {
    float * ptr = &a_up[start_idx];
    bsg_unroll(1)
    for (int i = 0; i < LOCAL_SIZE; i++) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (ptr[i]));
    }
  }
  asm volatile("": : :"memory");

  if (((uintptr_t) a_down) & 0x80000000) {
    float * ptr = &a_down[start_idx];
    bsg_unroll(1)
    for (int i = 0; i < LOCAL_SIZE; i++) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (ptr[i]));
    }
  }
  asm volatile("": : :"memory");
}





void compute (
  int c0, int c1,
  float *dram_self,
  float *dram_next,
  float *a_left,
  float *a_right,
  float *a_up,
  float *a_down,
  float *a_self,
  const int nz,
  int pod_id,
  volatile int* done,
  int* alert
) {

  float c0f = (float) c0;
  float c1f = (float) c1;
  bsg_unroll(1)
  for (int ii = 0; ii < nz; ii += LOCAL_SIZE) {

    copySelf(dram_self, a_self, ii, nz);
    prefetch_dram(a_left, a_right, a_up, a_down, ii);
    bsg_fence();
    bsg_barrier_multipod(pod_id, NUM_POD_X, done, alert);

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
    bsg_barrier_multipod(pod_id, NUM_POD_X, done, alert);
  }
}
