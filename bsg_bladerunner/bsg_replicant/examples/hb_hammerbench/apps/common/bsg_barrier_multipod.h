#ifndef BSG_BARRIER_MULTIPOD
#define BSG_BARRIER_MULTIPOD


#include "bsg_cuda_lite_barrier.h"
#include "bsg_manycore.h"

// currently for one pod row;
static inline void bsg_barrier_multipod(int pod_id, int num_pod_x, volatile int* done, int* alert)
{
  for (int px = 0; px < num_pod_x; px++) {
    done[px] = 0;
  }
  *alert=0;
  bsg_fence();
  bsg_barrier_tile_group_sync();

  // center tile;
  if ((__bsg_x == bsg_tiles_X/2) && (__bsg_y == bsg_tiles_Y/2)) {
    // send remote store to the done list;
    volatile int* remote_done_ptr =  bsg_global_ptr((BSG_MACHINE_GLOBAL_X+(bsg_tiles_X/2)),
                                                    (BSG_MACHINE_GLOBAL_Y+(bsg_tiles_Y/2)),
                                                    &done[pod_id]);
    *remote_done_ptr  = 1;
    bsg_fence();

    if (pod_id == 0) {
      // main pod
      // wait for every other pod to join;
      int done_count = 0;
      while (done_count < num_pod_x) {
        if (done[done_count] == 0) {
          continue;
        } else {
          done_count++;
        }
      }
  
      // one every pod joins, wake up everyone;
      for (int px = 0; px < num_pod_x; px++) {
        volatile int* remote_alert_ptr =  bsg_global_ptr((((1+px)*BSG_MACHINE_GLOBAL_X)+(bsg_tiles_X/2)),
                                                         (BSG_MACHINE_GLOBAL_Y+(bsg_tiles_Y/2)),
                                                         (alert));
        *remote_alert_ptr = 1;
      }
    } else {
      // other pods sleep;
      int tmp;
      while (1) {
        tmp = bsg_lr(alert);
        if (tmp) {
          break;
        } else {
          tmp = bsg_lr_aq(alert);
          if (tmp) {
            break;
          }
        }
      }
    }
  }

  bsg_fence();
  bsg_barrier_tile_group_sync();
}

#endif
