#include <cmath>
#include <bsg_manycore_atomic.h>
#include <bsg_manycore.h>
#include <bsg_cuda_lite_barrier.h>

#include <Octree.hpp>
#include <Body.hpp>
#include <Config.hpp>

inline Point updateForce(Config &cfg, Point delta, float psq, float mass) {
        // Computing force += delta * mass * (|delta|^2 + eps^2)^{-3/2}
        float idr   = 1.0f / sqrtf((float)(psq + cfg.epssq));
        float scale = mass * idr * idr * idr;
        return delta * scale;
}

#define MAX_STACK_SIZE 256
HBOctree* node_stack[MAX_STACK_SIZE];
float diamsq_stack[MAX_STACK_SIZE];
HBOctree** max_node_stack_ptr = &node_stack[MAX_STACK_SIZE+1];

extern "C" void forces(Config *pcfg, HBOctree *proot, HBBody *HBBodies, int nBodies, unsigned int _diam, int *amocur, int body_end){
#ifdef KERNEL_FORCES
        // We can't pass float arguments (technical issue), just
        // pointers and integer arguments.
        // Copy frequently used data to local
        Config cfg = *pcfg;
        float diam = *reinterpret_cast<float *>(&_diam);
        bsg_barrier_hw_tile_group_init();
        bsg_barrier_hw_tile_group_sync();   
        bsg_cuda_print_stat_kernel_start();

        // Work imbalance is a pain. Use an amoadd queue to allocate work dynamically.
        int cur;
        cur = bsg_amoadd(amocur, 1);
        while(cur < body_end){
                float diamsq = diam * diam * cfg.itolsq;
                HBBody *pcurb = &HBBodies[cur];
                HBBody curb = *pcurb;
                Point prev_acc = curb.acc;
                curb.acc = Point(0.0f, 0.0f, 0.0f);

                // put the root in the stack;
                node_stack[0] = proot;
                HBOctree** node_stack_top = &node_stack[1];
                diamsq_stack[0] = diamsq;
                float * diamsq_stack_top = &diamsq_stack[1];
                

                //int octant = 0;
                Point delta;
                float distsq;
            
                while (node_stack_top != node_stack) {
                  // take one off the stack;
                  node_stack_top--;
                  HBOctree *curr_node = *node_stack_top;
                  diamsq_stack_top--;
                  float curr_diamsq = *diamsq_stack_top;
                
                  // check if it's far away;
                  delta  = (curb.pos - curr_node->pos);
                  distsq = delta.dist2();

                  if (distsq >= curr_diamsq) {
                    // Far away, compute summarized force
                    curb.acc += updateForce(cfg, delta, distsq, curr_node->mass);
                  } else {
                    float child_diamsq = curr_diamsq * 0.25f;
                    // Move down;
                    // Load all the child pointers;
                    HBNode* children[8];
                    HBNode* tmp0 = curr_node->child[0];
                    HBNode* tmp1 = curr_node->child[1];
                    HBNode* tmp2 = curr_node->child[2];
                    HBNode* tmp3 = curr_node->child[3];
                    HBNode* tmp4 = curr_node->child[4];
                    HBNode* tmp5 = curr_node->child[5];
                    HBNode* tmp6 = curr_node->child[6];
                    HBNode* tmp7 = curr_node->child[7];
                    asm volatile("": : :"memory");
                    children[0] = tmp0;
                    children[1] = tmp1;
                    children[2] = tmp2;
                    children[3] = tmp3;
                    children[4] = tmp4;
                    children[5] = tmp5;
                    children[6] = tmp6;
                    children[7] = tmp7;
                    asm volatile("": : :"memory");

                    for (int i = 0; i < 8; i++) {
                      if (children[i] == nullptr) {
                        // skip null point;
                        continue;
                      } else {
                        unsigned int leaf = reinterpret_cast<uintptr_t>(children[i]) & 1;
                        if (leaf) {
                          // child is leaf;
                          if (children[i] != static_cast<HBNode *>(pcurb)) {
                            // child is not self;
                            delta = (curb.pos - children[i]->pos);
                            distsq = delta.dist2();
                            curb.acc += updateForce(cfg, delta, distsq, children[i]->mass);
                          }
                        } else {
                          // child is internal node;
                          // put it on the stack;
                          *diamsq_stack_top = child_diamsq;
                          diamsq_stack_top++;
                          *node_stack_top = static_cast<HBOctree*>(children[i]);
                          node_stack_top++;
                          if (node_stack_top == max_node_stack_ptr) {
                            bsg_print_int(0xdeadbeef);
                          }
                        }
                      }
                    }
                  }
                }

                // Finished traversal
                curb.vel += (curb.acc - prev_acc) * cfg.dthf;
                HBBodies[cur].vel = curb.vel;
                HBBodies[cur].acc = curb.acc;
                cur = bsg_amoadd(amocur, 1);                
        }

        bsg_barrier_hw_tile_group_sync();
        bsg_cuda_print_stat_kernel_end();
        bsg_fence();
        bsg_barrier_hw_tile_group_sync();
        return;
#endif
}
