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

extern "C" void forces(Config *pcfg, HBOctree *proot, HBBody *HBBodies, int nBodies, unsigned int _diam, int *amocur, int body_end){
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
                //bsg_print_int(cur);
                float diamsq = diam * diam * cfg.itolsq;
                HBBody *pcurb = &HBBodies[cur];
                HBBody curb = *pcurb;
                Point prev_acc = curb.acc;
                curb.acc = Point(0.0f, 0.0f, 0.0f);
                HBNode *pother = proot; // It is not clear whether copying the other node locally will help.
                Point delta;
                float distsq;
                char octant = 0;
                while (pother != proot || (pother == proot && octant < HBOctree::octants)){
                        // Leaf status is encoded in the low-order bit of the address.
                        unsigned int leaf = reinterpret_cast<uintptr_t>(pother) & 1;
                        pother = reinterpret_cast<HBNode*>(reinterpret_cast<uintptr_t>(pother) & (~1));
                        HBOctree *node = static_cast<HBOctree *>(pother);

                        if(leaf){
                                // Leaf node, compute force
                                if(pother != static_cast<HBNode *>(pcurb)){
                                        delta  = (curb.pos - pother->pos);
                                        distsq = delta.dist2();
                                        curb.acc += updateForce(cfg, delta, distsq, pother->mass);
                                }
                                // Move upward
                                octant = pother->octant + 1;
                                pother = pother->pred;
                                diamsq *= 4.0f;
                        } else if (octant == 0) {
                                // Have not visited this node before
                                delta  = (curb.pos - pother->pos);
                                distsq = delta.dist2();
                                if(distsq >= diamsq){
                                        // Far away, compute summarized force
                                        curb.acc += updateForce(cfg, delta, distsq, pother->mass);
                                        // Move upward
                                        octant = pother->octant + 1;
                                        pother = pother->pred;
                                        diamsq *= 4.0f;
                                } else if(node->child[0] != nullptr){
                                        // Nearby, downward.
                                        pother = node->child[0];
                                        octant = 0;
                                        diamsq *= .25f;
                                } else {
                                        // No leaf/body, try next octant
                                        octant ++;
                                }
                        } else if (octant < HBOctree::octants) {
                                if(node->child[octant]) {
                                        // Downward
                                        pother = node->child[octant];
                                        octant = 0;
                                        diamsq *= .25f;
                                } else {
                                        // Sideways :p 
                                        octant ++;
                                }
                        } else {
                                // Upward
                                octant  = pother->octant + 1;
                                pother  = pother->pred;
                                diamsq *= 4.0f;
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
}
