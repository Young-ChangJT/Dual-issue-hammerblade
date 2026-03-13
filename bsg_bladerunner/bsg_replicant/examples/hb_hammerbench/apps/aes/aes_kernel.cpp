#include "aes_kernel.hpp"


// Private data;
const uint8_t sbox[256] __attribute__((section(".dmem")))= {
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16 };

// Local storage;
uint32_t localbuf0[AES_BLOCKLEN/sizeof(uint32_t)];
uint32_t localbuf1[AES_BLOCKLEN/sizeof(uint32_t)];
uint32_t localRoundKey[AES_keyExpSize/sizeof(uint32_t)];


// AddRoundKey
__attribute__ ((always_inline))
static void AddRoundKey(uint8_t round, state_t* state)
{
  uint32_t * state32 = (uint32_t *) state;
  uint32_t * RoundKey32 = (uint32_t *) (&localRoundKey[round * Nb]);
  uint32_t t0 = state32[0];
  uint32_t t1 = state32[1];
  uint32_t t2 = state32[2];
  uint32_t t3 = state32[3];
  uint32_t r0 = RoundKey32[0];
  uint32_t r1 = RoundKey32[1];
  uint32_t r2 = RoundKey32[2];
  uint32_t r3 = RoundKey32[3];
  asm volatile("" ::: "memory");
  state32[0] = t0 ^ r0;
  state32[1] = t1 ^ r1;
  state32[2] = t2 ^ r2;
  state32[3] = t3 ^ r3;
  asm volatile("" ::: "memory");
}


// SubBytes
__attribute__ ((always_inline))
static void SubBytes(uint8_t * state)
{
  uint8_t t0 = state[0];
  uint8_t t1 = state[1];
  uint8_t t2 = state[2];
  uint8_t t3 = state[3];
  uint8_t t4 = state[4];
  uint8_t t5 = state[5];
  uint8_t t6 = state[6];
  uint8_t t7 = state[7];
  asm volatile("" ::: "memory");
  uint8_t s0 = sbox[t0];
  uint8_t s1 = sbox[t1];
  uint8_t s2 = sbox[t2];
  uint8_t s3 = sbox[t3];
  uint8_t s4 = sbox[t4];
  uint8_t s5 = sbox[t5];
  uint8_t s6 = sbox[t6];
  uint8_t s7 = sbox[t7];
  asm volatile("" ::: "memory");
  state[0] = s0;
  state[1] = s1;
  state[2] = s2;
  state[3] = s3;
  state[4] = s4;
  state[5] = s5;
  state[6] = s6;
  state[7] = s7;
  asm volatile("" ::: "memory");

  t0 = state[8];
  t1 = state[9];
  t2 = state[10];
  t3 = state[11];
  t4 = state[12];
  t5 = state[13];
  t6 = state[14];
  t7 = state[15];
  asm volatile("" ::: "memory");
  s0 = sbox[t0];
  s1 = sbox[t1];
  s2 = sbox[t2];
  s3 = sbox[t3];
  s4 = sbox[t4];
  s5 = sbox[t5];
  s6 = sbox[t6];
  s7 = sbox[t7];
  asm volatile("" ::: "memory");
  state[8] = s0;
  state[9] = s1;
  state[10] = s2;
  state[11] = s3;
  state[12] = s4;
  state[13] = s5;
  state[14] = s6;
  state[15] = s7;
  asm volatile("" ::: "memory");
}


// ShiftRows
__attribute__ ((always_inline))
static void ShiftRows(uint8_t* state)
{
  uint8_t t1, t2, t3, t4;
  t1 = state[1];
  t2 = state[5];
  t3 = state[9];
  t4 = state[13];
  asm volatile("" ::: "memory");
  state[13] = t1;
  state[1] = t2;
  state[5] = t3;
  state[9] = t4;
  asm volatile("" ::: "memory");
  t1 = state[2];
  t2 = state[6];
  t3 = state[10];
  t4 = state[14];
  asm volatile("" ::: "memory");
  state[10] = t1;
  state[14] = t2;
  state[2] = t3;
  state[6] = t4;
  asm volatile("" ::: "memory");
  t1 = state[3];
  t2 = state[7];
  t3 = state[11];
  t4 = state[15];
  asm volatile("" ::: "memory");
  state[7] = t1;
  state[11] = t2;
  state[15] = t3;
  state[3] = t4;
  asm volatile("" ::: "memory");
}


// MixColumns
__attribute__ ((always_inline))
static void MixColumns(uint8_t * state)
{
  uint8_t t0, t1, t2, t3;
  uint8_t tmp1, tmp2, tmp3;
  bsg_unroll(1)
  for (int i = 0; i < 4; i++) {
    t0 = state[(4*i)+0];
    t1 = state[(4*i)+1];
    t2 = state[(4*i)+2];
    t3 = state[(4*i)+3];
    asm volatile("" ::: "memory");
    tmp1 = t0 ^ t1 ^ t2 ^ t3;
    tmp3 = t0 ^ t1;
    tmp2 = xtime(tmp3);
    state[(4*i)+0] = t0 ^ tmp1 ^ tmp2;
    tmp3 = t1 ^ t2;
    tmp2 = xtime(tmp3);
    state[(4*i)+1] = t1 ^ tmp1 ^ tmp2;
    tmp3 = t2 ^ t3;
    tmp2 = xtime(tmp3);
    state[(4*i)+2] = t2 ^ tmp1 ^ tmp2;
    tmp3 = t3 ^ t0;
    tmp2 = xtime(tmp3);
    state[(4*i)+3] = t3 ^ tmp1 ^ tmp2;
    asm volatile("" ::: "memory");
  }
}


void Cipher(state_t* state)
{
  uint8_t round = 0;

  // Add the First round key to the state before starting the rounds.
  AddRoundKey(0, state);

  for (round = 1; ; ++round)
  {
    SubBytes((uint8_t *) state);
    ShiftRows((uint8_t *) state);
    if (round == Nr) {
      break;
    }
    MixColumns((uint8_t *) state);
    AddRoundKey(round, state);
  }

  // last round;
  AddRoundKey(Nr, state);

}



void AES_CBC_encrypt_buffer(struct AES_ctx *ctx, uint8_t * buf, size_t length)
{
  uint32_t *dbuf = (uint32_t *) &buf[0];
  uint32_t *pbuf = &localbuf0[0];
  uint32_t *pIv  = &localbuf1[0];
  uint32_t *temp;


  // Load roundkey;
  uint32_t *dramRoundKey = (uint32_t *) &ctx->RoundKey[0];
  bsg_unroll(1)
  for (int i = 0; i < AES_keyExpSize/sizeof(uint32_t); i+=8) {
    uint32_t r0 = dramRoundKey[i+0];
    uint32_t r1 = dramRoundKey[i+1];
    uint32_t r2 = dramRoundKey[i+2];
    uint32_t r3 = dramRoundKey[i+3];
    uint32_t r4 = dramRoundKey[i+4];
    uint32_t r5 = dramRoundKey[i+5];
    uint32_t r6 = dramRoundKey[i+6];
    uint32_t r7 = dramRoundKey[i+7];
    asm volatile("" ::: "memory");
    localRoundKey[i+0] = r0;
    localRoundKey[i+1] = r1;
    localRoundKey[i+2] = r2;
    localRoundKey[i+3] = r3;
    localRoundKey[i+4] = r4;
    localRoundKey[i+5] = r5;
    localRoundKey[i+6] = r6;
    localRoundKey[i+7] = r7;
  }

  // load Iv;
  uint32_t *dramIv = (uint32_t *) &ctx->Iv[0];
  uint32_t Iv0 = dramIv[0];
  uint32_t Iv1 = dramIv[1];
  uint32_t Iv2 = dramIv[2];
  uint32_t Iv3 = dramIv[3];
  asm volatile("" ::: "memory");
  pIv[0] = Iv0;
  pIv[1] = Iv1;
  pIv[2] = Iv2;
  pIv[3] = Iv3;
  asm volatile("" ::: "memory");

  // Load the first block in regfile;
  register uint32_t w0 = dbuf[0];
  register uint32_t w1 = dbuf[1];
  register uint32_t w2 = dbuf[2];
  register uint32_t w3 = dbuf[3];
  asm volatile("" ::: "memory");
  pbuf[0] = w0;
  pbuf[1] = w1;
  pbuf[2] = w2;
  pbuf[3] = w3;
  asm volatile("" ::: "memory");


  // FP registers for pre-loading;
  register float f0;
  register float f1;
  register float f2;
  register float f3;


  bsg_unroll(1)
  for (size_t i = 0; i < length; i += AES_BLOCKLEN)
  {
    // load and XorWithIv
    w0 = pbuf[0];
    w1 = pbuf[1];
    w2 = pbuf[2];
    w3 = pbuf[3];
    Iv0 = pIv[0];
    Iv1 = pIv[1];
    Iv2 = pIv[2];
    Iv3 = pIv[3];
    asm volatile("" ::: "memory");
    pbuf[0] = w0 ^ Iv0;
    pbuf[1] = w1 ^ Iv1;
    pbuf[2] = w2 ^ Iv2;
    pbuf[3] = w3 ^ Iv3;
    asm volatile("" ::: "memory");

    // Pre-load the next blk in FP regfile;
    if (i != length - AES_BLOCKLEN) {
      asm volatile ("flw %[rd], %[p]" : [rd] "=f" (f0) : [p] "m" (dbuf[4]));
      asm volatile ("flw %[rd], %[p]" : [rd] "=f" (f1) : [p] "m" (dbuf[5]));
      asm volatile ("flw %[rd], %[p]" : [rd] "=f" (f2) : [p] "m" (dbuf[6]));
      asm volatile ("flw %[rd], %[p]" : [rd] "=f" (f3) : [p] "m" (dbuf[7]));
      asm volatile("" ::: "memory");
    }

    // cipher
    Cipher((state_t*)pbuf);

    // copy encryped result to dram;
    dbuf[0] = pbuf[0];
    dbuf[1] = pbuf[1];
    dbuf[2] = pbuf[2];
    dbuf[3] = pbuf[3];
    asm volatile("" ::: "memory");

    // write the pre-loaded blk;
    if (i != length - AES_BLOCKLEN) {
      asm volatile ("fsw %[rs], %[p]" :: [rs] "f" (f0),  [p] "m" (pIv[0]));
      asm volatile ("fsw %[rs], %[p]" :: [rs] "f" (f1),  [p] "m" (pIv[1]));
      asm volatile ("fsw %[rs], %[p]" :: [rs] "f" (f2),  [p] "m" (pIv[2]));
      asm volatile ("fsw %[rs], %[p]" :: [rs] "f" (f3),  [p] "m" (pIv[3]));
      asm volatile("" ::: "memory");
    }

    // swap pointers;
    temp = pIv;
    pIv = pbuf;
    pbuf = temp;

    // Incremented block;
    dbuf += AES_BLOCKLEN/sizeof(uint32_t);
  }
  
  // Store IB in ctx for next call;
  Iv0 = pIv[0];
  Iv1 = pIv[1];
  Iv2 = pIv[2];
  Iv3 = pIv[3];
  asm volatile("" ::: "memory");
  dramIv[0] = Iv0;
  dramIv[1] = Iv1;
  dramIv[2] = Iv2;
  dramIv[3] = Iv3;
  asm volatile("" ::: "memory");
}
