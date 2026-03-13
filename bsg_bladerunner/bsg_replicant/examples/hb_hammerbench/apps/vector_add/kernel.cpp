#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>

#ifdef WARM_CACHE
__attribute__((noinline))
static void warmup(float *A, float *B, float *C, int N)
{
  volatile float sink;
  for (int i = __bsg_id*CACHE_LINE_WORDS; i < N; i += bsg_tiles_X*bsg_tiles_Y*CACHE_LINE_WORDS) {
      sink = A[i];
      sink = B[i];
      sink = C[i];
  }
  bsg_fence();
}
#endif


// Vector-Add: C = A + B
// N = vector size
extern "C" __attribute__ ((noinline))
int
kernel_vector_add(float * A, float * B, float *C, int N) {

  bsg_barrier_tile_group_init();
#ifdef WARM_CACHE
  warmup(A, B, C, N);
  bsg_barrier_tile_group_sync();
#endif
  bsg_cuda_print_stat_kernel_start();

  // ==============================================================
  // 🚀 雙發射 (Dual-Issue) 絕對安全測試區
  // 1. 先用 fmv.w.x 將 x0(0) 複製給 FP 暫存器，保證浮點數是乾淨的 0.0
  // 2. Clobber List 保護 C++ 的變數不會被我們覆蓋
  // ==============================================================
  // __asm__ __volatile__ (
  //     "fmv.w.x f1, x0\n\t"
  //     "fmv.w.x f2, x0\n\t"
      
  //     "fadd.s f4, f1, f2\n\t"   // [FP]  0.0 + 0.0
  //     "addi   x5, x0, 1\n\t"    // [INT] 0 + 1
      
  //     "fmul.s f5, f1, f2\n\t"   // [FP]  0.0 * 0.0
  //     "addi   x6, x0, 2\n\t"    // [INT] 0 + 2
      
  //     "fsub.s f6, f1, f2\n\t"   // [FP]  0.0 - 0.0
  //     "addi   x7, x0, 3\n\t"    // [INT] 0 + 3
  //     : // 沒有 Output
  //     : // 沒有 Input
  //     : "f1", "f2", "f4", "f5", "f6", "x5", "x6", "x7" // ⚠️ 告訴編譯器：這些暫存器被我弄髒了，請避開！
  // );
  // ==============================================================

  // Each tile does a portion of vector_add
  int len = N / (bsg_tiles_X*bsg_tiles_Y);
  float *myA = &A[__bsg_id*len];
  float *myB = &B[__bsg_id*len];
  float *myC = &C[__bsg_id*len];

  int i = 0;
  for (; i + 7 < len; i += 8) {
      float a0,a1,a2,a3,a4,a5,a6,a7;
      float b0,b1,b2,b3,b4,b5,b6,b7;

      // loads
      a0 = myA[i+0];  b0 = myB[i+0];
      a1 = myA[i+1];  b1 = myB[i+1];
      a2 = myA[i+2];  b2 = myB[i+2];
      a3 = myA[i+3];  b3 = myB[i+3];
      a4 = myA[i+4];  b4 = myB[i+4];
      a5 = myA[i+5];  b5 = myB[i+5];
      a6 = myA[i+6];  b6 = myB[i+6];
      a7 = myA[i+7];  b7 = myB[i+7];

      // adds
      float c0 = a0 + b0;
      float c1 = a1 + b1;
      float c2 = a2 + b2;
      float c3 = a3 + b3;
      float c4 = a4 + b4;
      float c5 = a5 + b5;
      float c6 = a6 + b6;
      float c7 = a7 + b7;

      // stores
      myC[i+0] = c0;
      myC[i+1] = c1;
      myC[i+2] = c2;
      myC[i+3] = c3;
      myC[i+4] = c4;
      myC[i+5] = c5;
      myC[i+6] = c6;
      myC[i+7] = c7;
  }

  // tail
  for (; i < len; i++) {
      myC[i] = myA[i] + myB[i];
  }

  bsg_fence();
  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_tile_group_sync();

  return 0;
}