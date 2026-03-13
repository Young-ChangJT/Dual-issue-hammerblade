#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>
#include <aes_kernel.hpp>


#define CACHE_LINE_IN_BYTES 64
__attribute__ ((noinline))
void warmup(struct AES_ctx *ctx, uint8_t* buf, size_t length, int niters) {
  // prefetch ctx
  for (int n = 0; n < niters; n++) {
    struct AES_ctx *curr_ctx = &ctx[(__bsg_id*niters) + n];
    asm volatile ("lw x0, %[p]" :: [p] "m" (curr_ctx->RoundKey[0]));
    asm volatile ("lw x0, %[p]" :: [p] "m" (curr_ctx->RoundKey[CACHE_LINE_IN_BYTES]));
    asm volatile ("lw x0, %[p]" :: [p] "m" (curr_ctx->RoundKey[CACHE_LINE_IN_BYTES*2]));
    asm volatile ("lw x0, %[p]" :: [p] "m" (curr_ctx->Iv));
  }

  // prefetch buf
  uint8_t *my_buf = &buf[(__bsg_id*niters*length)];
  for (int i = 0; i < length*niters; i += CACHE_LINE_IN_BYTES) {
    asm volatile ("lw x0, %[p]" :: [p] "m" (my_buf[i]));
  }

  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
}


extern "C" void aes_singlegrid(struct AES_ctx *ctx, uint8_t* buf, size_t length, int niters){
	      //int tg_idx = (bsg_tiles_X * __bsg_y + __bsg_x);
        bsg_barrier_hw_tile_group_init();
        bsg_barrier_hw_tile_group_sync();
        warmup(ctx, buf, length, niters);
 
        bsg_cuda_print_stat_kernel_start();

        for(int i = 0 ; i < niters; ++i){
                AES_CBC_encrypt_buffer(&ctx[__bsg_id * niters + i], &buf[__bsg_id * niters * length + length * i], length);
        }

        bsg_cuda_print_stat_kernel_end();
        bsg_fence();
        bsg_barrier_hw_tile_group_sync();
}
