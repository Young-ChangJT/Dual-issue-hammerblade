// common_fft.hpp
// FFT utilities and an optimized 256-point radix-2 DIT implementation

#include "bsg_manycore.h"
#include "bsg_set_tile_x_y.h"

#include <complex>
#include "reverse256.hpp"

// This library has sinf/cosf functions specialized for the following
// number of points
#define MAX_NUM_POINTS      256
#define MAX_LOG2_NUM_POINTS 8
#define NUM_POINTS      256
#define LOG2_NUM_POINTS 8

typedef std::complex<float> FP32Complex;


/*******************************************************************************
 * Efficient sinf and cosf implementation
*******************************************************************************/
// NOTE: both sinf and cosf here are specialized for a specific input size
// Return sin(-2*pi*x/NUM_POINTS) and cos(-2*pi*x/NUM_POINTS)

// 65 elements = 260B
float sinf_pi_over_2[(MAX_NUM_POINTS>>2)+1] = {
    0.0000000000000000f,
    0.0245412285229123f,
    0.0490676743274180f,
    0.0735645635996674f,
    0.0980171403295606f,
    0.1224106751992162f,
    0.1467304744553617f,
    0.1709618887603012f,
    0.1950903220161282f,
    0.2191012401568698f,
    0.2429801799032639f,
    0.2667127574748984f,
    0.2902846772544623f,
    0.3136817403988915f,
    0.3368898533922201f,
    0.3598950365349881f,
    0.3826834323650898f,
    0.4052413140049899f,
    0.4275550934302821f,
    0.4496113296546065f,
    0.4713967368259976f,
    0.4928981922297840f,
    0.5141027441932217f,
    0.5349976198870972f,
    0.5555702330196022f,
    0.5758081914178453f,
    0.5956993044924334f,
    0.6152315905806268f,
    0.6343932841636455f,
    0.6531728429537768f,
    0.6715589548470183f,
    0.6895405447370668f,
    0.7071067811865475f,
    0.7242470829514669f,
    0.7409511253549591f,
    0.7572088465064845f,
    0.7730104533627370f,
    0.7883464276266062f,
    0.8032075314806448f,
    0.8175848131515837f,
    0.8314696123025452f,
    0.8448535652497070f,
    0.8577286100002721f,
    0.8700869911087113f,
    0.8819212643483549f,
    0.8932243011955153f,
    0.9039892931234433f,
    0.9142097557035307f,
    0.9238795325112867f,
    0.9329927988347388f,
    0.9415440651830208f,
    0.9495281805930367f,
    0.9569403357322089f,
    0.9637760657954398f,
    0.9700312531945440f,
    0.9757021300385286f,
    0.9807852804032304f,
    0.9852776423889412f,
    0.9891765099647810f,
    0.9924795345987100f,
    0.9951847266721968f,
    0.9972904566786902f,
    0.9987954562051724f,
    0.9996988186962042f,
    1.0000000000000000f
};

inline float
opt_fft_sinf(int x) {
    const int No2 = MAX_NUM_POINTS >> 1; // 128
    const int No4 = MAX_NUM_POINTS >> 2; // 64
    if ((x >= No4) && (x < No2)) {
        x = No2 - x;
    }
    return -sinf_pi_over_2[x];
}

inline float
opt_fft_cosf(int x) {
    const int No2 = MAX_NUM_POINTS >> 1; // 128
    const int No4 = MAX_NUM_POINTS >> 2; // 64
    if ((x >= No4) && (x < No2)) {
      return -sinf_pi_over_2[x-No4];
    } else {
      return sinf_pi_over_2[No4-x];
    }
}

/* End of sinf and cosf implementation */


inline void
load_strided(FP32Complex *dst, const FP32Complex *src) {
    int i = 0, strided_i = 0;
    for (; i < NUM_POINTS; i += 4, strided_i += 4*NUM_POINTS) {
        FP32Complex tmp0 = src[strided_i               ];
        FP32Complex tmp1 = src[strided_i + NUM_POINTS  ];
        FP32Complex tmp2 = src[strided_i + NUM_POINTS*2];
        FP32Complex tmp3 = src[strided_i + NUM_POINTS*3];
        asm volatile("": : :"memory");
        dst[i    ] = tmp0;
        dst[i + 1] = tmp1;
        dst[i + 2] = tmp2;
        dst[i + 3] = tmp3;
    }
}

inline void
load_sequential(FP32Complex *dst, const FP32Complex *src) {
    for (int i = 0; i < NUM_POINTS; i += 4) {
        FP32Complex tmp0 = src[i  ];
        FP32Complex tmp1 = src[i+1];
        FP32Complex tmp2 = src[i+2];
        FP32Complex tmp3 = src[i+3];
        asm volatile("": : :"memory");
        dst[i  ] = tmp0;
        dst[i+1] = tmp1;
        dst[i+2] = tmp2;
        dst[i+3] = tmp3;
    }
}


inline void
store_strided(FP32Complex *dst, const FP32Complex *src) {
    int i = 0, strided_i = 0;
    for (; i < NUM_POINTS; i += 4, strided_i += 4*NUM_POINTS) {
        FP32Complex tmp0 = src[i    ];
        FP32Complex tmp1 = src[i + 1];
        FP32Complex tmp2 = src[i + 2];
        FP32Complex tmp3 = src[i + 3];
        asm volatile("": : :"memory");
        dst[strided_i              ] = tmp0;
        dst[strided_i + NUM_POINTS  ] = tmp1;
        dst[strided_i + NUM_POINTS*2] = tmp2;
        dst[strided_i + NUM_POINTS*3] = tmp3;
    }
}


inline void
opt_bit_reverse(FP32Complex *list) {
  #define REV_UNROLL 2
  for (int i = 0; i < NUM_REVERSE*2; i+=REV_UNROLL*2) {
    unsigned char a0 = reverse256[i+0];
    unsigned char b0 = reverse256[i+1];
    unsigned char a1 = reverse256[i+2];
    unsigned char b1 = reverse256[i+3];
    FP32Complex temp_a0 = list[a0];
    FP32Complex temp_a1 = list[a1];
    FP32Complex temp_b0 = list[b0];
    FP32Complex temp_b1 = list[b1];
    list[b0] = temp_a0;
    list[b1] = temp_a1;
    list[a0] = temp_b0;
    list[a1] = temp_b1;
  }
}


// In-place implementation
void fft256_specialized(FP32Complex *list) {

  opt_bit_reverse(list);


  int even_idx, odd_idx;
  float exp1_re, exp1_im;
  float tw1_re, tw1_im;
  float even1_re, even1_im;
  float odd1_re, odd1_im;
  float even_res1_re, even_res1_im;
  float odd_res1_re,  odd_res1_im;

  float exp2_re, exp2_im;
  float tw2_re, tw2_im;
  float even2_re, even2_im;
  float odd2_re, odd2_im;
  float even_res2_re, even_res2_im;
  float odd_res2_re,  odd_res2_im;

  float tw1_re_temp, tw1_im_temp;
  float tw2_re_temp, tw2_im_temp;

  int n = 2;
  int lshift = MAX_LOG2_NUM_POINTS-1;

  while (n < NUM_POINTS) {
    int half_n = (n/2);

    for (int k = 0; k < half_n; k++) {
      int x = k << lshift;
      exp1_re = opt_fft_cosf(x);
      exp1_im = opt_fft_sinf(x);

      even_idx = k;
      odd_idx = k + half_n;
      for (int i = 0; i < NUM_POINTS; i += 2*n) {
        odd1_re = list[odd_idx].real();
        odd1_im = list[odd_idx].imag();
        odd2_re = list[odd_idx+n].real();
        odd2_im = list[odd_idx+n].imag();
        even1_re = list[even_idx].real();
        even1_im = list[even_idx].imag();
        even2_re = list[even_idx+n].real();
        even2_im = list[even_idx+n].imag();

        asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
          : [rd] "=f" (tw1_re_temp) \
          : [rs1] "f" (odd1_im), [rs2] "f" (exp1_im));
        asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
          : [rd] "=f" (tw1_im_temp) \
          : [rs1] "f" (odd1_re), [rs2] "f" (exp1_im));
        asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
          : [rd] "=f" (tw2_re_temp) \
          : [rs1] "f" (odd2_im), [rs2] "f" (exp1_im));
        asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
          : [rd] "=f" (tw2_im_temp) \
          : [rs1] "f" (odd2_re), [rs2] "f" (exp1_im));

        asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
          : [rd] "=f" (tw1_re) \
          : [rs1] "f" (odd1_re), [rs2] "f" (exp1_re), [rs3] "f" (tw1_re_temp));
        asm volatile ("fmadd.s %[rd], %[rs1], %[rs2], %[rs3]" \
          : [rd] "=f" (tw1_im) \
          : [rs1] "f" (odd1_im), [rs2] "f" (exp1_re), [rs3] "f" (tw1_im_temp));
        asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
          : [rd] "=f" (tw2_re) \
          : [rs1] "f" (odd2_re), [rs2] "f" (exp1_re), [rs3] "f" (tw2_re_temp));
        asm volatile ("fmadd.s %[rd], %[rs1], %[rs2], %[rs3]" \
          : [rd] "=f" (tw2_im) \
          : [rs1] "f" (odd2_im), [rs2] "f" (exp1_re), [rs3] "f" (tw2_im_temp));

        even_res1_re = even1_re + tw1_re;
        even_res1_im = even1_im + tw1_im;
        odd_res1_re = even1_re - tw1_re;
        odd_res1_im = even1_im - tw1_im;
        even_res2_re = even2_re + tw2_re;
        even_res2_im = even2_im + tw2_im;
        odd_res2_re = even2_re - tw2_re;
        odd_res2_im = even2_im - tw2_im;

        list[even_idx]   = FP32Complex(even_res1_re, even_res1_im);
        list[odd_idx]    = FP32Complex(odd_res1_re, odd_res1_im);
        list[even_idx+n] = FP32Complex(even_res2_re, even_res2_im);
        list[odd_idx+n]  = FP32Complex(odd_res2_re, odd_res2_im);

        even_idx += 2*n;
        odd_idx += 2*n;
      }
    }

    n = n*2;
    lshift--;
  }


  // last stage
  odd_idx  = NUM_POINTS/2;
  for (int i = 0; i < NUM_POINTS/2; i += 2) {
    exp1_re = opt_fft_cosf(i);
    exp1_im = opt_fft_sinf(i);
    exp2_re = opt_fft_cosf(i+1);
    exp2_im = opt_fft_sinf(i+1);
 
    odd1_re = list[odd_idx].real();
    odd1_im = list[odd_idx].imag();
    odd2_re = list[odd_idx+1].real();
    odd2_im = list[odd_idx+1].imag();
    even1_re = list[i].real();
    even1_im = list[i].imag();
    even2_re = list[i+1].real();
    even2_im = list[i+1].imag();

    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (tw1_re_temp) \
      : [rs1] "f" (odd1_im), [rs2] "f" (exp1_im));
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (tw1_im_temp) \
      : [rs1] "f" (odd1_re), [rs2] "f" (exp1_im));
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (tw2_re_temp) \
      : [rs1] "f" (odd2_im), [rs2] "f" (exp2_im));
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (tw2_im_temp) \
      : [rs1] "f" (odd2_re), [rs2] "f" (exp2_im));
    asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (tw1_re) \
      : [rs1] "f" (odd1_re), [rs2] "f" (exp1_re), [rs3] "f" (tw1_re_temp));
    asm volatile ("fmadd.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (tw1_im) \
      : [rs1] "f" (odd1_im), [rs2] "f" (exp1_re), [rs3] "f" (tw1_im_temp));
    asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (tw2_re) \
      : [rs1] "f" (odd2_re), [rs2] "f" (exp2_re), [rs3] "f" (tw2_re_temp));
    asm volatile ("fmadd.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (tw2_im) \
      : [rs1] "f" (odd2_im), [rs2] "f" (exp2_re), [rs3] "f" (tw2_im_temp));

    even_res1_re = even1_re + tw1_re;
    even_res1_im = even1_im + tw1_im;
    odd_res1_re = even1_re - tw1_re;
    odd_res1_im = even1_im - tw1_im;
    even_res2_re = even2_re + tw2_re;
    even_res2_im = even2_im + tw2_im;
    odd_res2_re = even2_re - tw2_re;
    odd_res2_im = even2_im - tw2_im;

    list[i]          = FP32Complex(even_res1_re, even_res1_im);
    list[odd_idx]    = FP32Complex(odd_res1_re, odd_res1_im);
    list[i+1]        = FP32Complex(even_res2_re, even_res2_im);
    list[odd_idx+1]  = FP32Complex(odd_res2_re, odd_res2_im);
   
    odd_idx += 2;
  }



}


__attribute__ ((always_inline))
void twiddle_scaling( FP32Complex *local_lst,
                      FP32Complex *tw)
{
  float w0_re, w0_im;
  float w1_re, w1_im;
  float w2_re, w2_im;
  float w3_re, w3_im;
  float l0_re, l0_im;
  float l1_re, l1_im;
  float l2_re, l2_im;
  float l3_re, l3_im;
  float res0_re, res0_im, res0_re_temp, res0_im_temp;
  float res1_re, res1_im, res1_re_temp, res1_im_temp;
  float res2_re, res2_im, res2_re_temp, res2_im_temp;
  float res3_re, res3_im, res3_re_temp, res3_im_temp;

  for (int c = 0; c < NUM_POINTS; c += 4) {
    w0_re = tw[c+0].real();
    w0_im = tw[c+0].imag();
    w1_re = tw[c+1].real();
    w1_im = tw[c+1].imag();
    w2_re = tw[c+2].real();
    w2_im = tw[c+2].imag();
    w3_re = tw[c+3].real();
    w3_im = tw[c+3].imag();
    l0_re = local_lst[c+0].real();
    l0_im = local_lst[c+0].imag();
    l1_re = local_lst[c+1].real();
    l1_im = local_lst[c+1].imag();
    l2_re = local_lst[c+2].real();
    l2_im = local_lst[c+2].imag();
    l3_re = local_lst[c+3].real();
    l3_im = local_lst[c+3].imag();
    
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (res0_re_temp) \
      : [rs1] "f" (w0_im), [rs2] "f" (l0_im));
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (res0_im_temp) \
      : [rs1] "f" (w0_im), [rs2] "f" (l0_re));
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (res1_re_temp) \
      : [rs1] "f" (w1_im), [rs2] "f" (l1_im));
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (res0_im_temp) \
      : [rs1] "f" (w1_im), [rs2] "f" (l1_re));
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (res2_re_temp) \
      : [rs1] "f" (w2_im), [rs2] "f" (l2_im));
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (res2_im_temp) \
      : [rs1] "f" (w2_im), [rs2] "f" (l2_re));
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (res3_re_temp) \
      : [rs1] "f" (w3_im), [rs2] "f" (l3_im));
    asm volatile ("fmul.s %[rd], %[rs1], %[rs2]" \
      : [rd] "=f" (res3_im_temp) \
      : [rs1] "f" (w3_im), [rs2] "f" (l3_re));

    asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (res0_re) \
      : [rs1] "f" (w0_re), [rs2] "f" (l0_re), [rs3] "f" (res0_re_temp));
    asm volatile ("fmadd.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (res0_im) \
      : [rs1] "f" (w0_re), [rs2] "f" (l0_im), [rs3] "f" (res0_im_temp));
    asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (res1_re) \
      : [rs1] "f" (w1_re), [rs2] "f" (l1_re), [rs3] "f" (res1_re_temp));
    asm volatile ("fmadd.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (res1_im) \
      : [rs1] "f" (w1_re), [rs2] "f" (l1_im), [rs3] "f" (res1_im_temp));
    asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (res2_re) \
      : [rs1] "f" (w2_re), [rs2] "f" (l2_re), [rs3] "f" (res2_re_temp));
    asm volatile ("fmadd.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (res2_im) \
      : [rs1] "f" (w2_re), [rs2] "f" (l2_im), [rs3] "f" (res2_im_temp));
    asm volatile ("fmsub.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (res3_re) \
      : [rs1] "f" (w3_re), [rs2] "f" (l3_re), [rs3] "f" (res3_re_temp));
    asm volatile ("fmadd.s %[rd], %[rs1], %[rs2], %[rs3]" \
      : [rd] "=f" (res3_im) \
      : [rs1] "f" (w3_re), [rs2] "f" (l3_im), [rs3] "f" (res3_im_temp));


    local_lst[c+0] = FP32Complex(res0_re, res0_im);
    local_lst[c+1] = FP32Complex(res1_re, res1_im);
    local_lst[c+2] = FP32Complex(res2_re, res2_im);
    local_lst[c+3] = FP32Complex(res3_re, res3_im);
  }
}

