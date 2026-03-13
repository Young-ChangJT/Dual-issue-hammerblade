#include <bsg_manycore_errno.h>
#include <bsg_manycore_cuda.h>
#include <cstdlib>
#include <time.h>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <bsg_manycore_regression.h>
#include <bsg_manycore.h>
#include <cstdint>
#include <vector>
#include "bs.hpp"
#include "option_data.hpp"

#define ALLOC_NAME "default_allocator"


void read_input(const char* input_path, OptionData* options, int num_option) {
  FILE* file = fopen(input_path, "r");
  int rv;
  int numOptions;

  // read first line;
  fscanf(file, "%i", &numOptions);

  // read option data;
  for (int i = 0; i < num_option; i++) {
    float s, strike, r, divq, v, t;
    float unused0, unused1;
    int unused2;
    fscanf(file, "%f %f %f %f %f %f %c %f %f",
      &s, &strike, &r, &divq, &v, &t,
      &unused2, &unused0, &unused1);
    options[i].s = s;
    options[i].strike = strike;
    options[i].r = r;
    options[i].v = v;
    options[i].t = t;
    options[i].call = 0.0f;
    options[i].put = 0.0f;
    options[i].unused = 0.0f;
  }

  fclose(file);
}


// Host main;
int bs_multipod(int argc, char ** argv) {
  int r = 0;
  
  // command line;
  const char *bin_path = argv[1];
  const char *input_path = argv[2];

  // parameters;
  int num_option = NUM_OPTION; // per pod;
  printf("num_option=%d\n", num_option);

  // Prepare inputs;
  OptionData *options = (OptionData*) malloc(num_option*sizeof(OptionData));
  read_input(input_path, options, num_option);
  

  // Initialize devices;
  hb_mc_device_t device;
  BSG_CUDA_CALL(hb_mc_device_init(&device, "bs_multipod", HB_MC_DEVICE_ID));

  eva_t d_options;

  hb_mc_pod_id_t pod;
  hb_mc_device_foreach_pod_id(&device, pod)
  {
    printf("Loading program for pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));
    BSG_CUDA_CALL(hb_mc_device_program_init(&device, bin_path, ALLOC_NAME, 0));

    // Allocate memory on device;
    BSG_CUDA_CALL(hb_mc_device_malloc(&device, num_option*sizeof(OptionData), &d_options));

    // DMA transfer;
    printf("Transferring data: pod %d\n", pod);
    std::vector<hb_mc_dma_htod_t> htod_job;
    htod_job.push_back({d_options, options, num_option*sizeof(OptionData)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_device(&device, htod_job.data(), htod_job.size()));

    // Cuda args;
    hb_mc_dimension_t tg_dim = { .x = bsg_tiles_X, .y = bsg_tiles_Y};
    hb_mc_dimension_t grid_dim = { .x = 1, .y = 1};
    #define CUDA_ARGC 3
    uint32_t cuda_argv[CUDA_ARGC] = {d_options, num_option, pod};

    // Enqueue kernel;
    printf("Enqueue Kernel: pod %d\n", pod);
    BSG_CUDA_CALL(hb_mc_kernel_enqueue (&device, grid_dim, tg_dim, "kernel", CUDA_ARGC, cuda_argv));
  }


  // Launch pods;
  printf("Launching all pods\n");
  BSG_CUDA_CALL(hb_mc_device_pods_kernels_execute(&device));

  // Host calculation;
  for (int i = 0; i < num_option; i++) {
    BlkSchlsEqEuroNoDiv(&options[i]);
  }

  // Read from devices;
  OptionData *actual_options = (OptionData*) malloc(num_option*sizeof(OptionData));
 

  bool fail = false;
  hb_mc_device_foreach_pod_id(&device, pod) {
    printf("Reading results: pods %d\n", pod);
    BSG_CUDA_CALL(hb_mc_device_set_default_pod(&device, pod));

    // clear buf;
    for (int i = 0; i < num_option; i++) {
      actual_options[i].call = 0.0f;
      actual_options[i].put  = 0.0f;
    }
    
    // DMA transfer: device -> host;
    std::vector<hb_mc_dma_dtoh_t> dtoh_job;
    dtoh_job.push_back({d_options, actual_options, num_option*sizeof(OptionData)});
    BSG_CUDA_CALL(hb_mc_device_transfer_data_to_host(&device, dtoh_job.data(), dtoh_job.size()));

    // Validate;
    float err = 0.0f;
    for (int i = 0; i < num_option; i++) {
      // Call;
      float actual_call = actual_options[i].call;
      float expected_call = options[i].call;
      printf("call %d: actual=%f, expected=%f\n", i, actual_call, expected_call);
      float diff = actual_call - expected_call;
      err += (diff*diff);
      // Put;
      float actual_put = actual_options[i].put;
      float expected_put = options[i].put;
      printf("put %d: actual=%f, expected=%f\n", i, actual_put, expected_put);
      diff = actual_put - expected_put;
      err += (diff*diff);
    }

    printf("err=%f\n", err);

    if (err > 0.01f) {
      fail = true;
    }
  } 


  // Finish;
  BSG_CUDA_CALL(hb_mc_device_finish(&device));
  if (fail) {
    return HB_MC_FAIL;
  } else {
    return HB_MC_SUCCESS;
  }

}


declare_program_main("bs_multipod", bs_multipod);
