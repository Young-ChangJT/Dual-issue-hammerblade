#include <bsg_manycore.h>
#include <stdint.h>
#include <stddef.h>

// AES128
#define AES_BLOCKLEN 16
#define Nb 4
#define Nk 4
#define Nr 10
#define xtime(x) ((x<<1) ^ (((x>>7) ) * 0x1b))
#define AES_KEYLEN 16
#define AES_keyExpSize 176
typedef uint8_t state_t[4][4];

struct AES_ctx
{
  uint8_t RoundKey[AES_keyExpSize];
  uint8_t Iv[AES_BLOCKLEN];
};


void AES_CBC_encrypt_buffer(struct AES_ctx *ctx, uint8_t * buf, size_t length);
