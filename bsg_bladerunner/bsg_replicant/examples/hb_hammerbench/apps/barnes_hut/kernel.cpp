#include <cmath>
#include <bsg_manycore.h>
#include <bsg_manycore_atomic.h>
#include <bsg_cuda_lite_barrier.h>
#include "bsg_barrier_multipod.h"
#include "HBNode.hpp"
#include "HBBody.hpp"

// Constants;
#define itolsq  (1.0f/(0.5f*0.5f))
#define epssq   (0.05f*0.05f)
#define dthf    (0.25f)

inline void updateForce(float* force, float* delta, float distsq, float mass) {
  float idr =  1.0f / sqrtf(distsq + epssq);
  float scale = mass * idr * idr * idr;
  force[0] = delta[0] * scale;
  force[1] = delta[1] * scale;
  force[2] = delta[2] * scale;
}

inline float dist2(float x, float y, float z) {
  return (x*x) + (y*y) + (z*z);
}

// multipod barrier;
volatile int done[NUM_POD_X] = {0};
int alert = 0;

int l_body_start;

// Kernel main;
extern "C" int kernel(HBNode* hbnodes, HBBody* hbbodies,
                      int* body_start, int body_end,
                      HBNode** nodestack,
                      HBBody* remote_body,
                      int pod_id
)
{
  bsg_barrier_tile_group_init();
  bsg_barrier_tile_group_sync();   
  l_body_start = *body_start;
  bsg_fence(); 
  bsg_barrier_multipod(pod_id, NUM_POD_X, done, &alert);
  bsg_cuda_print_stat_kernel_start();

  int curr;
  curr = bsg_amoadd(body_start,1);

  // my stack in DRAM;
  HBNode** mystack = &nodestack[__bsg_id*STACK_SIZE];
  HBNode** max_mystack_ptr = &mystack[STACK_SIZE+1];

  // delta;
  float delta[3];
  float distsq;

  while (curr < body_end) {
    HBBody *pcurr_body = &hbbodies[curr];
    HBBody curr_body = *pcurr_body;

    float prev_acc[3];
    prev_acc[0] = curr_body.acc[0];
    prev_acc[1] = curr_body.acc[1];
    prev_acc[2] = curr_body.acc[2];
    curr_body.acc[0] = 0.0f;
    curr_body.acc[1] = 0.0f;
    curr_body.acc[2] = 0.0f;

    // put the root in the stack
    mystack[0] = &hbnodes[0];
    HBNode** mystack_top = &mystack[1];
  
    while (mystack_top != mystack) {
      // take one off the stack;
      mystack_top--;
      HBNode* curr_node = *mystack_top;
      
      // distsq;
      float l_co_mass;
      float l_co_pos[3];
      float l_diamsq;
      l_co_mass = curr_node->co_mass;
      l_co_pos[0] = curr_node->co_pos[0];
      l_co_pos[1] = curr_node->co_pos[1];
      l_co_pos[2] = curr_node->co_pos[2];
      l_diamsq = curr_node->diamsq;
      asm volatile("": : :"memory");

      delta[0] = curr_body.pos[0] - l_co_pos[0];
      delta[1] = curr_body.pos[1] - l_co_pos[1];
      delta[2] = curr_body.pos[2] - l_co_pos[2];
      float curr_diamsq = itolsq * l_diamsq;
      distsq = dist2(delta[0], delta[1], delta[2]);

      if (distsq >= curr_diamsq) {
        // far away; compute summarized force;
        float node_force[3];
        updateForce(node_force, delta, distsq, l_co_mass);
        curr_body.acc[0] += node_force[0];
        curr_body.acc[1] += node_force[1];
        curr_body.acc[2] += node_force[2];
      } else {
        //float child_diamsq = curr_diamsq * 0.25f;
        // Move down;
        // Load all child pointers;
        uint32_t children[8];
        uint32_t tmp0 = curr_node->child[0];
        uint32_t tmp1 = curr_node->child[1];
        uint32_t tmp2 = curr_node->child[2];
        uint32_t tmp3 = curr_node->child[3];
        uint32_t tmp4 = curr_node->child[4];
        uint32_t tmp5 = curr_node->child[5];
        uint32_t tmp6 = curr_node->child[6];
        uint32_t tmp7 = curr_node->child[7];
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
          if (children[i] == 0) {
            // skip null pointer;
            continue;
          } else {
            uint32_t is_leaf = children[i] & 1;
            if (is_leaf) {
              // child is leaf;
              HBBody* body_ptr = (HBBody*) children[i];
              if (body_ptr != pcurr_body) {
                // child is not self;
                float child_pos[3];
                float child_mass;
                float child_delta[3];
                float child_distsq;
                child_pos[0] = body_ptr->pos[0];
                child_pos[1] = body_ptr->pos[1];
                child_pos[2] = body_ptr->pos[2];
                child_mass = body_ptr->mass;
                asm volatile("": : :"memory");
                child_delta[0] = curr_body.pos[0] - child_pos[0];
                child_delta[1] = curr_body.pos[1] - child_pos[1];
                child_delta[2] = curr_body.pos[2] - child_pos[2];
                child_distsq = dist2(child_delta[0], child_delta[1], child_delta[2]);
                float child_force[3];
                updateForce(child_force, child_delta, child_distsq, child_mass);
                curr_body.acc[0] += child_force[0];
                curr_body.acc[1] += child_force[1];
                curr_body.acc[2] += child_force[2];
              }
            } else {
              // child is an internal node;
              // put it on the stack;
              *mystack_top = (HBNode *) children[i];
              mystack_top++;
              if (mystack_top == max_mystack_ptr) {
                bsg_print_int(0xdeadbeef);
              }
            }
          }
        }
      }  
    }

    // Finished traversal;
    float new_vel[3]; 
    new_vel[0] = dthf * (curr_body.acc[0] - prev_acc[0]);
    new_vel[1] = dthf * (curr_body.acc[1] - prev_acc[1]);
    new_vel[2] = dthf * (curr_body.acc[2] - prev_acc[2]);
    curr_body.vel[0] += new_vel[0];
    curr_body.vel[1] += new_vel[1];
    curr_body.vel[2] += new_vel[2];
    hbbodies[curr].vel[0] = curr_body.vel[0];
    hbbodies[curr].vel[1] = curr_body.vel[1];
    hbbodies[curr].vel[2] = curr_body.vel[2];
    hbbodies[curr].acc[0] = curr_body.acc[0];
    hbbodies[curr].acc[1] = curr_body.acc[1];
    hbbodies[curr].acc[2] = curr_body.acc[2];
    curr = bsg_amoadd(body_start,1);
  }
  
  bsg_fence();
  
  // estimating interpod communication;
  /*
  for (int i = l_body_start+__bsg_id; i < body_end; i+=bsg_tiles_X*bsg_tiles_Y) {
    float l_vel[3];
    float l_acc[3];
    l_vel[0] = hbbodies[i].vel[0];
    l_vel[1] = hbbodies[i].vel[1];
    l_vel[2] = hbbodies[i].vel[2];
    l_acc[0] = hbbodies[i].acc[0];
    l_acc[1] = hbbodies[i].acc[1];
    l_acc[2] = hbbodies[i].acc[2];
    asm volatile("": : :"memory");
    for (int n = 0; n < NUMPODS-1; n++) {
      remote_body[(NBODIES*n)+i].vel[0] = l_vel[0];
      remote_body[(NBODIES*n)+i].vel[1] = l_vel[1];
      remote_body[(NBODIES*n)+i].vel[2] = l_vel[2];
      remote_body[(NBODIES*n)+i].acc[0] = l_acc[0];
      remote_body[(NBODIES*n)+i].acc[1] = l_acc[1];
      remote_body[(NBODIES*n)+i].acc[2] = l_acc[2];
    }
    asm volatile("": : :"memory");
  }
  bsg_fence();
  */
  bsg_barrier_tile_group_sync();
  bsg_cuda_print_stat_kernel_end();
  bsg_fence();
  bsg_barrier_tile_group_sync();
  return 0;
}
