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
#define DRAM_LIST_NODES 16777216 // (=2**24)

void read_graph(int* ptr, std::string filename) {
  std::ifstream fh(filename);
  int idx = 0;
  int num;
  while (fh >> num) {
    ptr[idx] = num;
    idx++;
  }
}

void read_graph_float(float* ptr, std::string filename) {
  std::ifstream fh(filename);
  int idx = 0;
  float num;
  while (fh >> num) {
    ptr[idx] = num;
    idx++;
  }
}

void get_submatrix(int row_start, int row_end,
                   int col_start, int col_end,
                   int *row_offset, int *col_idx, float *nnz,
                   std::vector<int>& sub_row_offset, 
                   std::vector<int>& sub_col_idx, 
                   std::vector<float>& sub_nnz) {

  int offset = 0;
  for (int row = row_start; row < row_end; row++) {
    sub_row_offset.push_back(offset);
    int start = row_offset[row];
    int end = row_offset[row+1];
    for (int i = start; i < end; i++) {
      if ((col_idx[i] >= col_start) && (col_idx[i] < col_end)) {
        sub_col_idx.push_back(col_idx[i]-col_start);
        sub_nnz.push_back(nnz[i]);
        offset++;
      }
    } 
  }
  sub_row_offset.push_back(offset);
} 

int spgemm_multipod(int argc, char ** argv)
{
  int r = 0;
  
  // command line arguments;
  const char * bin_path = argv[1];
  std::string row_offset_file = argv[2];
  std::string col_idx_file = argv[3];
  std::string nnz_file  = argv[4];
  std::string output_row_offset_file = argv[5];
  std::string output_col_idx_file = argv[6];
  std::string output_nnz_file = argv[7];
  std::cout << "row_offset_file = "  << row_offset_file << std::endl;
  std::cout << "col_idx_file = "  << col_idx_file << std::endl;
  std::cout << "nnz_file = "  << nnz_file << std::endl;
  std::cout << "output_row_offset_file = "  << output_row_offset_file << std::endl;
  std::cout << "output_col_idx_file = "  << output_col_idx_file << std::endl;
  std::cout << "output_nnz_file = "  << output_nnz_file << std::endl;

  // Define parameters;
  int pod_id = PODID;
  //int pod_id_x = PODID % POD_DIM_X;
  //int pod_id_y = PODID / POD_DIM_X;
  int pod_dim_x = POD_DIM_X;
  int pod_dim_y = POD_DIM_Y;
  int V = VERTEX;
  int E = EDGE;
  int OUTPUT_E = OUTPUT_EDGE;
  printf("pod_id = %d\n", pod_id);
  //printf("pod_id_x = %d\n", pod_id_x);
  //printf("pod_id_y = %d\n", pod_id_y);
  printf("pod_dim_x= %d\n", pod_dim_x);
  printf("pod_dim_y= %d\n", pod_dim_y);
  printf("V=%d\n", V);
  printf("E=%d\n", E);
  printf("OUTPUT_E=%d\n", OUTPUT_E);

  // Read matrix;
  int *row_offset = (int *) malloc(sizeof(int)*(V+1));
  int *col_idx    = (int *) malloc(sizeof(int)*(E));
  float *nnz        = (float *) malloc(sizeof(float)*(E));
  int *output_row_offset = (int *) malloc(sizeof(int)*(V+1));
  int *output_col_idx    = (int *) malloc(sizeof(int)*(OUTPUT_E));
  float *output_nnz        = (float *) malloc(sizeof(float)*(OUTPUT_E));
  read_graph(row_offset, row_offset_file);
  read_graph(col_idx, col_idx_file);
  read_graph_float(nnz, nnz_file);
  read_graph(output_row_offset, output_row_offset_file);
  read_graph(output_col_idx, output_col_idx_file);
  read_graph_float(output_nnz, output_nnz_file);

  // partition by row (vertex);
  int row_per_pod = (V+pod_dim_y-1)/pod_dim_y;
  int col_per_pod = (V+pod_dim_x-1)/pod_dim_x;
 
  // Initialize device;
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, "spgemm_multipod", HB_MC_DEVICE_ID));

  // Pod;
  hb_mc_pod_id_t pod;
  // matrix A
  hb_mc_eva_t d_A_row_offset[NUM_POD_X];
  hb_mc_eva_t d_A_col_idx[NUM_POD_X];
  hb_mc_eva_t d_A_nnz[NUM_POD_X];
  // matrix B
  hb_mc_eva_t d_B_row_offset[NUM_POD_X];
  hb_mc_eva_t d_B_col_idx[NUM_POD_X];
  hb_mc_eva_t d_B_nnz[NUM_POD_X];
  // matrix C
  hb_mc_eva_t d_C_row_offset[NUM_POD_X];
  hb_mc_eva_t d_C_col_idx[NUM_POD_X];
  hb_mc_eva_t d_C_nnz[NUM_POD_X];
  // needed by algorithm;
  hb_mc_eva_t d_C_col_count[NUM_POD_X];
  hb_mc_eva_t d_C_list_head[NUM_POD_X];
  hb_mc_eva_t d_dram_nodes[NUM_POD_X];
  

  hb_mc_device_foreach_pod_id(&device, pod)
  {
    // Loading program;
    printf("Loading program for pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

    // pod id;
    int curr_pod_id_x = (PODID+pod) % POD_DIM_X;
    int curr_pod_id_y = (PODID+pod) / POD_DIM_X;

    int row_start = std::min(curr_pod_id_y*row_per_pod,V);
    int row_end  = std::min(row_start+row_per_pod,V);
    int num_row = row_end-row_start;
    int col_start = std::min(curr_pod_id_x*col_per_pod,V);
    int col_end = std::min(col_start+col_per_pod,V);
    int num_col = col_end-col_start;

    //int nnz_start = row_offset[row_start];
    //int nnz_end = row_offset[row_end];
    //int num_nnz = nnz_end-nnz_start;
    printf("curr_pod_id=(%d %d), row_start=%d, row_end=%d, col_start=%d, col_end=%d\n",
      curr_pod_id_x, curr_pod_id_y,
      row_start, row_end,
      col_start, col_end);
    
    // sub matrix A;
    std::vector<int> sub_A_row_offset;
    std::vector<int> sub_A_col_idx;
    std::vector<float> sub_A_nnz;
    get_submatrix(row_start, row_end, 0, V,
      row_offset, col_idx, nnz,
      sub_A_row_offset, sub_A_col_idx, sub_A_nnz); 
    // sub matrix B;
    std::vector<int> sub_B_row_offset;
    std::vector<int> sub_B_col_idx;
    std::vector<float> sub_B_nnz;
    get_submatrix(0, V, col_start, col_end,
      row_offset, col_idx, nnz,
      sub_B_row_offset, sub_B_col_idx, sub_B_nnz); 
    // sub matrix C;
    std::vector<int> sub_C_row_offset;
    std::vector<int> sub_C_col_idx;
    std::vector<float> sub_C_nnz;
    get_submatrix(row_start, row_end, col_start, col_end,
      output_row_offset, output_col_idx, output_nnz,
      sub_C_row_offset, sub_C_col_idx, sub_C_nnz); 


    // Allocate memory on device;
    // matrix A
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sub_A_row_offset.size()*sizeof(int), &d_A_row_offset[pod]));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sub_A_col_idx.size()*sizeof(int), &d_A_col_idx[pod]));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sub_A_nnz.size()*sizeof(float), &d_A_nnz[pod]));
    // matrix B
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sub_B_row_offset.size()*sizeof(int), &d_B_row_offset[pod]));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sub_B_col_idx.size()*sizeof(int), &d_B_col_idx[pod]));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sub_B_nnz.size()*sizeof(float), &d_B_nnz[pod]));
    // matrix C
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sub_C_row_offset.size()*sizeof(int), &d_C_row_offset[pod]));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sub_C_col_idx.size()*sizeof(int), &d_C_col_idx[pod]));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, sub_C_nnz.size()*sizeof(float), &d_C_nnz[pod]));
    // needed by algorithm
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (num_row+1)*sizeof(int), &d_C_col_count[pod]));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (num_row)*sizeof(int), &d_C_list_head[pod]));
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, (DRAM_LIST_NODES)*3*sizeof(int), &d_dram_nodes[pod]));
    
    // DMA transfer;
    printf("Transferring data: pod %d\n", pod);
    std::vector<hb_mc_dma_htod_t> htod_job;
    // matrix A;
    htod_job.push_back({d_A_row_offset[pod], sub_A_row_offset.data(), sub_A_row_offset.size()*sizeof(int)});
    htod_job.push_back({d_A_col_idx[pod], sub_A_col_idx.data(), sub_A_col_idx.size()*sizeof(int)});
    htod_job.push_back({d_A_nnz[pod], sub_A_nnz.data(), sub_A_nnz.size()*sizeof(float)});
    // matrix B
    htod_job.push_back({d_B_row_offset[pod], sub_B_row_offset.data(), sub_B_row_offset.size()*sizeof(int)});
    htod_job.push_back({d_B_col_idx[pod], sub_B_col_idx.data(), sub_B_col_idx.size()*sizeof(int)});
    htod_job.push_back({d_B_nnz[pod], sub_B_nnz.data(), sub_B_nnz.size()*sizeof(float)});
    // C col count;
    int *device_C_col_count = (int*) malloc(sizeof(int)*(num_row+1)); 
    for (int i = 0; i < num_row+1; i++) {
      device_C_col_count[i] = 0;
    }
    htod_job.push_back({d_C_col_count[pod], device_C_col_count, (num_row+1)*sizeof(int)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_device(&device, htod_job.data(), htod_job.size()));



    // CUDA args;
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 14
    uint32_t cuda_argv[CUDA_ARGC] = {
      d_A_row_offset[pod],
      d_A_col_idx[pod],
      d_A_nnz[pod],
      d_B_row_offset[pod],
      d_B_col_idx[pod],
      d_B_nnz[pod],
      d_C_row_offset[pod],
      d_C_col_idx[pod],
      d_C_nnz[pod],
      d_C_col_count[pod],
      d_C_list_head[pod],
      d_dram_nodes[pod],
      num_row,
      pod
    };

    // Enqueue kernel;
    printf("Enqueing kernel: pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_kernel_enqueue(&device, grid_dim, tg_dim, "kernel", CUDA_ARGC, cuda_argv));

    // Free;
    free(device_C_col_count);
  }

  // Launch pods;
  printf("Launching all pods\n");
  BSG_CUDA_CALL(hb_mc_device_pods_kernels_execute(&device));

  // Read from device;
  bool fail = false;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    printf("Reading results: pods %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    int curr_pod_id_x = (PODID+pod) % POD_DIM_X;
    int curr_pod_id_y = (PODID+pod) / POD_DIM_X;
    int row_start = std::min(curr_pod_id_y*row_per_pod,V);
    int row_end  = std::min(row_start+row_per_pod,V);
    int num_row = row_end-row_start;
    int col_start = std::min(curr_pod_id_x*col_per_pod,V);
    int col_end = std::min(col_start+col_per_pod,V);
    int num_col = col_end-col_start;

    // matrix C;
    std::vector<int> sub_C_row_offset;
    std::vector<int> sub_C_col_idx;
    std::vector<float> sub_C_nnz;
    get_submatrix(row_start, row_end, col_start, col_end,
      output_row_offset, output_col_idx, output_nnz,
      sub_C_row_offset, sub_C_col_idx, sub_C_nnz); 

    // DMA transfer; device -> host;
    std::vector<hb_mc_dma_dtoh_t> dtoh_job;
    int *actual_C_row_offset  = (int*) malloc(sub_C_row_offset.size()*sizeof(int));
    int *actual_C_col_idx     = (int*) malloc(sub_C_col_idx.size()*sizeof(int));
    float *actual_C_nnz       = (float*) malloc(sub_C_nnz.size()*sizeof(float));
    dtoh_job.push_back({d_C_row_offset[pod], actual_C_row_offset, sub_C_row_offset.size()*sizeof(int)});
    dtoh_job.push_back({d_C_col_idx[pod], actual_C_col_idx, sub_C_col_idx.size()*sizeof(int)});
    dtoh_job.push_back({d_C_nnz[pod], actual_C_nnz, sub_C_nnz.size()*sizeof(float)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_host(&device, dtoh_job.data(), dtoh_job.size()));

    // Validate;
    // row_offset;
    for (int row = 0; row < sub_C_row_offset.size(); row++) {
      int expected = sub_C_row_offset[row];
      int actual = actual_C_row_offset[row];
      //printf("row_offset : row=%d, expected=%d, actual=%d\n",
      //  row, expected, actual);
      if (expected != actual) {
        printf("row_offset mismatch: row=%d, expected=%d, actual=%d\n",
          row, expected, actual);
        fail = true;
      }
    }
    // col_idx;
    for (int nz = 0; nz < sub_C_col_idx.size(); nz++) {
      int expected = sub_C_col_idx[nz];
      int actual = actual_C_col_idx[nz];
      //printf("col_idx : nz=%d, expected=%d, actual=%d\n",
      //  nz, expected, actual);
      if (expected != actual) {
        printf("col_idx mismatch: nz=%d, expected=%d, actual=%d\n",
          nz, expected, actual);
        fail = true;
      }
    }
    // nnz;
    for (int nz = 0; nz < sub_C_nnz.size(); nz++) {
      float expected = sub_C_nnz[nz];
      float actual = actual_C_nnz[nz];
      //printf("nnz: nz=%d, expected=%f, actual=%f\n",
      //  nz, expected, actual);
      if (expected != actual) {
        printf("nnz mismatch: nz=%d, expected=%f, actual=%f\n",
          nz, expected, actual);
        fail = true;
      }
    }
  }
  
  // FINISH;
  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  if (fail) {
    return HB_MC_FAIL;
  } else {
    return HB_MC_SUCCESS;
  }
}

declare_program_main("spgemm_multipod", spgemm_multipod);

