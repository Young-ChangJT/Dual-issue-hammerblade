#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <random>
#include <limits>
#include <bsg_manycore_cuda.h>
#include <bsg_manycore_regression.h>
#include <bsg_manycore_eva.h>

#define ALLOC_NAME "default_allocator"

void host_mm_opt(float *result, float *mat1, float *mat2)
{
  for (int y = 0; y < N; y++) {
    for (int x =0; x < N; x++) {
      float sum = 0.0f;
      for (int z = 0; z < N; z++) {
        sum += mat1[(N*y)+z]*mat2[(N*z)+x];
      }
      result[(N*y)+x] = sum;
    }
  }
}

int kernel_matrix_mul (int argc, char **argv)
{
      int rc;
      char *bin_path, *test_name;
      struct arguments_path args = {NULL, NULL};

      argp_parse (&argp_path, argc, argv, 0, 0, &args);
      bin_path = args.path;
      test_name = args.name;

      // Define path to binary.
      // Initialize device, load binary and unfreeze tiles.
      hb_mc_device_t device;
      BSG_CUDA_CALL(hb_mc_device_init(&device, test_name, 0));


      hb_mc_pod_id_t pod;
      hb_mc_device_foreach_pod_id(&device, pod)
      {

        BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
        BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

        //************************************************************
        // Define tg_dim_x/y: number of tiles in each tile group
        // Calculate grid_dim_x/y: number of tile groups needed based on block_size_x/y
        //************************************************************
        hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y };
        hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};


        // Allocate memory on host;
        float *host_mat1;
        float *host_mat2;
        float *host_result;
        float *host_expected;
        host_mat1 = (float*) malloc(N*N*sizeof(float));
        host_mat2 = (float*) malloc(N*N*sizeof(float));
        host_result = (float*) malloc(N*N*sizeof(float));
        host_expected = (float*) malloc(N*N*sizeof(float));
      
        for (int y = 0; y < N; y++) {
          for (int x = 0; x < N; x++) {
            int i = (N*y) + x;
            host_mat1[i] = i;
            if (y == x) {
              host_mat2[i] = 1.0f;
            } else {
              host_mat2[i] = 0.0f;
            }
            host_result[i] = 0.0f;
          }
        }

        host_mm_opt(host_expected, host_mat1, host_mat2);

        // allocate memory on device
        // Make it pod-cache aligned;
        #define CACHE_LINE_WORDS 16
        eva_t temp_device1, temp_device2;
        BSG_CUDA_CALL(hb_mc_device_malloc(&device, CACHE_LINE_WORDS*sizeof(int), &temp_device1));
        printf("temp Addr: %x\n", temp_device1);
        int align_size = (32)-1-((temp_device1>>2)%(CACHE_LINE_WORDS*32)/CACHE_LINE_WORDS);
        BSG_CUDA_CALL(hb_mc_device_malloc(&device, align_size*sizeof(int)*CACHE_LINE_WORDS, &temp_device2));

        eva_t device_mat1, device_mat2, device_result;
        BSG_CUDA_CALL(hb_mc_device_malloc(&device, N*N*sizeof(float), &device_mat1));
        BSG_CUDA_CALL(hb_mc_device_malloc(&device, N*N*sizeof(float), &device_mat2));
        BSG_CUDA_CALL(hb_mc_device_malloc(&device, N*N*sizeof(float), &device_result));
        printf("device_mat1   = %x\n", device_mat1);
        printf("device_mat2   = %x\n", device_mat2);
        printf("device_result = %x\n", device_result);

        // Transfer mat1, mat2
        hb_mc_dma_htod_t htod_job[] = {
          {
            .d_addr = device_mat1,
            .h_addr = (void *) host_mat1,
            .size = N*N*sizeof(float)
          },
          {
            .d_addr = device_mat2,
            .h_addr = (void *) host_mat2,
            .size = N*N*sizeof(float)
          }
        };

        BSG_CUDA_CALL(hb_mc_device_dma_to_device(&device, htod_job, 2));


        // cuda arguments
        #define CUDA_ARGC 3
        uint32_t cuda_argv[CUDA_ARGC] = {device_mat1, device_mat2, device_result};

        // Enqueue kernel
        bsg_pr_info("Enqueue Kernel\n");
        BSG_CUDA_CALL(hb_mc_kernel_enqueue (&device, grid_dim, tg_dim, "kernel_mm_opt", CUDA_ARGC, cuda_argv));

        // Execute kernel.        
        bsg_pr_info("Execute Kernel\n");
        BSG_CUDA_CALL(hb_mc_device_tile_groups_execute(&device));


        // copy result.
        bsg_pr_info("Copying result back\n");
        hb_mc_dma_dtoh_t dtoh_job [] = {
          {
            .d_addr = device_result,
            .h_addr = (void *) &host_result[0],
            .size = N*N * sizeof(float)
          }
        };

        BSG_CUDA_CALL(hb_mc_device_dma_to_host(&device, dtoh_job, 1));

        //************************************************************
        // Freeze the tiles and memory manager cleanup.
        //************************************************************
        BSG_CUDA_CALL(hb_mc_device_finish(&device));

        // validate
        float sse = 0.0f;
        for(int i = 0; i < N*N; ++i){
          float diff =  (host_result[i] - host_expected[i]);
          sse += diff*diff;
          if (diff != 0.0f) {
            printf("[%d] actual=%f, expected=%f\n", i, host_result[i], host_expected[i]);
          }
        }
        if (sse >= .01) {
          bsg_pr_err(BSG_RED("Matrix Mismatch.(SSE: %f)\n"), sse);
          return HB_MC_FAIL;
        }

        bsg_pr_test_info(BSG_GREEN("Matrix Match. (SSE: %f)\n"), sse);

      }

      return HB_MC_SUCCESS;
}

#ifdef VCS
int vcs_main(int argc, char ** argv) {
#else
        int main(int argc, char ** argv) {
#endif
                bsg_pr_test_info("test_matrix_mul Regression Test\n");
                int rc = kernel_matrix_mul(argc, argv);
                bsg_pr_test_pass_fail(rc == HB_MC_SUCCESS);
                return rc;
        }


