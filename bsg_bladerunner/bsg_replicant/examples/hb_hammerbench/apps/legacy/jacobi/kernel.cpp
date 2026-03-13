//This kernel adds 2 vectors

#define BSG_TILE_GROUP_X_DIM bsg_tiles_X
#define BSG_TILE_GROUP_Y_DIM bsg_tiles_Y
#include <math.h>
#include "bsg_manycore.h"
#include "bsg_set_tile_x_y.h"
#include "bsg_group_strider.hpp"
#include "bsg_cuda_lite_barrier.h"


#define Index3D(nx,ny,nz,x,y,z) (((((y)*nx)+(x))*nz)+(z))
#define bsg_tg_float_ptr(x,y,addr) \
  ((float *) ((1 << REMOTE_PREFIX_SHIFT) \
             | (y << REMOTE_Y_CORD_SHIFT) \
             | (x << REMOTE_X_CORD_SHIFT) \
             | ((int) addr)))



#ifdef WARM_CACHE
__attribute__ ((noinline))
static int warmup(float *A0, float *Anext, int nx, int ny, int nz)
{
  for (int i = __bsg_id*CACHE_LINE_WORDS; i < nx*ny*nz; i += bsg_tiles_X*bsg_tiles_Y*CACHE_LINE_WORDS) {
      asm volatile ("lw x0, %[p]" :: [p] "m" (A0[i]));
      asm volatile ("lw x0, %[p]" :: [p] "m" (Anext[i]));
  }

  bsg_fence();
  bsg_barrier_hw_tile_group_sync();
  return 0;
}
#endif


float a_self[LOCAL_SIZE+2]    = {0.0f};


extern "C" void compute(
  int c0, int c1,
  float * bsg_attr_noalias A0,
  float * bsg_attr_noalias Anext,
  float * bsg_attr_noalias a_left,
  float * bsg_attr_noalias a_right,
  float * bsg_attr_noalias a_up,
  float * bsg_attr_noalias a_down,
  float * bsg_attr_noalias a_self,
  const int nz
);


// Main Kernel Function
extern "C" __attribute__ ((noinline)) __attribute__((used))
int kernel_jacobi(
  int c0, int c1,
  float *A0, float * Anext,
  const int nx, const int ny, const int nz) {


  // Number of sub-blocks in XY dim.
  const int BX = nx / bsg_tiles_X;
  const int BY = ny / bsg_tiles_Y;
  // XY edge tiles.
  const bool x_l_bound = (__bsg_x == 0);
  const bool x_h_bound = (__bsg_x == (bsg_tiles_X-1));
  const bool y_l_bound = (__bsg_y == 0);
  const bool y_h_bound = (__bsg_y == (bsg_tiles_Y-1));

  // Init hw barrier.
  bsg_barrier_hw_tile_group_init();
#ifdef WARM_CACHE
  warmup(A0, Anext, nx, ny, nz);
#endif
  bsg_barrier_hw_tile_group_sync();

  // kernel start
  bsg_cuda_print_stat_kernel_start();


  // Remote pointers
  float* a_up, *a_down, *a_left, *a_right, *dram_self, *dram_next;

  bsg_unroll(1)
  for (int bx = 0; bx < nx; bx+=bsg_tiles_X) {
    bsg_unroll(1)
    for (int by = 0; by < ny; by+=bsg_tiles_Y) {

      // Figure out the remote pointers
      if (x_l_bound) {
        if (bx == 0) {
          a_left = 0;
        } else {
          a_left = &A0[Index3D(nx,ny,nz, bx-1, by+__bsg_y, 0)];
        }
      } else {
        a_left = bsg_tg_float_ptr(__bsg_x-1, __bsg_y, &a_self[1]);
      }

      if (x_h_bound) {
        if (bx == nx-bsg_tiles_X) {
          a_right = 0;
        } else {
          a_right = &A0[Index3D(nx,ny,nz, bx+bsg_tiles_X, by+__bsg_y, 0)];
        }
      } else {
        a_right = bsg_tg_float_ptr(__bsg_x+1, __bsg_y, &a_self[1]);
      }

      if (y_l_bound) {
        if (by == 0) {
          a_down = 0;
        } else {
          a_down = &A0[Index3D(nx,ny,nz, bx+__bsg_x, by-1, 0)];
        }
      } else {
        a_down = bsg_tg_float_ptr(__bsg_x, __bsg_y-1, &a_self[1]);
      }

      if (y_h_bound) {
        if (by == ny-bsg_tiles_Y) {
          a_up   = 0;
        } else {
          a_up   = &A0[Index3D(nx,ny,nz, bx+__bsg_x, by+bsg_tiles_Y, 0)];
        }
      } else {
        a_up   = bsg_tg_float_ptr(__bsg_x, __bsg_y+1, &a_self[1]);
      }

      dram_self = &A0[Index3D(nx,ny,nz, bx+__bsg_x, by+__bsg_y, 0)];
      dram_next = &Anext[Index3D(nx,ny,nz, bx+__bsg_x, by+__bsg_y, 0)];

      compute(c0, c1,
        dram_self, dram_next,
        a_left, a_right, a_up, a_down, a_self,
        nz);

    }
  }


  // Kernel End.
  bsg_cuda_print_stat_kernel_end(); // stat kernel end
  bsg_fence();
  bsg_barrier_hw_tile_group_sync();

	return 0;
}
