#include <bsg_manycore_tile.h>
#include <bsg_manycore_errno.h>
#include <bsg_manycore_tile.h>
#include <bsg_manycore_loader.h>
#include <bsg_manycore_cuda.h>
#include <bsg_manycore_regression.h>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <set>
#include <fstream>
#include <math.h>

#define ALLOC_NAME "default_allocator"

float damp = 0.85f;
float beta_score = (1.0f - damp) / (float) EDGE;


void read_graph(int* ptr, std::string filename) {
  std::ifstream fh(filename);
  int idx = 0;
  int num;
  while (fh >> num) {
    ptr[idx] = num;
    idx++;
  }
}

void host_pagerank( int* in_indices,
                    int* in_neighbors,
                    float* out_degree_inv,
                    float* contrib,
                    float* contrib_new,
                    float* new_rank,
                    int start_id,
                    int end_id)
{
  for (int i = start_id; i < end_id; i++) {
    int nz_start = in_indices[i];
    int nz_end = in_indices[i+1];
    float temp_new = 0.0f;
    for (int nz = nz_start; nz < nz_end; nz++) {
      int idx = in_neighbors[nz];
      float c = contrib[idx];
      temp_new += c;
    }

    temp_new = (temp_new * damp) + beta_score;
    contrib_new[i] = temp_new * out_degree_inv[i];
    new_rank[i] = temp_new;
  }
}

int pagerank_multipod(int argc, char ** argv)
{
  int r = 0;

  // Command line arguments;
  const char * bin_path = argv[1];
  std::string fwd_offsets_file  = argv[2];
  std::string fwd_nonzeros_file = argv[3];
  std::string rev_offsets_file  = argv[4];
  std::string rev_nonzeros_file = argv[5];
  std::cout << "fwd_offsets_file = "  << fwd_offsets_file << std::endl;
  std::cout << "fwd_nonzeros_file = " << fwd_nonzeros_file << std::endl;
  std::cout << "rev_offsets_file = "  << rev_offsets_file << std::endl;
  std::cout << "rev_nonzeros_file = " << rev_nonzeros_file << std::endl;

  // Define parameters;
  int pod_id = PODID;
  int numpods = NUMPODS;
  int V = VERTEX;
  int E = EDGE;
  printf("pod_id = %d\n", pod_id);
  printf("numpods = %d\n", numpods);
  printf("V=%d\n", V);
  printf("E=%d\n", E);


  // Read input graph
  int * fwd_offsets  = (int *) malloc(sizeof(int)*(V+1));
  int * fwd_nonzeros = (int *) malloc(sizeof(int)*(E));
  int * rev_offsets  = (int *) malloc(sizeof(int)*(V+1));
  int * rev_nonzeros = (int *) malloc(sizeof(int)*(E));
  read_graph(fwd_offsets,  fwd_offsets_file);
  read_graph(fwd_nonzeros, fwd_nonzeros_file);
  read_graph(rev_offsets,  rev_offsets_file);
  read_graph(rev_nonzeros, rev_nonzeros_file);


  // Out degree;
  float *out_degree_inv = (float *) malloc(sizeof(float)*V);
  for (int i = 0; i < V; i++) {
    int od = fwd_offsets[i+1] - fwd_offsets[i];
    if (od == 0) {
      out_degree_inv[i] = 1.0f;
    } else {
      out_degree_inv[i] = 1.0f / (float) od;
    }
    //printf("out_degree_inv[%d]=%f\n", i, out_degree_inv[i]);
  }


  // Contrib;  
  float * contrib     = (float *) malloc(sizeof(float)*V);
  float * contrib_new = (float *) malloc(sizeof(float)*V);
  float * new_rank    = (float *) malloc(sizeof(float)*V);
  for (int i = 0; i < V; i++) {
    contrib[i] = 1.0f / (float) V;
  }

  // Partition vertices based on # of edges + vertices (hybrid);
  int V2E_per_pod = ((2*V)+E+numpods-1)/numpods;
  int v2e_count[numpods];
  int V_range[numpods+1];
  V_range[0] = 0;
  printf("V2E_per_pod=%d\n", V2E_per_pod);

  int curr_v2e = (rev_offsets[1] - rev_offsets[0]) + 2;
  int curr_pod = 0;
  for (int v = 1; v < V; v++) {
    if (curr_v2e > V2E_per_pod) {
      V_range[curr_pod+1] = v;
      v2e_count[curr_pod] = curr_v2e;
      curr_v2e = 0;
      curr_pod++;
    }   
    curr_v2e += (rev_offsets[v+1] - rev_offsets[v]) + 2;
  }
  
  V_range[curr_pod+1] = V;
  v2e_count[curr_pod] = curr_v2e;

  for (int p = curr_pod+1; p < numpods; p++) {
    V_range[p+1]=V;
    v2e_count[p] = 0;
  }

  for (int p = 0; p < numpods; p++) {
    printf("pod=%d, V_range=(%d, %d), V_count=%d, count=%d\n",
      p,
      V_range[p], V_range[p+1],
      V_range[p+1]-V_range[p],
      v2e_count[p]);
  }

  //return 0;


  // Initialize device;
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, "pagerank_multipod", HB_MC_DEVICE_ID));

  // Pod;
  hb_mc_pod_id_t pod;
  hb_mc_eva_t d_in_indices;
  hb_mc_eva_t d_in_neighbors;
  hb_mc_eva_t d_out_degree_inv;
  hb_mc_eva_t d_new_rank;
  hb_mc_eva_t d_contrib;
  hb_mc_eva_t d_contrib_new;
  hb_mc_eva_t d_start_id;
  
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    // Loading program;
    printf("Loading program for pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

    // pod id;
    int curr_pod_id = pod_id + pod;
    //int V_per_pod = (V+numpods-1)/numpods;
    //int V_start = std::min(V, curr_pod_id*V_per_pod);
    //int V_end   = std::min(V, V_start+V_per_pod);
    int V_start = V_range[curr_pod_id];
    int V_end = V_range[curr_pod_id+1];
    printf("curr_pod_id=%d, V_start=%d, V_end=%d\n", curr_pod_id, V_start, V_end);

    // Allocate memory on device;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (V+1)*sizeof(int), &d_in_indices));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (E)*sizeof(int), &d_in_neighbors));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (V)*sizeof(float), &d_out_degree_inv));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (V)*sizeof(float), &d_new_rank));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (V)*sizeof(float), &d_contrib));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (V)*sizeof(float), &d_contrib_new));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (V)*sizeof(int), &d_contrib_new));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sizeof(int), &d_start_id));

    // DMA transfer;
    printf("Transferring data: pod %d\n", pod);
    std::vector<hb_mc_dma_htod_t> htod_job;
    htod_job.push_back({d_in_indices, rev_offsets, (V+1)*sizeof(int)});
    htod_job.push_back({d_in_neighbors, rev_nonzeros, (E)*sizeof(int)});
    htod_job.push_back({d_out_degree_inv, out_degree_inv, (V)*sizeof(float)});
    htod_job.push_back({d_contrib, contrib, (V)*sizeof(int)});
    htod_job.push_back({d_start_id, &V_start, sizeof(int)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_device(&device, htod_job.data(), htod_job.size()));

    // CUDA args;
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 9
    uint32_t cuda_argv[CUDA_ARGC] = {
      d_in_indices,
      d_in_neighbors,
      d_out_degree_inv,
      d_new_rank,
      d_contrib,
      d_contrib_new,
      d_start_id,
      V_end,
      pod
    };

    // Enqueue kernel;
    printf("Enqueing kernel: pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_kernel_enqueue(&device, grid_dim, tg_dim, "kernel", CUDA_ARGC, cuda_argv));

    // host pagerank;
    host_pagerank(rev_offsets, rev_nonzeros,
                  out_degree_inv,
                  contrib,
                  contrib_new,
                  new_rank,
                  V_start, V_end);
  }

  // Launch pods;
  printf("Launching all pods\n");
  BSG_CUDA_CALL(hb_mc_device_pods_kernels_execute(&device));


  // Read from device;
  float *actual_contrib_new = (float *) malloc(V*sizeof(float));
  float *actual_new_rank = (float *) malloc(V*sizeof(float));

  hb_mc_device_foreach_pod_id(&device, pod)
  {
    printf("Reading results: pods %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));

    int curr_pod_id = pod_id + pod;
    //int V_per_pod = (V+numpods-1)/numpods;
    //int V_start = std::min(V, curr_pod_id*V_per_pod);
    //int V_end = std::min(V, V_start+V_per_pod);
    int V_start = V_range[curr_pod_id];
    int V_end = V_range[curr_pod_id+1];

    std::vector<hb_mc_dma_dtoh_t> dtoh_job;
    dtoh_job.push_back({d_contrib_new, actual_contrib_new, V*sizeof(float)});
    dtoh_job.push_back({d_new_rank, actual_new_rank, V*sizeof(float)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_host(&device, dtoh_job.data(), dtoh_job.size()));

    // validate
    float sse0 = 0.0f;
    float sse1 = 0.0f;
    for (int v = V_start; v < V_end; v++) {
      // check contrib_new;
      printf("[%d] contrib_new: hb=%f, cpu=%f\n", v, actual_contrib_new[v], contrib_new[v]);
      int isinf0 = isinf(actual_contrib_new[v]);
      int isinf1 = isinf(contrib_new[v]);
      if (isinf0 || isinf1) {
        if (isinf0 != isinf1) {
          return HB_MC_FAIL;
        }
      } else {
        sse0 += ((actual_contrib_new[v]-contrib_new[v])*(actual_contrib_new[v]-contrib_new[v]));
      }

      // check new_rank;
      printf("[%d] new_rank:    hb=%f, cpu=%f\n", v, actual_new_rank[v],    new_rank[v]);
      isinf0 = isinf(actual_new_rank[v]);
      isinf1 = isinf(new_rank[v]);
      if (isinf0 || isinf1) {
        if (isinf0 != isinf1) {
          return HB_MC_FAIL;
        }
      } else {
        sse1 += ((actual_new_rank[v]-new_rank[v])*(actual_new_rank[v]-new_rank[v]));
      }
    }

    printf("sse0=%f\n",sse0);
    printf("sse1=%f\n",sse1);
    if (sse0 > 0.001f) return HB_MC_FAIL; 
    if (sse1 > 0.001f) return HB_MC_FAIL; 
  }

  // Finish
  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  return HB_MC_SUCCESS; 
}



declare_program_main("pagerank_multipod", pagerank_multipod);
