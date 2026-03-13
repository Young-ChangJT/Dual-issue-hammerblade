#include <bsg_manycore_tile.h>
#include <bsg_manycore_errno.h>
#include <bsg_manycore_tile.h>
#include <bsg_manycore_loader.h>
#include <bsg_manycore_cuda.h>
#include <bsg_manycore_regression.h>
#include <iostream>
#include <vector>
#include <random>
#include <math.h>
#include "Body.hpp"
#include "Node.hpp"
#include "HBBody.hpp"
#include "HBNode.hpp"

#define ALLOC_NAME "default_allocator"

// Constants;
#define itolsq  (1.0f/(0.5f*0.0f))
#define epssq   (0.05f*0.05f)
#define dthf    (0.25f)

// Generate Bodies;
void generate_bodies(std::vector<Body>& bodies, int nbodies) {
  std::mt19937 gen(0);
  std::uniform_real_distribution<float> dist(0, 1);

  float v, sq, scale;
  float rsc = (3 * M_PI) / 16;
  float vsc = sqrt(1.0 / rsc);
  float a = 1.0f;

  for (int i = 0; i < nbodies; i++) {
    Body body;
    float r = 1.0f / a / sqrt(pow(dist(gen) * 0.999f, -2.0f / 3.0f) -1);

    float p[3];
    // position
    do {
      p[0] = dist(gen) * 2.0 - 1.0;
      p[1] = dist(gen) * 2.0 - 1.0;
      p[2] = dist(gen) * 2.0 - 1.0;
      sq = (p[0] * p[0]) +
           (p[1] * p[1]) +
           (p[2] * p[2]);
    } while (sq > 1.0);
    scale = rsc * r / sqrt(sq);

    body.mass = 1.0 / nbodies;
    body.pos[0] = p[0] * scale;
    body.pos[1] = p[1] * scale;
    body.pos[2] = p[2] * scale;

    // Velocity
    do {
      p[0] = dist(gen);
      p[1] = dist(gen) * 0.1;
    } while (p[1] > p[0] * p[0] * pow(1 - p[0] * p[0], 3.5));

    v = p[0] * sqrt(2.0 / sqrt(a*a + r*r));

    do {
      p[0] = dist(gen) * 2.0 - 1.0;
      p[1] = dist(gen) * 2.0 - 1.0;
      p[2] = dist(gen) * 2.0 - 1.0;
      sq = (p[0] * p[0]) +
           (p[1] * p[1]) +
           (p[2] * p[2]);
    } while (sq > 1.0);
    scale = vsc * v / sqrt(sq);
    body.vel[0] = p[0] * scale;
    body.vel[1] = p[1] * scale;
    body.vel[2] = p[2] * scale;

    body.acc[0] = 0.0f;
    body.acc[1] = 0.0f;
    body.acc[2] = 0.0f;
    bodies.push_back(body);
  }
}


int find_octree_idx(float *pos, float *pcenter) {
  int idx = 0;
  idx += (pos[0] > pcenter[0]) ? 1 : 0;
  idx += (pos[1] > pcenter[1]) ? 2 : 0;
  idx += (pos[2] > pcenter[2]) ? 4 : 0;
  return idx;
}

void find_new_center(float *new_center, float *pcenter, float radius, int octree_idx) {
  float delta = radius/2;
  int idx0 = octree_idx & 1;
  int idx1 = octree_idx & 2;
  int idx2 = octree_idx & 4;
  new_center[0] = idx0 ? (pcenter[0] + delta) : (pcenter[0] - delta);
  new_center[1] = idx1 ? (pcenter[1] + delta) : (pcenter[1] - delta);
  new_center[2] = idx2 ? (pcenter[2] + delta) : (pcenter[2] - delta);
}

// Insert Body in to a tree (recursive);
void insert_into_tree(
  std::vector<Body>& bodies,
  int body_idx,   // inserting this
  std::vector<Node>& nodes,
  int curr_node_idx,
  float curr_radius,
  float* curr_center
)
{
  // find oct idx;
  int oct_idx = find_octree_idx(bodies[body_idx].pos, curr_center);
  // new center;
  float new_center[3] = {0,0,0};
  find_new_center(new_center, curr_center, curr_radius, oct_idx);
  /*
  printf("insert_into_tree: body_idx=%d, pos=(%f,%f,%f), center=(%f,%f,%f)\n",
    body_idx,
    bodies[body_idx].pos[0],
    bodies[body_idx].pos[1],
    bodies[body_idx].pos[2],
    curr_center[0],
    curr_center[1],
    curr_center[2]
  );
  */
  
  if (nodes[curr_node_idx].is_leaf[oct_idx]==1) {
    /*
    printf("insert_into_tree: split. body_idx=%d, curr_node_idx=%d, oct_idx=%d, rad=%f\n",
      body_idx,
      curr_node_idx,
      oct_idx,
      curr_radius);
    */
    // split;
    int leaf_body_idx = nodes[curr_node_idx].child[oct_idx];
    // no longer leaf;
    nodes[curr_node_idx].is_leaf[oct_idx] = 0;
    // new node;
    Node new_node;
    for (int i = 0; i < 8; i++) {
      new_node.is_leaf[i] = 0;
      new_node.child[i] = -1;
    }

    // insert new node;
    int new_node_idx = nodes.size();
    nodes.push_back(new_node);
    nodes[curr_node_idx].child[oct_idx] = new_node_idx;

    // insert bodies;
    insert_into_tree(bodies, leaf_body_idx, nodes, new_node_idx, curr_radius/2, new_center);
    insert_into_tree(bodies, body_idx, nodes, new_node_idx, curr_radius/2, new_center);
  } else {
    if (nodes[curr_node_idx].child[oct_idx] == -1) {
      // insert body here;
      /*
      printf("insert_into_tree: insert body here. body_idx=%d, curr_node_idx=%d, oct_idx=%d\n",
        body_idx, curr_node_idx, oct_idx);
      */
      nodes[curr_node_idx].is_leaf[oct_idx] = 1;
      nodes[curr_node_idx].child[oct_idx] = body_idx;
    } else {
      // traverse down;
      /*
      printf("insert_into_tree: traverse down. body_idx=%d, curr_node_idx=%d, oct_idx=%d\n",
        body_idx, curr_node_idx, oct_idx);
      */
      int next_node_idx = nodes[curr_node_idx].child[oct_idx];
      insert_into_tree(bodies, body_idx, nodes, next_node_idx, curr_radius/2, new_center);
    }
  }
}


// report tree;
void report_tree(std::vector<Node> nodes) {
  for (size_t id = 0; id < nodes.size(); id++) {
    Node curr_node = nodes[id];
    int num_leaf = 0;
    int num_node = 0;
    int num_empty = 0;
    for (int i = 0; i < 8; i++) {
      if (curr_node.is_leaf[i]) {
        num_leaf++;
      } else {
        if (curr_node.child[i] == -1) {
          num_empty++;
        } else {
          num_node++;
        }
      }
    }
    printf("node_id=%d, leaf=%d, empty=%d, node=%d\n", (int) id, num_leaf, num_empty, num_node);
  }
}


// Calculate CoM of a node; recursive helper;
void center_of_mass_helper(std::vector<Node>& nodes, std::vector<Body>& bodies, int node_idx,
  float* total_mass,
  float* wpos
)
{
  Node curr_node = nodes[node_idx];

  for (int i = 0; i < 8; i++) {
    if (curr_node.is_leaf[i]) {
      // leaf body;
      int body_idx = curr_node.child[i];
      float mass0 = bodies[body_idx].mass;
      *total_mass += mass0;
      wpos[0] += mass0 * bodies[body_idx].pos[0];
      wpos[1] += mass0 * bodies[body_idx].pos[1];
      wpos[2] += mass0 * bodies[body_idx].pos[2];
    } else {
      if (curr_node.child[i] != -1) {
        // next node;
        center_of_mass_helper(nodes, bodies, curr_node.child[i], total_mass, wpos);
      }
    }
  } 
}


// Calculate CoM of a node;
void center_of_mass(std::vector<Node>& nodes, std::vector<Body>& bodies, int node_idx) {
  float total_mass = 0.0f;
  float wpos[3]; // weighted pos;
  wpos[0] = 0.0f;
  wpos[1] = 0.0f;
  wpos[2] = 0.0f;

  center_of_mass_helper(nodes, bodies, node_idx, &total_mass, wpos);

  nodes[node_idx].co_mass = total_mass;
  nodes[node_idx].co_pos[0] = wpos[0] / total_mass;
  nodes[node_idx].co_pos[1] = wpos[1] / total_mass;
  nodes[node_idx].co_pos[2] = wpos[2] / total_mass;
}


// Convert to HB;
void convert_hb(std::vector<Node>& nodes, std::vector<Body>& bodies,
                HBNode* hbnodes, HBBody* hbbodies,
                hb_mc_eva_t d_hbnodes, hb_mc_eva_t d_hbbodies
)
{
  // convert bodies;
  for (size_t i = 0; i < bodies.size(); i++) {
    Body curr_body = bodies[i];
    hbbodies[i].mass   = curr_body.mass;
    hbbodies[i].pos[0] = curr_body.pos[0];
    hbbodies[i].pos[1] = curr_body.pos[1];
    hbbodies[i].pos[2] = curr_body.pos[2];
    hbbodies[i].vel[0] = curr_body.vel[0];
    hbbodies[i].vel[1] = curr_body.vel[1];
    hbbodies[i].vel[2] = curr_body.vel[2];
    hbbodies[i].acc[0] = curr_body.acc[0];
    hbbodies[i].acc[1] = curr_body.acc[1];
    hbbodies[i].acc[2] = curr_body.acc[2];
  }

  // convert nodes;
  for (size_t i = 0; i < nodes.size(); i++) {
    Node curr_node = nodes[i];
    hbnodes[i].co_mass = curr_node.co_mass;
    hbnodes[i].co_pos[0] = curr_node.co_pos[0];
    hbnodes[i].co_pos[1] = curr_node.co_pos[1];
    hbnodes[i].co_pos[2] = curr_node.co_pos[2];
    for (int j = 0; j < 8; j++) {
      if (curr_node.is_leaf[j]) {
        // LSB set to 1 to indicate that it's leaf;
        hbnodes[i].child[j] = (uint32_t) (((curr_node.child[j]*sizeof(HBBody)) + d_hbbodies) | 1);
      } else {
        if (curr_node.child[j] == -1) {
          hbnodes[i].child[j] = 0;
        } else {
          hbnodes[i].child[j] = (uint32_t) (d_hbnodes + (curr_node.child[j]*sizeof(HBNode)));
        }
      }
    }
  }
}


// set diamsq - recursive;
void set_diamsq(float diamsq, int node_id, std::vector<Node>& nodes, HBNode* hbnodes) {
  // set diamsq;
  hbnodes[node_id].diamsq = diamsq;
  nodes[node_id].diamsq = diamsq;

  // traverse down;
  for (int i = 0; i < 8; i++) {
    if (nodes[node_id].is_leaf[i]) {
      continue;
    } else {
      if (nodes[node_id].child[i] != -1) {
        set_diamsq(diamsq*0.25f, nodes[node_id].child[i], nodes, hbnodes);
      }
    }
  }
}


// Dist 2
float dist2(float x, float y, float z) {
  return (x*x) + (y*y) + (z*z);
}

// Force;
void updateForce(float* force, float* delta, float distsq, float mass) {
  float idr = 1.0f / sqrtf(distsq + epssq);
  float scale = mass * idr * idr * idr;
  force[0] = delta[0] * scale;
  force[1] = delta[1] * scale;
  force[2] = delta[2] * scale;
}

// calculate force;
void calculate_force(std::vector<Node>& nodes, std::vector<Body>& bodies)
{
  size_t max_stack_size = 0;
  for (size_t b = 0; b < bodies.size(); b++) {
    std::vector<int> stack;
    Body curr_body = bodies[b];
    
    float prev_acc[3];
    prev_acc[0] = curr_body.acc[0];
    prev_acc[1] = curr_body.acc[1];
    prev_acc[2] = curr_body.acc[2];
    curr_body.acc[0] = 0.0f;
    curr_body.acc[1] = 0.0f;
    curr_body.acc[2] = 0.0f;

    // put the root in the stack
    stack.push_back(0);

    while (!stack.empty()) {
      int curr_node_id = stack.back();
      //printf("curr_node_id: %d\n", curr_node_id);
      stack.pop_back();
      Node curr_node = nodes[curr_node_id]; 

      // distsq
      float delta[3];
      delta[0] = curr_body.pos[0] - curr_node.co_pos[0];
      delta[1] = curr_body.pos[1] - curr_node.co_pos[1];
      delta[2] = curr_body.pos[2] - curr_node.co_pos[2];
      float curr_diamsq = itolsq * curr_node.diamsq;
      float distsq = dist2(delta[0], delta[1], delta[2]);   
      //printf("curr_node.co_pos[0]=%f\n", curr_node.co_pos[0]); 
      //printf("curr_node.co_pos[1]=%f\n", curr_node.co_pos[1]); 
      //printf("curr_node.co_pos[2]=%f\n", curr_node.co_pos[2]); 
      //printf("curr_node.diamsq=%f\n", curr_node.diamsq); 
      //printf("distsq=%f, curr_diamsq=%f\n", distsq, curr_diamsq); 
      if (distsq >= curr_diamsq) {
        // far away;
        //printf("far away\n");
        float node_force[3];
        updateForce(node_force, delta, distsq, curr_node.co_mass);
        curr_body.acc[0] += node_force[0];
        curr_body.acc[1] += node_force[1];
        curr_body.acc[2] += node_force[2];
      } else {
        // Traverse down;
        for (int i = 0; i < 8; i++) {
          if (curr_node.is_leaf[i]) {
            // child is a leaf;
            if (b != curr_node.child[i]) {
              // leaf is not self;
              Body child = bodies[curr_node.child[i]];
              float child_delta[3];
              child_delta[0] = curr_body.pos[0] - child.pos[0];
              child_delta[1] = curr_body.pos[1] - child.pos[1];
              child_delta[2] = curr_body.pos[2] - child.pos[2];
              float child_distsq = dist2(child_delta[0], child_delta[1], child_delta[2]);
              float child_force[3];
              updateForce(child_force, child_delta, child_distsq, child.mass);
              curr_body.acc[0] += child_force[0];
              curr_body.acc[1] += child_force[1];
              curr_body.acc[2] += child_force[2];
            }
          } else {
            if (curr_node.child[i] == -1) {
              // skip
              continue;
            } else {
              // put it on stack;
              stack.push_back(curr_node.child[i]);
            }
          } 
        }
        if (stack.size() > max_stack_size) {
          max_stack_size = stack.size();
        }
      }
    }
    
    // Finish
    float new_vel[3];
    new_vel[0] = dthf * (curr_body.acc[0] - prev_acc[0]);
    new_vel[1] = dthf * (curr_body.acc[1] - prev_acc[1]);
    new_vel[2] = dthf * (curr_body.acc[2] - prev_acc[2]);
    curr_body.vel[0] += new_vel[0];
    curr_body.vel[1] += new_vel[1];
    curr_body.vel[2] += new_vel[2];
    bodies[b].vel[0] = curr_body.vel[0];
    bodies[b].vel[1] = curr_body.vel[1];
    bodies[b].vel[2] = curr_body.vel[2];
    bodies[b].acc[0] = curr_body.acc[0];
    bodies[b].acc[1] = curr_body.acc[1];
    bodies[b].acc[2] = curr_body.acc[2];
  }

  printf("max_stack_size = %d\n", max_stack_size);
}



// Main;
int barneshut_multipod(int argc, char ** argv) {
  int r = 0;

  // Command line args;
  const char * bin_path = argv[1];
 
  // Parameters; 
  int nbodies = NBODIES;
  int numpods = NUMPODS;
  int pod_id = PODID;
  printf("nbodies=%d\n", nbodies);
  printf("numpods=%d\n", numpods);
  printf("pod_id=%d\n", pod_id);


  // Generate Input bodies;
  std::vector<Body> bodies;
  generate_bodies(bodies, nbodies);

  // Print bodies;
  /*
  for (int i = 0; i < bodies.size(); i++) {
    printf("p=(%f,%f,%f) v=(%f,%f,%f)\n",
      bodies[i].pos0, bodies[i].pos1, bodies[i].pos2,
      bodies[i].vel0, bodies[i].vel1, bodies[i].vel2);
  }
  */

  // Find center coordinate;
  float pmax[3] = {-1000000,-1000000,-1000000};
  float pmin[3] = {1000000,1000000,1000000};
  for (size_t i = 0; i < bodies.size(); i++) {
    for (int d = 0; d < 3; d++) {
      pmax[d] = std::max(pmax[d], bodies[i].pos[d]);
      pmin[d] = std::min(pmin[d], bodies[i].pos[d]);
    }
  }
  
  float pcenter[3];
  for (int d = 0; d < 3; d++) {
    pcenter[d] = (pmax[d] + pmin[d]) / 2;
  }
  printf("center = (%f, %f, %f)\n", pcenter[0], pcenter[1], pcenter[2]);
  
  // radius;
  float radius = std::max(std::max(pmax[0]-pmin[0], pmax[1]-pmin[1]),pmax[2]-pmin[2]) / 2.0f;
  printf("radius = %f\n", radius);
  float diameter = radius*2.0f;
  float diamsq = diameter*diameter;

  // Build Tree;
  std::vector<Node> nodes;
  Node root;
  for (int i = 0; i < 8; i++) {
    root.child[i] = -1;
    root.is_leaf[i] = 0;
  }
  nodes.push_back(root);
  for (size_t i = 0; i < bodies.size(); i++) {
    insert_into_tree(bodies, i, nodes, 0, radius, pcenter);
  }

  printf("nodes.size() = %d\n", nodes.size());

  // Report tree;
  //report_tree(nodes);

  // Calculate center-of-mass for nodes;
  for (size_t i = 0; i < nodes.size(); i++) {
    center_of_mass(nodes, bodies, i);
    //printf("node_id=%d, mass=(%f) pos=(%f,%f,%f)\n", i, nodes[i].co_mass, nodes[i].co_pos[0], nodes[i].co_pos[1], nodes[i].co_pos[2]);
  }


  // Initialize Device;
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, "barneshut_multipod", HB_MC_DEVICE_ID));


  // Pod
  hb_mc_pod_id_t pod;

  hb_mc_eva_t d_hbnodes;
  hb_mc_eva_t d_hbbodies;
  hb_mc_eva_t d_nodestack;
  hb_mc_eva_t d_curr_body;
  hb_mc_eva_t d_remote_body;

  hb_mc_device_foreach_pod_id(&device, pod)
  {
    // Loading program;
    printf("Loading program for pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

    // body per pod;
    int curr_pod_id = pod_id + pod;
    int body_per_pod = (nbodies+NUMPODS-1)/NUMPODS;
    int body_start = std::min(nbodies, curr_pod_id*body_per_pod);
    int body_end = std::min(nbodies, body_start+body_per_pod);

    // Allocate memory on device;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, nodes.size()*sizeof(HBNode), &d_hbnodes));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, bodies.size()*sizeof(HBBody), &d_hbbodies));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, STACK_SIZE*sizeof(int)*bsg_tiles_X*bsg_tiles_Y, &d_nodestack));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sizeof(int), &d_curr_body));
    // For estimating interpod communications
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sizeof(HBBody)*(NUMPODS-1)*bodies.size(), &d_remote_body));
    

    // Convert to HBBody, HBNode;
    HBNode* hbnodes  = (HBNode *) malloc(nodes.size()*sizeof(HBNode));
    HBBody* hbbodies = (HBBody *) malloc(bodies.size()*sizeof(HBBody));
    convert_hb(nodes, bodies, hbnodes, hbbodies, d_hbnodes, d_hbbodies);


    // Set diamsq;
    set_diamsq(diamsq, 0, nodes, hbnodes);

    // DMA transfer;
    std::vector<hb_mc_dma_htod_t> htod_job;
    htod_job.push_back({d_hbnodes, hbnodes, nodes.size()*sizeof(HBNode)});
    htod_job.push_back({d_hbbodies, hbbodies, bodies.size()*sizeof(HBBody)});
    htod_job.push_back({d_curr_body, &body_start, sizeof(int)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_device(&device, htod_job.data(), htod_job.size()));


    // CUDA args
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1}; 
    #define CUDA_ARGC 7
    uint32_t cuda_argv[CUDA_ARGC] = {
      d_hbnodes,
      d_hbbodies,
      d_curr_body,
      body_end,
      d_nodestack,
      d_remote_body,
      (uint32_t) pod
    };

    // Enqueue kernel;
    printf("Enqueing kernel: pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_kernel_enqueue(&device, grid_dim, tg_dim, "kernel", CUDA_ARGC, cuda_argv));
  }

  // calculate force
  calculate_force(nodes, bodies);

  // Launch pods;
  printf("Launching all pods\n");
  BSG_CUDA_CALL(hb_mc_device_pods_kernels_execute(&device));




  // copy bodies from device;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    HBBody* next_hbbodies = (HBBody *) malloc(bodies.size()*sizeof(HBBody));
    std::vector<hb_mc_dma_dtoh_t> dtoh_job;
    dtoh_job.push_back({d_hbbodies, next_hbbodies, bodies.size()*sizeof(HBBody)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_host(&device, dtoh_job.data(), dtoh_job.size()));

    // Validate; 
    int curr_pod_id = pod_id + pod;
    int body_per_pod = (nbodies+NUMPODS-1)/NUMPODS;
    int body_start = std::min(nbodies, curr_pod_id*body_per_pod);
    int body_end = std::min(nbodies, body_start+body_per_pod);

    float serror;
    for (int b = body_start; b < body_end; b++) {
      printf("b=%d, HB acc=(%f %f %f), x86 acc=(%f %f %f)\n",
        b,
        next_hbbodies[b].acc[0], 
        next_hbbodies[b].acc[1], 
        next_hbbodies[b].acc[2], 
        bodies[b].acc[0],
        bodies[b].acc[1],
        bodies[b].acc[2]
      );
      float acc_diff0 = (next_hbbodies[b].acc[0] - bodies[b].acc[0]);
      float acc_diff1 = (next_hbbodies[b].acc[1] - bodies[b].acc[1]);
      float acc_diff2 = (next_hbbodies[b].acc[2] - bodies[b].acc[2]);
      serror += acc_diff0 * acc_diff0;
      serror += acc_diff1 * acc_diff1;
      serror += acc_diff2 * acc_diff2;

      printf("b=%d, HB vel=(%f %f %f), x86 vel=(%f %f %f)\n",
        b,
        next_hbbodies[b].vel[0], 
        next_hbbodies[b].vel[1], 
        next_hbbodies[b].vel[2], 
        bodies[b].vel[0],
        bodies[b].vel[1],
        bodies[b].vel[2]
      );
      float vel_diff0 = (next_hbbodies[b].vel[0] - bodies[b].vel[0]);
      float vel_diff1 = (next_hbbodies[b].vel[1] - bodies[b].vel[1]);
      float vel_diff2 = (next_hbbodies[b].vel[2] - bodies[b].vel[2]);
      serror += vel_diff0 * vel_diff0;
      serror += vel_diff1 * vel_diff1;
      serror += vel_diff2 * vel_diff2;
    }
    
    printf("serror = %f\n", serror);
    if (serror > 0.01f) {
      return HB_MC_FAIL;
    }
  }
  
  
  




  return HB_MC_SUCCESS;
}

declare_program_main("barneshut_multipod", barneshut_multipod);
